#include "server.hpp"

bool process_plain_text_request(ChunkedClientInfo &client)
{
    std::cout << "Processing plain text request" << std::endl;
    if (client.partial_data.empty())
    {
        std::cerr << "No data received for plain text request" << std::endl;
        return false;
    }

    if (!open_file_for_writing(client, "plain_text.txt"))
    {
        client.is_active = false;
        return false;
    }

    client.file_stream.write(client.partial_data.c_str(), client.partial_data.size());
    client.file_stream.close();
    client.upload_state = 2;
    std::cout << "Plain text request processed successfully" << std::endl;
    return true;
}
// Process POST request headers and content type
bool process_post_request(ChunkedClientInfo &client)
{
    if (client.content_length <= 0 && client.transfer_encod.empty())
    {
        std::cerr << "POST request without valid Content-Length" << std::endl;
        return false;
    }

    std::map<std::string, std::string>::iterator ct_it = client.parsed_headers.find("Content-Type");
    if (ct_it == client.parsed_headers.end())
    {
        std::cerr << "POST request without Content-Type" << std::endl;
        return false;
    }

    if (ct_it->second.find("multipart/form-data") != std::string::npos)
    {
        return process_multipart_request(client, ct_it->second);
    }
    else if (ct_it->second.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        if (process_urlencoded_request(client))
        {
            client.upload_state = 2;
            return true;
        }
        return false;
    }
    else if (ct_it->second.find("plain/text") != std::string::npos)
    {
        if (process_plain_text_request(client))
        {
            client.request_obj.mthod = "GET";
            client.request_obj.path = client.filename;
            client.upload_state = 2;
            return true;
        }
        return false;
    }
    else
    {
        std::cerr << "Unsupported Content-Type for POST: " << ct_it->second << std::endl;
        return false;
    }
}
// Open file for writing
bool open_file_for_writing(ChunkedClientInfo &client, const std::string &filename)
{
    std::string index_path = client.request_obj.uri;
    bool found_upload_path = false;
    std::string upload_path;
    while (true)
    {
        for (size_t i = 0; i < client.request_obj.local_data.size(); ++i)
        {
            std::string location_path = normalize_path(client.request_obj.local_data[i].path);
            index_path = normalize_path(index_path);
            if (index_path == location_path)
            {
                found_upload_path = true;
                upload_path = client.request_obj.local_data[i].upload_path;
                break;
            }
        }
        if (index_path.empty() || found_upload_path == true || index_path == client.request_obj.root)
            break;
        index_path = remove_last_path_component(index_path);
    }
    if (!upload_path.empty())
    {
        upload_path = remove_first_slash(upload_path);
        client.filename = upload_path + "/" + filename;
    }
    else
        client.filename = "upload/" + filename;
    client.file_stream.open(client.filename.c_str(), std::ios::binary);
    if (!client.file_stream.is_open())
    {
        std::cerr << "Failed to open file: " << client.filename << std::endl;
        return false;
    }
    std::cout << "File opened successfully: " << client.filename << std::endl;
    return true;
}

// Find and write file data, check for boundary
bool write_file_data(ChunkedClientInfo &client, const std::string &data)
{
    size_t data_start = data.find("\r\n\r\n");
    if (data_start == std::string::npos)
        return false;

    data_start += 4;
    std::string file_data = data.substr(data_start);

    if (!client.boundary.empty())
    {
        size_t boundary_pos = file_data.find(client.boundary);
        if (boundary_pos != std::string::npos)
        {
            size_t write_size = boundary_pos;
            if (boundary_pos >= 2 && file_data.substr(boundary_pos - 2, 2) == "\r\n")
            {
                write_size = boundary_pos - 2;
            }

            if (write_size > 0)
            {
                client.file_stream.write(file_data.c_str(), write_size);
            }
            client.file_stream.close();
            client.upload_state = 2;
            std::cout << "File upload completed: " << client.filename << std::endl;
            return true;
        }
    }
    size_t end_data_pos = file_data.find("0\r\n");
    if (end_data_pos != std::string::npos)
    {
        file_data = file_data.substr(0, end_data_pos);
        client.file_stream.write(file_data.c_str(), file_data.length());
        client.file_stream.close();
        client.upload_state = 2;
        std::cout << "File upload completed: " << client.filename << std::endl;
        return true;
    }
    if (!file_data.empty())
    {
        client.file_stream.write(file_data.c_str(), file_data.length());
        std::cout << "Wrote file data chunk: " << file_data.length() << " bytes" << std::endl;
    }
    return false;
}

