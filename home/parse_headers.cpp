#include "server.hpp"

void parse_headers(std::istringstream &stream, std::map<std::string, std::string> &headers, ChunkedClientInfo &client)
{
    std::string line;
    while (std::getline(stream, line))
    {
        // Remove carriage return if present
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            break;

        size_t pos = line.find(":");
        if (pos != std::string::npos && pos > 0)
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            if (key == "Host")
            {
                if (value.empty())
                {
                    std::cerr << "Host header is empty!" << std::endl;
                    continue; // Skip empty Host header
                }
                else
                {
                    size_t pos1 = value.find(":");
                    if (pos1 != std::string::npos && pos1 > 0)
                    {
                        std::string key1 = value.substr(1, pos1 - 1);
                        std::string value1 = value.substr(pos1 + 1);
                        client.request_obj.server_host = key1;
                        client.request_obj.server_port = strtol(value1.c_str(), NULL, 10);
                    }
                }
            };

            // Trim whitespace from value
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos)
                value = value.substr(start);

            headers[key] = value;
        }
    }
}

// Parse method line and extract request information
bool parse_method_line(ChunkedClientInfo &client)
{
    std::istringstream method_stream(client.headers);
    std::string method_line;
    if (std::getline(method_stream, method_line))
    {
        parsing_method(client.request_obj, method_line);
        parse_headers(method_stream, client.parsed_headers, client);
        return true;
    }
    return false;
}

// Extract content length from headers
void extract_content_length(ChunkedClientInfo &client)
{
    std::map<std::string, std::string>::iterator it = client.parsed_headers.find("Content-Length");
    if (it != client.parsed_headers.end())
    {
        client.content_length = atoll(it->second.c_str());
    }
    else
    {
        std::map<std::string, std::string>::iterator it = client.parsed_headers.find("Transfer-Encoding");
        if (it != client.parsed_headers.end())
        {
            client.transfer_encod = it->second;
        }
    }
}

// Check if headers are complete in received data
bool check_headers_complete(ChunkedClientInfo &client)
{
    size_t header_end = client.partial_data.find("\r\n\r\n");
    if (header_end != std::string::npos)
    {
        client.headers = client.partial_data.substr(0, header_end);
        client.partial_data = client.partial_data.substr(header_end + 4);
        client.headers_complete = true;
        return true;
    }
    return false;
}

// Find the right server config (index) based on Host header.
// If no match → return the default for this socket.
std::string extract_host_header(const std::string &raw_headers)
{
    std::istringstream stream(raw_headers);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.find("Host:") == 0)
        {
            size_t pos = line.find(":");
            if (pos != std::string::npos)
            {
                std::string host = line.substr(pos + 1);
                while (!host.empty() && (host[0] == ' ' || host[0] == '\t'))
                    host.erase(0, 1);
                while (!host.empty() && (host[host.size() - 1] == '\r' || host[host.size() - 1] == '\n'))
                    host.erase(host.size() - 1);
                return host;
            }
        }
    }
    return "";
}
size_t resolve_server_index(const std::string &host_header,
                            const std::vector<Request> &global_obj,
                            const std::map<std::string, std::vector<size_t> > &hostport_to_indexes, size_t client_server_idx)
{
    // Check if the host header matches any known host:port combinations
    std::map<std::string, std::vector<size_t> >::const_iterator it = hostport_to_indexes.find(host_header);
    if (it != hostport_to_indexes.end())
    {
        for (size_t i = 0; i < it->second.size(); ++i)
        {
            size_t server_index = it->second[i];
            if (server_index < global_obj.size())
            {
                std::string server_name = global_obj[server_index].server.server_name;
                ssize_t pos = host_header.find(":");
                if (pos != std::string::npos)
                {
                    std::string host = host_header.substr(0, pos);
                    std::string port_str = host_header.substr(pos + 1);
                    ;
                    if (server_name == host)
                        return server_index; // Match found, return the server index
                }
            }
        }
    }

    // If still no match, return the client_server_idx as a fallback
    return client_server_idx;
}

// Read headers from the socket and decide which server should handle the request
bool read_headers_chunked(int fd,
                          ChunkedClientInfo &client,
                          std::vector<Request> &global_obj,
                          const std::map<std::string, std::vector<size_t> > &hostport_to_indexes,
                          size_t client_server_idx)
{
    char buffer[CHUNK_SIZE];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

    if (bytes_read > 0)
    {
        client.partial_data.append(buffer, bytes_read);
        client.last_active = time(NULL);

        if (check_headers_complete(client))
        {
            std::string host = extract_host_header(client.headers);
            size_t server_index = resolve_server_index(host, global_obj, hostport_to_indexes, client_server_idx);
            client.server_index = server_index;
            client.request_obj.server = global_obj[server_index].server;
            client.request_obj.root = global_obj[server_index].root;
            client.request_obj.server_port = global_obj[server_index].server_port;
            client.request_obj = global_obj[server_index];
            client.request_obj.fd_client = fd;
            client.request_obj.server.client_max_body_size = global_obj[server_index].server.client_max_body_size;

            if (parse_method_line(client))
            {
                extract_content_length(client);

                std::cout << "Headers parsed - Method: " << client.request_obj.mthod
                          << ", Path: " << client.request_obj.path
                          << ", Content-Length: " << client.content_length << std::endl;

                return true;
            }
        }
    }
    else if (bytes_read == 0)
    {
        client.is_active = false;
        return false;
    }
    else if (bytes_read < 0)
    {
        perror("Read headers failed");
        client.is_active = false;
        return false;
    }

    return false;
}

