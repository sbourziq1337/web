#include <string>
#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <map>
#include <set>
#include <climits>

#include "server.hpp"

// Helper function to remove trailing semicolon
std::string removeSemicolon(const std::string &str)
{
    std::string result = str;
    if (!result.empty() && result[result.length() - 1] == ';')
        result.erase(result.length() - 1);
    return result;
}

// Helper function to check if string ends with semicolon
bool endsWithSemicolon(const std::string &str)
{
    return !str.empty() && str[str.length() - 1] == ';';
}

std::vector<ServerConfig> check_configfile()
{
    std::ifstream inputFile("configfile.conf");
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open file configfile.conf" << std::endl;
        return std::vector<ServerConfig>(); // Return empty vector on error
    }

    // Define allowed directives
    std::set<std::string> allowedServerDirectives;
    allowedServerDirectives.insert("listen");
    allowedServerDirectives.insert("host");
    allowedServerDirectives.insert("root");
    allowedServerDirectives.insert("server_name");
    allowedServerDirectives.insert("index");
    allowedServerDirectives.insert("error_page");
    allowedServerDirectives.insert("client_max_body_size");
    allowedServerDirectives.insert("location");

    std::set<std::string> allowedLocationDirectives;
    allowedLocationDirectives.insert("method");
    allowedLocationDirectives.insert("autoindex");
    allowedLocationDirectives.insert("root");
    allowedLocationDirectives.insert("index");
    allowedLocationDirectives.insert("upload_path");
    allowedLocationDirectives.insert("cgi_path");
    allowedLocationDirectives.insert("redirection");

    // Directives that require special treatment for semicolon checking
    std::set<std::string> specialDirectives;
    specialDirectives.insert("location");
    specialDirectives.insert("server");

    std::string line;
    std::vector<ServerConfig> servers;
    ServerConfig currentServer;
    LocationConfig *currentLocation = NULL;
    bool inServerBlock = false;
    bool inLocationBlock = false;
    int braceCount = 0;
    int locationBraceCount = 0;
    int lineNumber = 0;

    while (std::getline(inputFile, line))
    {
        lineNumber++;
        std::string cleanLine = line;
        std::string originalLine = line; // Keep a copy of the original line for error reporting

        // Clean up the line - remove leading and trailing spaces and tabs
        while (!cleanLine.empty() && (cleanLine[0] == ' ' || cleanLine[0] == '\t'))
            cleanLine.erase(0, 1);
        while (!cleanLine.empty() && (cleanLine[cleanLine.size() - 1] == ' ' || cleanLine[cleanLine.size() - 1] == '\t'))
            cleanLine.erase(cleanLine.size() - 1, 1);

        // Remove everything after #
        size_t commentPos = cleanLine.find('#');
        if (commentPos != std::string::npos)
            cleanLine = cleanLine.substr(0, commentPos);

        if (cleanLine.empty())
            continue;

        // Check if line contains "server" and "{" in any format
        if (cleanLine == "server" || cleanLine == "server{" || cleanLine == "server;" || cleanLine == "server {" )
        {
            // For server declarations, we don't require a semicolon
            if (cleanLine == "server")
            {
                // std::cout << "2222222222222222222222222222\n";
                std::string nextLine;
                while (std::getline(inputFile, nextLine))
                {
                    lineNumber++;
                    // Clean up the next line
                    while (!nextLine.empty() && (nextLine[0] == ' ' || nextLine[0] == '\t'))
                        nextLine.erase(0, 1);
                    while (!nextLine.empty() && (nextLine[nextLine.size() - 1] == ' ' || nextLine[nextLine.size() - 1] == '\t'))
                        nextLine.erase(nextLine.size() - 1, 1);

                    if (nextLine.empty() || nextLine[0] == '#')
                        continue;

                    // Check for opening brace
                    if (nextLine == "{;" || nextLine == "{")
                    {
                        if (inServerBlock)
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Nested server blocks are not allowed" << std::endl;
                            return std::vector<ServerConfig>();
                        }
                        inServerBlock = true;
                        braceCount++;
                        break;
                    }
                    else
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Expected '{' after server, got: '"
                                  << nextLine << "'" << std::endl;
                        return std::vector<ServerConfig>();
                    }
                }
            }
            else
            {
                inServerBlock = true;
                braceCount++;
            }

            if (inServerBlock)
            {
                currentServer = ServerConfig();
                currentServer.port = 80;                          // Default port
                currentServer.host = "localhost";                 // Default host
            }
        }
        else if (cleanLine == "}" || cleanLine == "};")
        {
            if (inLocationBlock)
            {
                locationBraceCount--;
                if (locationBraceCount == 0)
                {
                    // Validate required directives before closing location block
                    if (currentLocation != NULL)
                    {
                        // Check for missing required directives
                        // if (currentLocation->methods.empty())
                        // {
                        //     std::cerr << "Error: Location block '" << currentLocation->path
                        //               << "' is missing required 'method' directive" << std::endl;
                        //     exit(1);
                        // }

                        // Check if autoindex was set (assuming it's required)
                        // You might need to add a flag to track if autoindex was explicitly set

                        // Check for missing root directive (if required for your config)
                        // if (currentLocation->root.empty())
                        // {
                        //     std::cerr << "Error: Location block '" << currentLocation->path
                        //               << "' is missing required 'root' directive" << std::endl;
                        //     exit(1);
                        // }

                        // Check for missing index directive in specific cases
                    }

                    inLocationBlock = false;
                    currentLocation = NULL;
                }
            }
            else if (inServerBlock)
            {
                braceCount--;
                if (braceCount == 0)
                {
                    // Check for duplicate server (same port and server_name)
                    for (size_t i = 0; i < servers.size(); i++)
                    {
                        if (servers[i].port == currentServer.port &&
                            servers[i].server_name == currentServer.server_name)
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Duplicate server configuration found. "
                                      << "Server with port " << currentServer.port
                                      << " and server_name '" << currentServer.server_name
                                      << "' already exists" << std::endl;
                            return std::vector<ServerConfig>();
                        }
                    }

                    servers.push_back(currentServer);
                    inServerBlock = false;
                }
            }
            else
            {
                std::cerr << "Error: Line " << lineNumber << ": Unexpected closing brace outside of any block" << std::endl;
                return std::vector<ServerConfig>();
            }
        }
        else if (inServerBlock)
        {
            std::istringstream iss(cleanLine);
            std::string directive;
            iss >> directive;

            // Check if this is a directive that needs special handling
            bool isSpecialDirective = specialDirectives.find(directive) != specialDirectives.end();

            if (inLocationBlock && currentLocation != NULL)
            {
                // Check if directive is allowed in location blocks
                if (allowedLocationDirectives.find(directive) == allowedLocationDirectives.end())
                {
                    std::cerr << "Error: Line " << lineNumber << ": Unknown directive '"
                              << directive << "' in location block" << std::endl;
                    return std::vector<ServerConfig>();
                }

                // For location block directives, ensure they end with semicolon (unless special)
                if (!isSpecialDirective && !endsWithSemicolon(cleanLine))
                {
                    std::cerr << "Error: Line " << lineNumber << ": Missing semicolon at the end of directive: '"
                              << originalLine << "'" << std::endl;
                    return std::vector<ServerConfig>();
                }

                // Remove trailing semicolon now that we've checked it
                if (!cleanLine.empty() && cleanLine[cleanLine.size() - 1] == ';')
                    cleanLine.erase(cleanLine.size() - 1, 1);

                if (directive == "method")
                {
                    std::string method;
                    while (iss >> method)
                    {
                        // Remove semicolon if present
                        method = removeSemicolon(method);

                        // Split by comma if there are multiple methods in one token
                        std::stringstream methodStream(method);
                        std::string singleMethod;

                        while (std::getline(methodStream, singleMethod, ','))
                        {
                            // Trim whitespace from the method
                            while (!singleMethod.empty() && (singleMethod[0] == ' ' || singleMethod[0] == '\t'))
                                singleMethod.erase(0, 1);
                            while (!singleMethod.empty() && (singleMethod[singleMethod.size() - 1] == ' ' || singleMethod[singleMethod.size() - 1] == '\t'))
                                singleMethod.erase(singleMethod.size() - 1, 1);

                            if (!singleMethod.empty())
                                currentLocation->methods.push_back(singleMethod);
                        }
                    }
                }
                else if (directive == "autoindex")
                {
                    std::string value;
                    iss >> value;

                    // Remove semicolon if present
                    value = removeSemicolon(value);

                    if (value != "on" && value != "off")
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Invalid autoindex value '"
                                  << value << "'. Must be 'on' or 'off'" << std::endl;
                        return std::vector<ServerConfig>();
                    }
                    currentLocation->autoindex = (value == "on");
                }
                else if (directive == "root")
                {
                    std::string root;
                    iss >> root;
                    currentLocation->root = removeSemicolon(root);
                }
                else if (directive == "index")
                {
                    std::string index;
                    while (iss >> index)
                    {
                        // Handle the last item which may have a semicolon
                        index = removeSemicolon(index);
                        currentLocation->index.push_back(index);
                    }
                }
                else if (directive == "upload_path")
                {
                    std::string upload_path;
                    iss >> upload_path;
                    currentLocation->upload_path = removeSemicolon(upload_path);
                }
                else if (directive == "cgi_path")
                {
                    std::string cgi_path;
                    iss >> cgi_path;
                    currentLocation->cgi_path = removeSemicolon(cgi_path);
                }
                else if (directive == "redirection")
                {
                    std::string redirectionUrl;
                    iss >> redirectionUrl;

                    if (redirectionUrl.empty())
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Missing URL for redirection directive" << std::endl;
                        return std::vector<ServerConfig>();
                    }

                    currentLocation->redirection = removeSemicolon(redirectionUrl);

                    // Optional: Add basic URL validation
                    if (currentLocation->redirection.empty())
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Empty redirection URL" << std::endl;
                        return std::vector<ServerConfig>();
                    }
                }
            }
            else
            {
                // Check if directive is allowed in server blocks
                if (allowedServerDirectives.find(directive) == allowedServerDirectives.end())
                {
                    std::cerr << "Error: Line " << lineNumber << ": Unknown directive '"
                              << directive << "' in server block" << std::endl;
                    return std::vector<ServerConfig>();
                }

                // For server block directives, ensure they end with semicolon (unless special)
                if (!isSpecialDirective && !endsWithSemicolon(cleanLine))
                {
                    std::cerr << "Error: Line " << lineNumber << ": Missing semicolon at the end of directive: '"
                              << originalLine << "'" << std::endl;
                    return std::vector<ServerConfig>();
                }

                // Now that we've checked for semicolon, remove it for processing
                if (!cleanLine.empty() && cleanLine[cleanLine.size() - 1] == ';')
                    cleanLine.erase(cleanLine.size() - 1, 1);

                if (directive == "listen")
                {
                    std::string listenValue;
                    iss >> listenValue;

                    // Remove semicolon if present
                    listenValue = removeSemicolon(listenValue);
                    
                    // Check if it contains host:port format
                    size_t colonPos = listenValue.find(":");
                    if (colonPos != std::string::npos)
                    {
                        // Split host and port
                        std::string hostPart = listenValue.substr(0, colonPos);
                        std::string portPart = listenValue.substr(colonPos + 1);
                        
                        // Set host
                        if (!hostPart.empty())
                        {
                            currentServer.host = hostPart;
                        }
                        
                        // Validate and set port
                        if (portPart.empty())
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Missing port number after ':'" << std::endl;
                            return std::vector<ServerConfig>();
                        }
                        
                        // Check if port is a valid number
                        for (size_t i = 0; i < portPart.length(); i++)
                        {
                            if (!isdigit(portPart[i]))
                            {
                                std::cerr << "Error: Line " << lineNumber << ": Invalid port number: "
                                          << portPart << std::endl;
                                return std::vector<ServerConfig>();
                            }
                        }
                        
                        currentServer.port = atoi(portPart.c_str());
                    }
                    else
                    {
                        // Only port specified
                        // Check if it's a valid number
                        for (size_t i = 0; i < listenValue.length(); i++)
                        {
                            if (!isdigit(listenValue[i]))
                            {
                                std::cerr << "Error: Line " << lineNumber << ": Invalid port number: "
                                          << listenValue << std::endl;
                                return std::vector<ServerConfig>();
                            }
                        }
                        
                        currentServer.port = atoi(listenValue.c_str());
                    }
                    
                    // Validate port range
                    if (currentServer.port <= 0 || currentServer.port > 65535)
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Port number out of range (1-65535): "
                                  << currentServer.port << std::endl;
                        return std::vector<ServerConfig>();
                    }
                }
                else if (directive == "host")
                {
                    std::string host;
                    iss >> host;
                    currentServer.host = removeSemicolon(host);
                }
                else if (directive == "root")
                {
                    std::string root;
                    iss >> root;
                    currentServer.root = removeSemicolon(root);
                }
                else if (directive == "server_name")
                {
                    std::string server_name;
                    iss >> server_name;
                    currentServer.server_name = removeSemicolon(server_name);
                }
                else if (directive == "index")
                {
                    std::string index;
                    while (iss >> index)
                    {
                        // Handle the last item which may have a semicolon
                        index = removeSemicolon(index);
                        currentServer.index_files.push_back(index);
                    }
                }
                else if (directive == "location")
                {
                    // Check if we're already in a location block
                    if (inLocationBlock)
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Found new location block before closing previous one" << std::endl;
                        return std::vector<ServerConfig>();
                    }

                    LocationConfig loc;
                    std::string path;

                    // Read the entire rest of the line after "location"
                    std::string restOfLine;
                    std::getline(iss, restOfLine);

                    // Trim whitespace from both ends
                    size_t start = restOfLine.find_first_not_of(" \t");
                    size_t end = restOfLine.find_last_not_of(" \t");
                    if (start != std::string::npos)
                        restOfLine = restOfLine.substr(start, end - start + 1);
                    else
                        restOfLine.clear();

                    // Remove trailing semicolon if present
                    if (!restOfLine.empty() && restOfLine[restOfLine.size() - 1] == ';')
                        restOfLine.erase(restOfLine.size() - 1, 1);

                    // Find opening brace in the line
                    size_t bracePos = restOfLine.find('{');
                    if (bracePos != std::string::npos)
                    {
                        // Split into path and potential brace
                        std::string pathPart = restOfLine.substr(0, bracePos);

                        // Trim trailing whitespace from path
                        size_t pathEnd = pathPart.find_last_not_of(" \t");
                        if (pathEnd != std::string::npos)
                            pathPart = pathPart.substr(0, pathEnd + 1);
                        else
                            pathPart.clear();

                        // Validate that pathPart contains only one word (the location path)
                        std::istringstream pathStream(pathPart);
                        std::string firstToken, secondToken;
                        pathStream >> firstToken;
                        
                        if (firstToken.empty())
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Missing location path" << std::endl;
                            return std::vector<ServerConfig>();
                        }
                        
                        if (pathStream >> secondToken)
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Invalid location directive syntax. "
                                      << "Expected 'location <path> {', got extra tokens: '"
                                      << secondToken << "'" << std::endl;
                            return std::vector<ServerConfig>();
                        }
                        
                        loc.path = firstToken;

                        // Check for content after brace
                        std::string afterBrace = restOfLine.substr(bracePos + 1);
                        size_t contentPos = afterBrace.find_first_not_of(" \t");
                        if (contentPos != std::string::npos)
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Unexpected content after '{' in location block: "
                                      << afterBrace << std::endl;
                            return std::vector<ServerConfig>();
                        }
                        inLocationBlock = true;
                        locationBraceCount++;
                    }
                    else
                    {
                        // No brace found in this line, validate that we only have one path token
                        std::istringstream pathStream(restOfLine);
                        std::string firstToken, secondToken;
                        pathStream >> firstToken;
                        
                        if (firstToken.empty())
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Missing location path" << std::endl;
                            return std::vector<ServerConfig>();
                        }
                        
                        if (pathStream >> secondToken)
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Invalid location directive syntax. "
                                      << "Expected 'location <path>', got extra tokens: '"
                                      << secondToken << "'" << std::endl;
                            return std::vector<ServerConfig>();
                        }
                        
                        loc.path = firstToken;

                        // Check next line for opening brace with proper trimming
                        std::string nextLine;
                        while (std::getline(inputFile, nextLine))
                        {
                            lineNumber++;
                            // Trim leading and trailing whitespace
                            size_t lineStart = nextLine.find_first_not_of(" \t");
                            if (lineStart == std::string::npos)
                            {
                                continue; // Skip empty lines
                            }
                            size_t lineEnd = nextLine.find_last_not_of(" \t");
                            nextLine = nextLine.substr(lineStart, lineEnd - lineStart + 1);

                            // Remove trailing semicolon if present
                            if (!nextLine.empty() && nextLine[nextLine.size() - 1] == ';')
                                nextLine.erase(nextLine.size() - 1, 1);

                            if (nextLine.empty() || nextLine[0] == '#')
                                continue;

                            if (nextLine == "{")
                            {
                                inLocationBlock = true;
                                locationBraceCount++;
                                break;
                            }
                            else
                            {
                                std::cerr << "Error: Line " << lineNumber << ": Expected '{' after location path, got: '"
                                          << nextLine << "'" << std::endl;
                                return std::vector<ServerConfig>();
                            }
                        }
                    }

                    if (!inLocationBlock)
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Missing opening brace for location block" << std::endl;
                        return std::vector<ServerConfig>();
                    }

                    // Add the location to the current server's locations vector
                    currentServer.locations.push_back(loc);
                    currentLocation = &currentServer.locations.back();
                }
                else if (directive == "error_page")
                {
                    std::string code_or_path;
                    std::vector<int> error_codes;
                    std::string path;

                    // First collect all error codes
                    while (iss >> code_or_path)
                    {
                        // Remove semicolon if it's the last item
                        bool isSemicolonRemoved = false;
                        if (!code_or_path.empty() && code_or_path[code_or_path.size() - 1] == ';')
                        {
                            code_or_path.erase(code_or_path.size() - 1);
                            isSemicolonRemoved = true;
                        }

                        // If it starts with a digit, treat it as an error code
                        if (!code_or_path.empty() && isdigit(code_or_path[0]))
                        {
                            error_codes.push_back(atoi(code_or_path.c_str()));
                        }
                        else
                        {
                            // Found the path, store it and break
                            path = code_or_path;

                            // If we removed a semicolon, this was the last token
                            if (isSemicolonRemoved)
                                break;
                        }
                    }

                    // Get any remaining part of the path (in case it has spaces)
                    std::string path_part;
                    while (iss >> path_part)
                    {
                        // Handle the last item which may have a semicolon
                        if (!path_part.empty() && path_part[path_part.size() - 1] == ';')
                        {
                            path_part.erase(path_part.size() - 1);
                            path += " " + path_part;
                            break;
                        }
                        else
                        {
                            path += " " + path_part;
                        }
                    }

                    // Associate the path with all error codes
                    if (!error_codes.empty() && !path.empty())
                    {
                        for (size_t i = 0; i < error_codes.size(); i++)
                        {
                            currentServer.error_pages[error_codes[i]] = path;
                        }
                    }
                    else
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Invalid error_page directive format" << std::endl;
                        return std::vector<ServerConfig>();
                    }
                }
                else if (directive == "client_max_body_size")
                {
                    std::string size;
                    iss >> size;

                    // Remove trailing semicolon if present
                    size = removeSemicolon(size);

                    if (!size.empty())
                    {
                        // Check if size is a valid number
                        for (size_t i = 0; i < size.length(); i++)
                        {
                            if (!isdigit(size[i]))
                            {
                                std::cerr << "Error: Line " << lineNumber << ": Invalid client_max_body_size value: "
                                          << size << std::endl;
                                return std::vector<ServerConfig>();
                            }
                        }
                        currentServer.client_max_body_size = atoll(size.c_str());
                    }
                    else
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Missing value for client_max_body_size" << std::endl;
                        return std::vector<ServerConfig>();
                    }
                }
            }
        }
        else
        {
            std::cerr << "Error: Line " << lineNumber << ": Unexpected content outside of server block: '"
                      << cleanLine << "'" << std::endl;
            return std::vector<ServerConfig>();
        }
    }

    // Check for unclosed server blocks
    if (braceCount != 0)
    {
        std::cerr << "Error: Missing closing brace for server block" << std::endl;
        return std::vector<ServerConfig>();
    }

    // Check for unclosed location blocks
    if (locationBraceCount != 0)
    {
        std::cerr << "Error: Missing closing brace for location block" << std::endl;
        return std::vector<ServerConfig>();
    }

    return servers;
}