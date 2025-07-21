#include "server.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>
#include <functional>

// CGI timeout in seconds
#define CGI_TIMEOUT 10

bool is_cgi_request(const std::string &path)
{
    return path.find(".cgi") != std::string::npos ||
           path.find(".py") != std::string::npos ||
           path.find(".php") != std::string::npos;
}

std::string normalize_cgi_path(const std::string &path)
{
    std::string normalized = path;
    while (!normalized.empty() && normalized[0] == '/')
    {
        normalized = normalized.substr(1);
    }
    return normalized;
}

std::string int_to_string(int n)
{
    std::ostringstream ss;
    ss << n;
    return ss.str();
}

// Helper function to check if process is still running
bool is_process_running(pid_t pid)
{
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    return (result == 0); // 0 means process is still running
}

// Helper function to get elapsed time in seconds using time_t
long get_elapsed_seconds(const time_t &start_time)
{
    time_t current_time = time(NULL);
    return (long)(current_time - start_time);
}

// Simple read with timeout check
int read_with_timeout_select(int fd, char *buffer, size_t buffer_size, int timeout_sec,
                             const time_t &start_time, std::string &accumulated_output)
{
    long elapsed = get_elapsed_seconds(start_time);
    if (elapsed >= timeout_sec)
    {
        return -1; // Timeout
    }

    // Try a non-blocking read
    ssize_t bytes_read = read(fd, buffer, buffer_size - 1);

    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        accumulated_output += std::string(buffer, bytes_read);
        return bytes_read;
    }
    else if (bytes_read == 0)
    {
        return 0; // EOF
    }
    else if (bytes_read == -1)
    {
        // For non-blocking I/O, assume it's just no data available
        return -2; // Try again
    }

    return -1; // Should not reach here
}

// Alternative implementation using poll

