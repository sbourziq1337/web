#include "server.hpp"
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <fstream>

void all_type(std::map<std::string, std::string> &mimetype)
{
    std::ifstream filetype("type.txt");
    if (!filetype.is_open())
    {
        std::cerr << "Error: Could not open type.txt" << std::endl; // Debug output
        return;
    }

    std::string line;
    while (std::getline(filetype, line))
    {
        size_t pos = line.find(":");
        if (pos != std::string::npos)
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            mimetype[key] = value; // Use at() for better error handling
        }
    }
    filetype.close();
}

std::string generate_directory_listing(const std::string &path, const std::string &uri)
{
    DIR *dir = opendir(path.c_str());
    if (!dir)
        return "<html><body><h1>Unable to open directory</h1></body></html>";

    std::ostringstream html;
    html << "<html><head><title>Index of " << uri << "</title></head><body>";
    html << "<h1>Index of " << uri << "</h1><ul>";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name(entry->d_name);
        if (name == "." || name == "..")
            continue;

        std::string display = name;
        std::string link = uri;
        if (link.empty() || link[link.size() - 1] != '/')
            link += '/';
        link += name;

        std::string fullPath = path;
        if (fullPath.empty() || fullPath[fullPath.size() - 1] != '/')
            fullPath += '/';
        fullPath += name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {
            display += '/';
            link += '/';
        }
        html << "<li><a href=\"" << link << "\">" << display << "</a></li>";
    }

    html << "</ul></body></html>";
    closedir(dir);
    return html.str();
}

std::string normalize_path(const std::string &path)
{
    if (path.length() > 1 && path[path.length() - 1] == '/')
        return path.substr(0, path.length() - 1);
    return path;
}

std::string remove_last_path_component(std::string path)
{
    path = normalize_path(path);
    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos)
        return "";
    if (last_slash == 0)
        return "/";
    return path.substr(0, last_slash);
}

int find_path(const std::string &uri, const Request &obj)
{
    std::string normalized_uri = normalize_path(uri);
    int authorized = -1;
    for (size_t i = 0; i < obj.local_data.size(); i++)
    {
        std::string location_path = normalize_path(obj.local_data[i].path);
        if (normalized_uri == location_path)
            authorized = obj.local_data[i].autoindex;
    }
    return authorized;
}

bool find_location_autoindex(const std::string &uri, const Request &obj)
{
    std::string path = uri;
    int check = find_path(path, obj);
    if (check != -1)
        return check;
    while (true)
    {
        path = remove_last_path_component(path);

        check = find_path(path, obj);
        if (check != -1)
            return check;
        if (path.empty() || path == obj.root)
            break;
    }
    return false; // Default to false if no location found
}

void handle_directory_request(const std::string &path, const std::string &uri, int &fd, Request &obj, const std::string &type)
{

    std::string index_path = uri;
    bool found_index = false;
    bool check_in_directory = false;
    while (true)
    {
        for (size_t i = 0; i < obj.local_data.size(); i++)
        {
            std::string location_path = normalize_path(obj.local_data[i].path);
            index_path = normalize_path(index_path);
            if (index_path == location_path)
            {
                check_in_directory = true;
                for (size_t j = 0; j < obj.local_data[i].index.size(); j++)
                {
                    std::string index_file = path + "/" + obj.local_data[i].index[j];
                    if (is_file(index_file))
                    {
                        found_index = true;
                        index_path = index_file;
                    }
                }
                if (found_index)
                    break;
            }
        }
        if (index_path.empty() || index_path == "/" || index_path == obj.root || found_index == true || check_in_directory == true)
            break;
        index_path = remove_last_path_component(index_path);
    }
    if (is_file(index_path))
    {
        std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + getContentType(index_path) + "\r\n";
        response(index_path, fd, header);
        return;
    }

    bool autoindex_enabled = find_location_autoindex(uri, obj);

    if (autoindex_enabled == false) // More idiomatic than == false
    {
        sendErrorResponse(fd, 403, "Forbidden", obj.server.error_pages[403]);
        return;
    }

    std::string html = generate_directory_listing(path, uri);
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    std::string full_response = header + html;

    // Add error checking for write operation
    ssize_t bytes_written = write(fd, full_response.c_str(), full_response.length());
    if (bytes_written == -1)
    {
        std::cerr << "Error writing response to socket" << std::endl;
    }
}

std::string remove_first_slash(const std::string &path)
{
    if (path.empty() || path[0] != '/')
        return path;
    for (size_t i = 1; i < path.length(); ++i) // Use size_t for consistency
    {
        if (path[i] != '/')
            return path.substr(i);
    }
    return ""; // If the path is all slashes, return empty string
}

bool initialize_and_serve_direct_path(Request &obj, const std::string &path,
                                      const std::string &type, const std::string &uri, int &fd)
{
    if (is_file(path))
    {
        std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + type + "\r\n";
        response(path, fd, header);
        return true;
    }

    if (is_directory(path))
    {
        handle_directory_request(path, uri, fd, obj, type);
        return true;
    }
    return false;
}

void serve_not_found(int &fd)
{
    std::string header = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n";
    response("error_page/404.html", fd, header);
}

void parsing_Get(std::map<std::string, std::string> head, std::string path,
                 int &fd, std::string type, std::string uri, Request &obj)
{
    if (initialize_and_serve_direct_path(obj, path, type, uri, fd))
        return;

    serve_not_found(fd);
}