// Extract boundary from multipart content type
std::string extract_boundary(const std::string &content_type)
{
    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos != std::string::npos)
    {
        boundary_pos += 9;
        size_t boundary_end = content_type.find(";", boundary_pos);
        if (boundary_end == std::string::npos)
            boundary_end = content_type.length();

        std::string boundary_value = content_type.substr(boundary_pos, boundary_end - boundary_pos);
        // Remove quotes if present
        if (!boundary_value.empty() && boundary_value[0] == '"' &&
            boundary_value[boundary_value.length() - 1] == '"')
        {
            boundary_value = boundary_value.substr(1, boundary_value.length() - 2);
        }
        return "--" + boundary_value;
    }
    return "";
}

void chunked_transfer_encoding(ChunkedClientInfo &client, std::string &data)
{
    std::string chunked_data = data;
    std::string chunk_buffer;

    while (true)
    {
        size_t chunk_size_end = chunked_data.find("\r\n");
        if (chunk_size_end == std::string::npos)
        {
            chunk_buffer += chunked_data;
            break;
        }

        std::string chunk_size_str = chunked_data.substr(0, chunk_size_end);
        size_t chunk_size = strtol(chunk_size_str.c_str(), NULL, 16);
        if (chunk_size == 0)
            break;
        // Move past chunk size and CRLF
        chunked_data = chunked_data.substr(chunk_size_end + 2);

        if (chunked_data.size() <= chunk_size)
        {
            chunk_buffer += chunked_data;
            client.bytes_chunked = chunk_size - chunked_data.size();
            break;
        }

        // Extract chunk data
        std::string chunk_data = chunked_data.erase(0, chunk_size);
        chunk_buffer.append(chunk_data);
        chunk_buffer += "\r\n";

        // Move past chunk data and CRLF
        chunked_data = chunked_data.substr(chunk_size + 2);
    }

    data = chunk_buffer;
}

// Process multipart form data
bool process_multipart_request(ChunkedClientInfo &client, const std::string &content_type)
{
    client.boundary = extract_boundary(content_type);
    if (client.boundary.empty())
    {
        std::cerr << "No boundary found in multipart request" << std::endl;
        return false;
    }

    if (!client.partial_data.empty())
    {
        std::string data_to_process = client.partial_data;

        if (client.transfer_encod == "chunked")
        {
            chunked_transfer_encoding(client, data_to_process);
        }

        if (!data_to_process.empty())
        {
            process_multipart_data(client, data_to_process);
        }
        if (client.bytes_chunked == 0 && data_to_process.empty())
        {
            client.partial_data.clear();
        }
    }
    return true;
}

// Process URL encoded form data
bool process_urlencoded_request(ChunkedClientInfo &client)
{
    static std::map<std::string, std::string> post_res;
    std::map<std::string, std::string> form_data;

    std::istringstream ss(client.partial_data);
    std::string pair;
    while (std::getline(ss, pair, '&'))
    {
        size_t eq = pair.find('=');
        if (eq != std::string::npos)
        {
            std::string key = pair.substr(0, eq);
            std::string value = pair.substr(eq + 1);
            form_data[key] = value;
        }
    }

    client.request_obj.path = Format_urlencoded(client.request_obj.path, post_res,
                                                form_data, client.request_obj);

    if (client.request_obj.path.empty())
    {
        std::cerr << "Failed to resolve POST path" << std::endl;
        return false;
    }

    return true;
}

// Extract filename from Content-Disposition header
std::string extract_filename(const std::string &data)
{
    size_t disp_pos = data.find("Content-Disposition:");
    if (disp_pos != std::string::npos)
    {
        size_t line_end = data.find("\r\n", disp_pos);
        if (line_end != std::string::npos)
        {
            std::string disp_line = data.substr(disp_pos, line_end - disp_pos);

            size_t fn_pos = disp_line.find("filename=\"");
            if (fn_pos != std::string::npos)
            {
                fn_pos += 10;
                size_t fn_end = disp_line.find("\"", fn_pos);
                if (fn_end != std::string::npos)
                {
                    return disp_line.substr(fn_pos, fn_end - fn_pos);
                }
            }
        }
    }
    return "";
}
