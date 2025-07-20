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

struct LocationConfig
{
    std::string path;
    std::vector<std::string> methods;
    bool autoindex;
    std::string root;
    std::vector<std::string> index;
    std::string upload_path;
    std::string cgi_path;
};

struct ServerConfig
{
    std::string host;
    int port;
    std::string root;
    std::string server_name;
    std::vector<std::string> index_files;
    std::map<int, std::string> error_pages;
    size_t client_max_body_size;
    std::vector<LocationConfig> locations;
};
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
// void printLocations(const std::vector<LocationConfig> &locations)
// {
//     for (size_t i = 0; i < locations.size(); i++)
//     {
//         std::cout << "\nLocation " << i + 1 << ":" << std::endl;
//         std::cout << "  Path: " << locations[i].path << std::endl;

//         std::cout << "  Methods: ";
//         for (size_t j = 0; j < locations[i].methods.size(); j++)
//         {
//             std::cout << locations[i].methods[j];
//             if (j < locations[i].methods.size() - 1)
//             {
//                 std::cout << ", ";
//             }
//         }
//         std::cout << std::endl;

//         std::cout << "  Autoindex: " << (locations[i].autoindex ? "on" : "off") << std::endl;

//         if (!locations[i].root.empty())
//             std::cout << "  Root: " << locations[i].root << std::endl;

//         if (!locations[i].index.empty())
//             std::cout << "  Index: " << locations[i].index << std::endl;

//         if (!locations[i].upload_path.empty())
//             std::cout << "  Upload Path: " << locations[i].upload_path << std::endl;

//         if (!locations[i].cgi_path.empty())
//             std::cout << "  CGI Path: " << locations[i].cgi_path << std::endl;
//     }
// }

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
        if (cleanLine == "server" || cleanLine == "server{" || cleanLine == "server;" ||
            cleanLine.find("server {") != std::string::npos)
        {
            // For server declarations, we don't require a semicolon
            if (cleanLine == "server")
            {
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
                            exit(1);
                        }
                        inServerBlock = true;
                        braceCount++;
                        break;
                    }
                    else
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Expected '{' after server, got: '"
                                  << nextLine << "'" << std::endl;
                        exit(1);
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
                currentServer.client_max_body_size = 1024 * 1024; // Default 1MB
            }
        }
        else if (cleanLine == "}" || cleanLine == "};")
        {
            if (inLocationBlock)
            {
                locationBraceCount--;
                if (locationBraceCount == 0)
                {
                    inLocationBlock = false;
                    currentLocation = NULL;
                }
            }
            else if (inServerBlock)
            {
                braceCount--;
                if (braceCount == 0)
                {
                    servers.push_back(currentServer);
                    inServerBlock = false;
                }
            }
            else
            {
                std::cerr << "Error: Line " << lineNumber << ": Unexpected closing brace outside of any block" << std::endl;
                exit(1);
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
                    exit(1);
                }

                // For location block directives, ensure they end with semicolon (unless special)
                if (!isSpecialDirective && !endsWithSemicolon(cleanLine))
                {
                    std::cerr << "Error: Line " << lineNumber << ": Missing semicolon at the end of directive: '"
                              << originalLine << "'" << std::endl;
                    exit(1);
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
                        exit(1);
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
            }
            else
            {
                // Check if directive is allowed in server blocks
                if (allowedServerDirectives.find(directive) == allowedServerDirectives.end())
                {
                    std::cerr << "Error: Line " << lineNumber << ": Unknown directive '"
                              << directive << "' in server block" << std::endl;
                    exit(1);
                }

                // For server block directives, ensure they end with semicolon (unless special)
                if (!isSpecialDirective && !endsWithSemicolon(cleanLine))
                {
                    std::cerr << "Error: Line " << lineNumber << ": Missing semicolon at the end of directive: '"
                              << originalLine << "'" << std::endl;
                    exit(1);
                }

                // Now that we've checked for semicolon, remove it for processing
                if (!cleanLine.empty() && cleanLine[cleanLine.size() - 1] == ';')
                    cleanLine.erase(cleanLine.size() - 1, 1);

                if (directive == "listen")
                {
                    std::string portStr;
                    iss >> portStr;

                    // Remove semicolon if present
                    portStr = removeSemicolon(portStr);

                    // Check if portStr is a valid number
                    for (size_t i = 0; i < portStr.length(); i++)
                    {
                        if (!isdigit(portStr[i]))
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Invalid port number: "
                                      << portStr << std::endl;
                            exit(1);
                        }
                    }

                    currentServer.port = atoi(portStr.c_str());
                    if (currentServer.port <= 0 || currentServer.port > 65535)
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Invalid port number: "
                                  << currentServer.port << std::endl;
                        exit(1);
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
                        exit(1);
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
                        loc.path = restOfLine.substr(0, bracePos);

                        // Trim trailing whitespace from path
                        size_t pathEnd = loc.path.find_last_not_of(" \t");
                        if (pathEnd != std::string::npos)
                            loc.path = loc.path.substr(0, pathEnd + 1);
                        else
                            loc.path.clear();

                        // Check for content after brace
                        std::string afterBrace = restOfLine.substr(bracePos + 1);
                        size_t contentPos = afterBrace.find_first_not_of(" \t");
                        if (contentPos != std::string::npos)
                        {
                            std::cerr << "Error: Line " << lineNumber << ": Unexpected content after '{' in location block: "
                                      << afterBrace << std::endl;
                            exit(1);
                        }
                        inLocationBlock = true;
                        locationBraceCount++;
                    }
                    else
                    {
                        // No brace found in this line, path is the whole line
                        loc.path = restOfLine;

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
                                exit(1);
                            }
                        }
                    }

                    if (!inLocationBlock)
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Missing opening brace for location block" << std::endl;
                        exit(1);
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
                        exit(1);
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
                                exit(1);
                            }
                        }
                        currentServer.client_max_body_size = atoi(size.c_str());
                    }
                    else
                    {
                        std::cerr << "Error: Line " << lineNumber << ": Missing value for client_max_body_size" << std::endl;
                        exit(1);
                    }
                }
            }
        }
        else
        {
            std::cerr << "Error: Line " << lineNumber << ": Unexpected content outside of server block: '"
                      << cleanLine << "'" << std::endl;
            exit(1);
        }
    }

    // Check for unclosed server blocks
    if (braceCount != 0)
    {
        std::cerr << "Error: Missing closing brace for server block" << std::endl;
        exit(1);
    }

    // Check for unclosed location blocks
    if (locationBraceCount != 0)
    {
        std::cerr << "Error: Missing closing brace for location block" << std::endl;
        exit(1);
    }

    return servers;
}

int main()
{
    std::vector<ServerConfig> servers = check_configfile();

    if (servers.empty())
    {
        std::cout << "No servers found in configuration file" << std::endl;
        return 1;
    }

    size_t serverIndex = 1;
    size_t locationIndex = 0;
    if (serverIndex >= servers.size())
    {
        std::cout << "Server index " << serverIndex << " is out of bounds. Total servers: " << servers.size() << std::endl;
        return 1;
    }

    if (locationIndex >= servers[serverIndex].locations.size())
    {
        std::cout << "Location index " << locationIndex << " is out of bounds for server " << serverIndex
                  << ". Total locations: " << servers[serverIndex].locations.size() << std::endl;
        return 1;
    }

    std::cout << servers[serverIndex].locations[locationIndex].path << std::endl;
    std::cout << servers[0].locations[0].index[1] << std::endl;

    return 0;
}