void handle_cgi_request(ChunkedClientInfo &client, int new_socket, std::map<std::string, std::string> &headers)
{
    // Set server_config after we know it's valid
    static int number = 0;
    number++;
    std::cout << "======= ============== >> " << number << std::endl ;
    client.request_obj.server_config = &client.request_obj.server;

    std::string script_path = client.request_obj.path;

    if (access(client.request_obj.cgj_path.c_str(), X_OK) != 0)
    {
        std::cout << "Warning: Script is not executable: " << client.request_obj.cgj_path << std::endl;
    }

    // Prepare environment variables
    std::vector<std::string> env_strings;
    env_strings.push_back("REQUEST_METHOD=" + client.request_obj.mthod);
    env_strings.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env_strings.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_strings.push_back("REQUEST_URI=" + client.request_obj.uri);
    env_strings.push_back("PATH_INFO=" + client.request_obj.uri);
    env_strings.push_back("SCRIPT_NAME=" + script_path);

    // Add server information
    if (client.request_obj.server_config)
    {
        env_strings.push_back("SERVER_NAME=" + client.request_obj.server_config->server_name);
        env_strings.push_back("SERVER_PORT=" + int_to_string(client.request_obj.server_config->port));
    }

    if (client.request_obj.mthod == "POST")
    {
        env_strings.push_back("CONTENT_LENGTH=" + int_to_string(client.request_obj.info_body.length()));
        if (headers.find("Content-Type") != headers.end())
            env_strings.push_back("CONTENT_TYPE=" + headers["Content-Type"]);
        else
            env_strings.push_back("CONTENT_TYPE=application/x-www-form-urlencoded");
    }

    // Convert to char* array for execve
    std::vector<char *> envp;
    for (size_t i = 0; i < env_strings.size(); ++i)
        envp.push_back(const_cast<char *>(env_strings[i].c_str()));
    envp.push_back(NULL);

    // Create pipes
    int pipefd[2], stdin_pipe[2];
    if (pipe(pipefd) == -1 || pipe(stdin_pipe) == -1)
    {
        perror("pipe failed");
        std::string error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<h1>500 Internal Server Error</h1>";
        send(new_socket, error_response.c_str(), error_response.length(), 0);
        return;
    }

    // Fork process
    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        close(pipefd[0]);
        close(stdin_pipe[1]);

        // Redirect stdout and stderr to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        dup2(stdin_pipe[0], STDIN_FILENO);

        close(pipefd[1]);
        close(stdin_pipe[0]);

        // Change to script directory
        std::string script_dir = script_path.substr(0, script_path.find_last_of('/'));
        if (chdir(script_dir.c_str()) != 0)
        {
            perror("Failed to change directory");
            exit(1);
        }

        // Prepare arguments
        std::string actual_script = script_path.substr(script_path.find_last_of('/') + 1);
        char *args[3];
        args[0] = const_cast<char *>(client.request_obj.cgj_path.c_str());
        args[1] = const_cast<char *>(actual_script.c_str());
        args[2] = NULL;

        // Execute CGI script
        execve(client.request_obj.cgj_path.c_str(), args, &envp[0]);
        perror("execve failed");
        exit(1);
    }
    else if (pid > 0)
    {
        // Parent process
        close(pipefd[1]);
        close(stdin_pipe[0]);

        struct epoll_event ev;
        ev.events = EPOLLIN;
        // Handle POST data
        if (client.request_obj.mthod == "POST")
        {
            // Set stdin pipe to non-blocking mode]
            int flags = fcntl(stdin_pipe[1], F_GETFL, 0);
            fcntl(stdin_pipe[1], F_SETFL, flags | O_NONBLOCK);
            epoll_ctl(client.request_obj.epfd, EPOLL_CTL_ADD, flags, &ev);

            // Write CGI headers if available
            if (!client.cgi_headrs.empty())
            {
                ssize_t written = write(stdin_pipe[1], client.cgi_headrs.c_str(), client.cgi_headrs.size());
                if (written == -1)
                {
                    perror("Failed to write CGI headers");
                }
            }

            // Write body data
            if (!client.request_obj.info_body.empty())
            {
                ssize_t written = write(stdin_pipe[1], client.request_obj.info_body.c_str(), client.request_obj.info_body.length());
                if (written == -1)
                {
                    perror("Failed to write body data");
                }
            }
            else if (!client.filename.empty())
            {
                // Fallback: read from file if info_body is empty
                std::ifstream cgi_body(client.filename.c_str(), std::ios::binary);
                if (cgi_body.is_open())
                {
                    cgi_body.seekg(0, std::ios::end);
                    std::streamsize file_size = cgi_body.tellg();
                    cgi_body.seekg(0, std::ios::beg);

                    if (file_size > 0)
                    {

                        std::string file_content(file_size, '\0');
                        cgi_body.read(&file_content[0], file_size);
                        ssize_t written = write(stdin_pipe[1], file_content.c_str(), file_size);
                        if (written == -1)
                        {
                            perror("Failed to write file content");
                        }
                    }
                    cgi_body.close();
                }
                unlink(client.filename.c_str());
            }
        }
        close(stdin_pipe[1]); // Close stdin to signal EOF

        // Set pipe to non-blocking mode
        fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
        epoll_ctl(client.request_obj.epfd, EPOLL_CTL_ADD, pipefd[0], &ev);

        // Read CGI output with timeout - NO WHILE LOOPS
        std::string cgi_output;
        char buffer[4096];
        time_t start_time;
        start_time = time(NULL);

        // Use recursive approach or state machine instead of while loop
        // Classic non-blocking loop for C++98 compatibility
        bool finished = false;
        bool timeout_occurred = false;
        while (!finished && !timeout_occurred)
        {
            int read_result = read_with_timeout_select(pipefd[0], buffer, sizeof(buffer), CGI_TIMEOUT, start_time, cgi_output);
            if (read_result > 0)
            {
                // Data read, continue
            }
            else if (read_result == 0)
            {
                // EOF
                finished = true;
            }
            else if (read_result == -2)
            {
                // No data available, check timeout and process status
                if (get_elapsed_seconds(start_time) >= CGI_TIMEOUT)
                {
                    timeout_occurred = true;
                }
                else if (!is_process_running(pid))
                {
                    // Process finished, try one more read then finish
                    usleep(100000); // 100ms delay
                    read_result = read_with_timeout_select(pipefd[0], buffer, sizeof(buffer), CGI_TIMEOUT, start_time, cgi_output);
                    if (read_result <= 0)
                    {
                        finished = true;
                    }
                }
                else
                {
                    // Just wait a bit
                    usleep(50000); // 50ms
                }
            }
            else
            {
                // Error or timeout
                timeout_occurred = true;
            }
        }
        close(pipefd[0]);
        // Handle timeout
        if (timeout_occurred)
        {
            std::cerr << "CGI script timeout after " << CGI_TIMEOUT << " seconds" << std::endl;
            kill(pid, SIGKILL);
            int status;
            waitpid(pid, &status, 0);
            std::string error_response = "HTTP/1.1 504 Gateway Timeout\r\nContent-Type: text/html\r\n\r\n<h1>504 Gateway Timeout</h1><p>CGI script exceeded " + int_to_string(CGI_TIMEOUT) + " second timeout</p>";
            send(new_socket, error_response.c_str(), error_response.length(), 0);
            return;
        }

        // Wait for process to finish if it hasn't already
        if (is_process_running(pid))
        {
            int status;
            waitpid(pid, &status, 0);
        }

        // Process CGI output
        std::string response;
        size_t header_end = cgi_output.find("\r\n\r\n");
        bool has_proper_headers = false;

        if (header_end == std::string::npos)
        {
            header_end = cgi_output.find("\n\n");
            if (header_end != std::string::npos)
            {
                std::string headers_part = cgi_output.substr(0, header_end);
                std::string body_part = cgi_output.substr(header_end + 2);

                // Check if headers contain Content-Type
                if (headers_part.find("Content-Type") != std::string::npos ||
                    headers_part.find("content-type") != std::string::npos)
                {
                    has_proper_headers = true;
                    // Normalize line endings
                    for (size_t i = 0; i < headers_part.length(); ++i)
                    {
                        if (headers_part[i] == '\n' && (i == 0 || headers_part[i - 1] != '\r'))
                        {
                            headers_part.insert(i, "\r");
                            ++i;
                        }
                    }
                    response = "HTTP/1.1 200 OK\r\n" + headers_part + "\r\n\r\n" + body_part;
                }
            }
        }
        else
        {
            std::string headers_part = cgi_output.substr(0, header_end);
            if (headers_part.find("Content-Type") != std::string::npos ||
                headers_part.find("content-type") != std::string::npos)
            {
                has_proper_headers = true;
                response = "HTTP/1.1 200 OK\r\n" + cgi_output;
            }
        }

        // If no proper headers found, treat entire output as body content
        if (!has_proper_headers || response.empty())
        {
            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: text/html\r\n\r\n";
            response += cgi_output;
        }

        send(new_socket, response.c_str(), response.length(), 0);
    }
    else
    {
        // Fork failed
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        std::string error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<h1>500 Internal Server Error</h1>";
        send(new_socket, error_response.c_str(), error_response.length(), 0);
    }
}