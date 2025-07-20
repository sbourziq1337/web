#include "server.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <cstring> // For strerror

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

void handle_cgi_request(ChunkedClientInfo &client, int new_socket, std::map<std::string, std::string> &headers)
{
    // if (client.request_obj.server_configs.empty())
    // {
    //     std::cout << ""
    //     std::cerr << "Error: Invalid server configuration - struct_data is empty" << std::endl;
    //     std::string error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<h1>500 Internal Server Error</h1>";
    //     send(new_socket, error_response.c_str(), error_response.length(), 0);
    //     return;
    // }

    // Set server_config after we know struct_data is not empty
    client.request_obj.server_config = &client.request_obj.server;

    std::string script_path = client.request_obj.path;

    if (access(client.request_obj.cgj_path.c_str(), X_OK) != 0)
    {
        std::cout << "Warning: Script is not executable: " << client.request_obj.cgj_path << std::endl;
    }

    std::vector<std::string> env_strings;
    env_strings.push_back("REQUEST_METHOD=" + client.request_obj.mthod);
    env_strings.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env_strings.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_strings.push_back("REQUEST_URI=" + client.request_obj.uri);
    env_strings.push_back("PATH_INFO=" + client.request_obj.uri);       // Use full path as PATH_INFO
    env_strings.push_back("SCRIPT_NAME=" + script_path); // Add SCRIPT_NAME for CGI compatibility

    // Add server information
    if (client.request_obj.server_config)
    {
        env_strings.push_back("SERVER_NAME=" + client.request_obj.server_config->server_name);
        env_strings.push_back("SERVER_PORT=" + int_to_string(client.request_obj.server_config->port));
    }

    if (client.request_obj.mthod == "POST" && !client.request_obj.info_body.empty())
    {
        env_strings.push_back("CONTENT_LENGTH=" + int_to_string(client.request_obj.info_body.length()));
        if (headers.find("Content-Type") != headers.end())
            env_strings.push_back("CONTENT_TYPE=" + headers["Content-Type"]);
        else
            env_strings.push_back("CONTENT_TYPE=application/x-www-form-urlencoded");
    }
    std::vector<char *> envp;
    for (size_t i = 0; i < env_strings.size(); ++i)
        envp.push_back(const_cast<char *>(env_strings[i].c_str()));
    envp.push_back(NULL);

    int pipefd[2], stdin_pipe[2];
    if (pipe(pipefd) == -1 || pipe(stdin_pipe) == -1)
    {
        perror("pipe failed");
        std::string error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<h1>500 Internal Server Error</h1>";
        send(new_socket, error_response.c_str(), error_response.length(), 0);
        return;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);
        close(stdin_pipe[1]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);

        // int fd = std::ifstream(client.filename);
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(pipefd[1]);
        close(stdin_pipe[0]);
        std::string script_dir = script_path.substr(0, script_path.find_last_of('/'));
        // std::cout << "Debug - script_path: " << script_dir << std::endl;

        if (chdir(script_dir.c_str()) != 0)
        {
            perror("Failed to change directory");
            exit(1);
        }

        // Fix: Use the actual script path, not duplicated path
        std::string actual_script = script_path.substr(script_path.find_last_of('/') + 1);

        char *args[3];
        args[0] = const_cast<char *>(client.request_obj.cgj_path.c_str());
        args[1] = const_cast<char *>(actual_script.c_str()); // Use just filename after chdir
        args[2] = NULL;

        execve(client.request_obj.cgj_path.c_str(), args, &envp[0]);
        perror("execve failed");
        exit(1);
    }
    else if (pid > 0)
    {
        close(pipefd[1]);
        close(stdin_pipe[0]);

        // For chunked requests, the server should have already unchunked the data
        // before calling this CGI handler. The CGI expects EOF as end of body.
        if (client.request_obj.mthod == "POST" && !client.request_obj.info_body.empty())
        {
            std::cout << "body == " << client.request_obj.info_body << std::endl;
            write(stdin_pipe[1], client.request_obj.info_body.c_str(), client.request_obj.info_body.length());
        }
        close(stdin_pipe[1]); // Close stdin to signal EOF to CGI

        std::string cgi_output;
        char buffer[CHUNK_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0';
            cgi_output.append(buffer, bytes_read);
        }
        close(pipefd[0]);
        int status;
        waitpid(pid, &status, 0);

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
        // Don't add Content-Length if CGI didn't provide it - EOF marks the end
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
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        std::string error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<h1>500 Internal Server Error</h1>";
        send(new_socket, error_response.c_str(), error_response.length(), 0);
    }
}