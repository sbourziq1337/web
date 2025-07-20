#include "server.hpp"

// Handle authentication logic and determine target path
std::string handle_authentication(const std::string &path, const std::string &username, 
                                const std::string &password, std::map<std::string, std::string> &post_res)
{
    std::string target_path = path;
    
    // Handle directory paths by adding index.html
    if (is_directory(path))
    {
        target_path = path;
        if (target_path[target_path.length() - 1] != '/')
            target_path += '/';
        target_path += "index.html";
    }

    // Check for login page
    if (target_path.find("login/index.html") != std::string::npos)
    {
        if (post_res.count(username) && post_res[username] == password)
        {
            // Successful login - redirect to upload page
            size_t pos = target_path.find("login/index.html");
            target_path = target_path.substr(0, pos) + "login/upload.html";
        }
        else
        {
            // Failed login
            size_t pos = target_path.find("login/index.html");
            target_path = target_path.substr(0, pos) + "login/login_failed.html";
        }
    }
    // Check for signup page
    else if (target_path.find("singup/index.html") != std::string::npos)
    {
        // Register new user
        post_res[username] = password;
    }
    // Handle direct login.html file
    else if (target_path.find("login.html") != std::string::npos)
    {
        if (post_res.count(username) && post_res[username] == password)
        {
            size_t pos = target_path.find("login.html");
            target_path = target_path.substr(0, pos) + "login/upload.html";
        }
        else
        {
            size_t pos = target_path.find("login.html");
            target_path = target_path.substr(0, pos) + "login/login_failed.html";
        }
    }

    return target_path;
}

// Resolve POST path using saved post_path from Request object
std::string resolve_post_path(const Request &obj, const std::string &original_path)
{
    if (obj.post_path.empty())
        return original_path;

    // If original path exists, use it
    if (is_file(original_path) || is_directory(original_path))
        return original_path;

    // Try to resolve using saved post_path
    size_t root_pos = original_path.find(obj.root);
    if (root_pos != std::string::npos)
    {
        std::string relative_path = original_path.substr(root_pos + obj.root.length());
        if (!relative_path.empty() && relative_path[0] == '/')
            relative_path = relative_path.substr(1);

        std::string resolved_path = obj.post_path + relative_path;
        
        if (is_file(resolved_path) || is_directory(resolved_path))
            return resolved_path;
    }

    return original_path; // Return original if resolution fails
}

// Main POST form handler with path resolution
std::string Format_urlencoded(std::string path, std::map<std::string, std::string> &post_res, 
                      std::map<std::string, std::string> &form_data, Request &obj)
{
    std::string username = form_data["username"];
    std::string password = form_data["password"];

    
    // First try to resolve the path using saved post_path
    std::string resolved_path = resolve_post_path(obj, path);
    
    // Handle authentication logic
    std::string target_path = handle_authentication(resolved_path, username, password, post_res);
    
    
    // Serve the target file
    if (is_file(target_path))
        return target_path; 
    else
    {
        std::string header = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n";
        
        // Try to use the resolved not_found.html path
        std::string not_found_path = obj.post_path.empty() ? 
            "error_page/404.html" : 
            obj.post_path + "error_page/404.html";
            
        return not_found_path; // Return the path to not_found.html
    }
}