// Process multipart data
void process_multipart_data(ChunkedClientInfo &client, const std::string &data)
{
    if (client.filename.empty())
    {
        std::string filename = extract_filename(data);
        if (!filename.empty())
        {
            if (!open_file_for_writing(client, filename))
            {
                client.is_active = false;
                return;
            }
        }
    }
       ssize_t pos = data.find("\r\n\r\n");
    if (pos != std::string::npos)
    {
        client.cgi_headrs = data.substr(0, pos + 4);
    }

    if (!client.filename.empty() && client.file_stream.is_open())
    {
        write_file_data(client, data);
    }
}

// Check for boundary in body data
bool check_body_boundary(ChunkedClientInfo &client, const std::string &data)
{
    if (client.boundary.empty())
        return false;

    size_t boundary_pos = data.find(client.boundary);
    if (boundary_pos != std::string::npos)
    {
        size_t write_size = boundary_pos;
        if (boundary_pos >= 2 && data.substr(boundary_pos - 2, 2) == "\r\n")
        {

            write_size = boundary_pos - 2;
        }

        if (write_size > 0 && client.file_stream.is_open())
        {
            client.file_stream.write(data.c_str(), write_size);
            std::cout << "Wrote final chunk: " << write_size << " bytes" << std::endl;
        }

        if (client.file_stream.is_open())
        {
            client.file_stream.close();
        }

        client.bytes_read += write_size;
        client.upload_state = 2;
        std::cout << "File upload completed: " << client.filename << std::endl;
        return true;
    }
    return false;
}

// Write body data to file
void write_body_to_file(ChunkedClientInfo &client, const char *buffer, ssize_t bytes_read)
{
    if (client.file_stream.is_open())
    {
        client.file_stream.write(buffer, bytes_read);
        std::cout << "Wrote body chunk to file: " << bytes_read << " bytes" << std::endl;
    }
    else
    {
        std::cout << "WARNING: Received body data but no file is open!" << std::endl;
        std::string data(buffer, bytes_read);
        process_multipart_data(client, data);
    }
}

// // Check if upload is complete by content length
bool check_upload_complete(ChunkedClientInfo &client, int &fd_socket)
{
    if (client.content_length > 0 && client.bytes_read >= client.content_length)
    {
        if (client.file_stream.is_open())
        {
            client.file_stream.close();
            std::cout << "Closed file due to content length reached" << std::endl;
        }
        client.upload_state = 2;
        std::cout << "Upload completed by content length: " << client.filename << std::endl;
        return true;
    }
    return false;
}

// Show upload progress
void show_upload_progress(const ChunkedClientInfo &client)
{
    if (client.content_length > 0)
    {
        double progress = (double)client.bytes_read / client.content_length * 100.0;
        std::cout << "Upload progress: " << progress << "% ("
                  << client.bytes_read << "/" << client.content_length << " bytes)" << std::endl;
    }
}

// Get MIME type from file extension
std::string getmine_type(std::string line_path, std::map<std::string, std::string> mime)
{
    std::string::size_type pos = line_path.rfind(".");
    if (pos != std::string::npos)
    {
        std::string type = line_path.substr(pos);
        std::map<std::string, std::string>::iterator it = mime.find(type);
        if (it != mime.end())
        {
            return it->second;
        }
    }
    return "application/octet-stream";
}

