#include "server.hpp"
#include <sys/stat.h>
#include <iomanip>
#include <algorithm>
#include <fcntl.h>
#include <errno.h>
#include <cstdlib>  // For atol
#include <unistd.h> // For usleep

long getFileSize(const std::string &filename)
{
    struct stat s;
    if (stat(filename.c_str(), &s) == 0)
        return (s.st_size);
    return -1;
}

// Parse Range header for partial content requests
bool parseRangeHeader(const std::string &range, long fileSize, long &start, long &end)
{
    // Expected format: "bytes=Start-End" or "bytes=Start-"
    if (range.compare(0, 6, "bytes=") != 0)
        return false;

    std::string byteRange = range.substr(6);
    size_t dashPos = byteRange.find('-');
    if (dashPos == std::string::npos)
        return false;

    std::string startStr = byteRange.substr(0, dashPos);
    std::string endStr = byteRange.substr(dashPos + 1);

    try
    {
        start = atol(startStr.c_str());
        if (endStr.empty())
        {
            end = fileSize - 1;
        }
        else
        {
            end = atol(endStr.c_str());
        }
    }
    catch (...)
    {
        return false;
    }

    if (start > end || end >= fileSize)
        return false;

    return true;
}

// Enhanced response function with better concurrent handling

bool hasEnding(const std::string &fullString, const std::string &ending)
{
    if (fullString.length() >= ending.length())
    {
        return fullString.compare(fullString.length() - ending.length(), ending.length(), ending) == 0;
    }
    return false;
}

std::string getContentType(const std::string &filename)
{
    if (hasEnding(filename, ".html"))
        return "text/html";
    if (hasEnding(filename, ".css"))
        return "text/css";
    if (hasEnding(filename, ".js"))
        return "application/javascript";
    if (hasEnding(filename, ".png"))
        return "image/png";
    if (hasEnding(filename, ".webp"))
        return "image/webp";
    if (hasEnding(filename, ".jpg") || hasEnding(filename, ".jpeg"))
        return "image/jpeg";
    if (hasEnding(filename, ".gif"))
        return "image/gif";
    if (hasEnding(filename, ".svg"))
        return "image/svg+xml";
    if (hasEnding(filename, ".ico"))
        return "image/x-icon";
    if (hasEnding(filename, ".txt"))
        return "text/plain";
    if (hasEnding(filename, ".json"))
        return "application/json";
    if (hasEnding(filename, ".xml"))
        return "application/xml";
    // Video file types
    if (hasEnding(filename, ".mp4"))
        return "video/mp4";
    if (hasEnding(filename, ".webm"))
        return "video/webm";
    if (hasEnding(filename, ".avi"))
        return "video/avi";
    if (hasEnding(filename, ".mov"))
        return "video/quicktime";
    if (hasEnding(filename, ".mkv"))
        return "video/x-matroska";
    return "application/octet-stream";
}

bool is_file(const std::string &path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return S_ISREG(s.st_mode);
    return false;
}

bool is_directory(const std::string &path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return S_ISDIR(s.st_mode);
    return false;
}

std::string urlDecode(const std::string &str)
{
    std::ostringstream decoded;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.length() &&
            std::isxdigit(str[i + 1]) && std::isxdigit(str[i + 2]))
        {
            std::string hex = str.substr(i + 1, 2);
            int value;
            std::istringstream iss(hex);
            iss >> std::hex >> value;
            char ch = static_cast<char>(value);

            decoded << ch;
            i += 2;
        }
        else if (str[i] == '+')
            decoded << ' ';
        else
            decoded << str[i];
    }
    return decoded.str();
}

std::string remove_slash(std::string path)
{
    size_t pos;
    // Remove consecutive slashes
    while ((pos = path.find("//")) != std::string::npos)
    {
        path.replace(pos, 2, "/");
    }

    if (path.empty())
    {
        return "/";
    }

    // Ensure path starts with /
    if (path[0] != '/')
    {
        return "/" + path;
    }

    return path;
}

void parsing_method(Request &rec, const std::string &line)
{
    std::istringstream input(line);
    std::string filename;
    input >> rec.mthod >> filename >> rec.version;

    // URL decode and normalize the filename
    filename = urlDecode(filename);
    filename = remove_slash(filename);

    // Check if the redirection is needed
    rec.found_redirection = false;
    std::string red_path = filename;
    while (true)
    {
        for (size_t i = 0; i < rec.local_data.size(); i++)
        {
            std::string location_path = normalize_path(rec.local_data[i].path);
            red_path = normalize_path(red_path);
            if (red_path == location_path)
            {
                if (rec.local_data[i].redirection.empty())
                {
                    rec.found_redirection = true;
                }
            }
        }
        red_path = remove_last_path_component(red_path);
        if (rec.found_redirection == true || red_path.empty() || red_path == "/" || red_path == rec.root)
            break;
    }
    if (rec.found_redirection == true)
    {
        // Set the path using root
        bool found_location_root = false;
        std::string index_path = filename;
        // handling location root
        while (true)
        {
            for (size_t i = 0; i < rec.local_data.size(); i++)
            {
                std::string location_path = normalize_path(rec.local_data[i].path);
                index_path = normalize_path(index_path);
                if (index_path == location_path)
                {
                    if (rec.local_data[i].root.empty())
                        found_location_root = true;
                    else
                        rec.root = rec.local_data[i].root;
                    found_location_root = true;
                }
            }
            index_path = remove_last_path_component(index_path);
            if (found_location_root == true || index_path.empty() || index_path == "/" || index_path == rec.root)
                break;
        }
    }

    rec.path = rec.root + filename;
    rec.path = remove_first_slash(rec.path);
    if (rec.path.empty() || rec.path == "/")
        rec.path = "../home";

    rec.uri = filename;
}

// Enhanced GET request handling with better concurrent support
void parsing_Get(std::map<std::string, std::string> &headers,
                 std::string path, int fd, std::string content_type,
                 std::string uri, ChunkedClientInfo &client)
{
    // Check if file exists
    if (!is_file(path))
    {
        sendErrorResponse(fd, 404, "Not Found", client.request_obj.server.error_pages[404]);
        return;
    }

    // Send file with appropriate content type and Range support
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + content_type + "\r\n";
    response_plus(path, fd, header, headers);
}

// Enhanced error response function
void sendErrorResponse(int fd, int error_code, const std::string &error_message, std::string path_file)
{
    std::ostringstream response;
    response << "HTTP/1.1 " << error_code << " " << error_message << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Connection: close\r\n\r\n";
    std::string resp_str = response.str();
    send(fd, resp_str.c_str(), resp_str.length(), MSG_NOSIGNAL);
    // read file
    if (path_file.empty())
    {
        path_file = "error_page/404.html";
    }
    std::ifstream file(path_file.c_str(), std::ios::binary);
    char buffer[BUFFER_SIZE];
    if (file.is_open())
    {
        while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0)
        {
            std::streamsize bytesRead = file.gcount();
            // Send the buffer to the client
            send(fd, &buffer[0], bytesRead, MSG_NOSIGNAL);
        }
        file.close();
    }
}