// Send HTTP response based on method
void send_response(int fd, ChunkedClientInfo &client)
{
    if (is_cgi_request(client.request_obj.path))
    {

        std::string index_path = client.request_obj.uri;
        bool found = false;
        // For POST requests, make sure body data is available
        while (true)
        {
            for (size_t i = 0; i < client.request_obj.local_data.size(); i++)
            {
                std::string location_path = normalize_path(client.request_obj.local_data[i].path);

                if (index_path == location_path)
                {
                    found = true;
                    client.request_obj.cgj_path = client.request_obj.local_data[i].cgi_path;
                    break;
                }
            }
            index_path = remove_last_path_component(index_path);
            if (found == true || index_path == "/")
                break;
        }

        if (!client.request_obj.cgj_path.empty())
        {
            handle_cgi_request(client, fd, client.parsed_headers);
            return;
        }
    }
    if (client.request_obj.mthod == "content_length")
    {
        std::cerr << "Content-Length exceeded or not set" << std::endl;
        sendErrorResponse(fd, 413, "Request Entity Too Large", client.request_obj.server.error_pages[413]);
        return;
    }
    else if (client.request_obj.mthod == "GET")
    {
        std::string type = getmine_type(client.request_obj.path, client.request_obj.mimitype);
        parsing_Get(client.parsed_headers, client.request_obj.path, fd, type,
                    client.request_obj.uri, client.request_obj);
    }
    else if (client.request_obj.mthod == "DELETE")
    {
        std::string fullPath;
        ssize_t pos = client.request_obj.uri.find_last_of("/");
        if (pos != std::string::npos)
        {
            // check is there data after last '/' bcouse we can have /login/ or alot of ///
            if (pos + 1 == client.request_obj.uri.size())
            {
                // If there is no data after the last '/', we cannot determine the filename
                std::string new_path = client.request_obj.uri.substr(0, pos - 1);
                ssize_t size_type2 = new_path.find_last_of("/");
                if (size_type2 != std::string::npos)
                    client.filename = new_path.substr(size_type2 + 1);
                else
                {
                    std::cerr << "Error: No filename provided in URI." << std::endl;
                    sendErrorResponse(fd, 400, "Bad Request", client.request_obj.server.error_pages[400]);
                    return;
                }
            }

            client.filename = client.request_obj.uri.substr(pos + 1);
        }
        // response_post("error_page/413.html", fd, response);
        std::string index_path = client.request_obj.uri;
        bool found_delete_path = false;
        std::string delete_path;
        while (true)
        {
            for (size_t i = 0; i < client.request_obj.local_data.size(); ++i)
            {
                std::string location_path = normalize_path(client.request_obj.local_data[i].path);
                index_path = normalize_path(index_path);
                if (index_path == location_path)
                {
                    found_delete_path = true;
                    delete_path = client.request_obj.local_data[i].upload_path;
                    break;
                }
            }
            if (index_path.empty() || found_delete_path == true || index_path == client.request_obj.root)
                break;
            index_path = remove_last_path_component(index_path);
        }
        if (!delete_path.empty())
        {
            delete_path = remove_first_slash(delete_path);
            fullPath = delete_path + "/" + client.filename;
        }
        else
        {
            fullPath = "upload/" + client.filename;
        }
        struct stat pathStat;
        if (stat(fullPath.c_str(), &pathStat) != 0)
        {
            sendErrorResponse(fd, 404, "Not Found", "error_page/404.html");
            return;
        }

        if (access(fullPath.c_str(), W_OK) != 0)
        {
            sendErrorResponse(fd, 403, "Forbidden", client.request_obj.server.error_pages[403]);
            return;
        }

        int status = (S_ISDIR(pathStat.st_mode)) ? rmdir(fullPath.c_str()) : remove(fullPath.c_str());

        if (status == 0)
        {
           sendErrorResponse(fd, 200, "OK", client.request_obj.server.error_pages[200]); // Successfully deleted
            return;
        }
        else
        {
            sendErrorResponse(fd, 500, "Internal Server Error", client.request_obj.server.error_pages[500]); // Internal error
            return;
        }

    }
    else if (client.request_obj.mthod == "POST")
    {
        std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " +
                             getContentType(client.request_obj.path) + "\r\n";
        response_post(client.request_obj.path, fd, header);
    }
    else if (client.request_obj.mthod == "Redirection")
    {
        send(fd, client.request_obj.response_red.c_str(), client.request_obj.response_red.size(), 0);
    }
    else
    {
        sendErrorResponse(fd, 405, "Method Not Allowed", client.request_obj.server.error_pages[405]);
    }
}
