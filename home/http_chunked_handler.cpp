#include "server.hpp"

std::string get_buffer(std::string buffer, ssize_t bytes_read, ssize_t &size_chunk, ChunkedClientInfo &client)
{
    std::string stor_data;
    static int count = 0;
    // Prepend any leftover buffer from last call
    if (!client.temp_buffer.empty())
    {
        buffer = client.temp_buffer + buffer;
        client.temp_buffer.clear();
    }

    if (size_chunk > 0)
    {
        if ((ssize_t)buffer.size() >= size_chunk)
        {
            stor_data.assign(buffer, 0, size_chunk);
            buffer.erase(0, size_chunk + 2);
            size_chunk = 0;
            // Check if we have \r\n if we dont we read again socket
            if (stor_data.find("\r\n") == std::string::npos)
            {
                // Incomplete chunk -- save for later
                client.temp_buffer = buffer;
                return stor_data;
            }
        }
        else
        {
            stor_data = buffer;
            size_chunk -= buffer.size();
            buffer.clear();
            return stor_data;
        }
    }
    std::string data_to_process = buffer;
    chunked_transfer_encoding(client, data_to_process);
    if (!data_to_process.empty())
    {
        stor_data += data_to_process;
    }
    return stor_data;
}

bool complete_chunked_data(ChunkedClientInfo &client, std::string data)
{
    size_t chunk_size_end = data.find("0\r\n");
    if (chunk_size_end != std::string::npos)
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

bool transfer_encoding_chunked(int fd, ChunkedClientInfo &client)
{
    char buffer[CHUNK_SIZE];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

    if (bytes_read > 0)
    {
        client.last_active = time(NULL);
        // CRITICAL: Use size-based constructor to handle binary data with null bytes
        std::string data(buffer, bytes_read);

        if (client.filename.empty() && !client.boundary.empty())
        {
            // CRITICAL: Use size-based constructor for binary data
            std::string data_to_process(buffer, bytes_read);

            chunked_transfer_encoding(client, data_to_process);
            process_multipart_data(client, data_to_process);
            return (client.upload_state == 2);
        }
        data = get_buffer(data, bytes_read, client.bytes_chunked, client);
        if (check_body_boundary(client, data))
            return true;
        write_body_to_file(client, data.c_str(), data.size());
        client.bytes_read += bytes_read;
        if (client.bytes_read > client.request_obj.server.client_max_body_size)
        {
            // we romove file and  reject the request
            std::remove(client.filename.c_str());
            client.upload_state = 2;
            client.request_obj.path = client.request_obj.server.error_pages[413];
            return true;
        }
        return false;
    }
    else if (bytes_read == 0)
    {
        std::cout << "Connection closed by client" << std::endl;
        if (client.file_stream.is_open())
            client.file_stream.close();
        client.is_active = false;
        return false;
    }
    else if (bytes_read < 0)
    {
        perror("Read body chunk failed");
        if (client.file_stream.is_open())
            client.file_stream.close();
        client.is_active = false;
        return false;
    }

    return false;
}

bool read_body_chunk(int fd, ChunkedClientInfo &client)
{
    if (client.transfer_encod == "chunked")
    {
        return transfer_encoding_chunked(fd, client);
    }
    char buffer[CHUNK_SIZE];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

    if (bytes_read > 0)
    {
        client.last_active = time(NULL);
        std::string data(buffer, bytes_read);

        if (client.filename.empty() && !client.boundary.empty())
        {
            process_multipart_data(client, data);
            client.bytes_read += bytes_read;
            return (client.upload_state == 2);
        }

        if (check_body_boundary(client, data))
            return true;

        write_body_to_file(client, buffer, bytes_read);
        client.bytes_read += bytes_read;

        if (check_upload_complete(client, fd))
            return true;
        show_upload_progress(client);
        return false;
    }
    else if (bytes_read == 0)
    {
        std::cout << "Connection closed by client" << std::endl;
        if (client.file_stream.is_open())
            client.file_stream.close();
        client.is_active = false;
        return false;
    }
    else if (bytes_read < 0)
    {
        perror("Read body chunk failed");
        if (client.file_stream.is_open())
            client.file_stream.close();
        client.is_active = false;
        return false;
    }

    return false;
}

// Handle request using state machine
void handle_request_chunked(int fd, ChunkedClientInfo &client, std::vector<Request> &global_obj,
                            std::map<std::string, std::vector<size_t> > &hostport_to_indexes,
                            size_t client_server_idx)
{
    switch (client.upload_state)
    {
    case 0: // Reading headers
        if (read_headers_chunked(fd, client, global_obj, hostport_to_indexes, client_server_idx))
        {
            std::cout << "2222========================== : " << client.request_obj.epfd << std::endl;
            if (process_request_headers(client))
            {
                if (client.upload_state == 2)
                {
                    send_response(fd, client);
                    if (client.upload_state != 3)
                        client.is_active = false;
                }
            }
            else
            {
                sendErrorResponse(fd, 400, "Bad Request", client.request_obj.server.error_pages[400]);
                client.is_active = false;
            }
        }
        break;

    case 1: // Reading body
        if (read_body_chunk(fd, client))
        {
            send_response(fd, client);
            if (client.upload_state != 3)
                client.is_active = false;
            std::cout << "======> " << client.upload_state << std::endl;
        }
        break;

    case 3:
        std::cout << "kkkkkkkkk " << std::endl;
        if (is_cgi_request(client.request_obj.path))
        {
            std::cout << "--------------------------------------------- --------- \n";
            client.last_active = time(NULL);
            if (!client.request_obj.cgj_path.empty())
            {
                handle_cgi_request(client, fd, client.parsed_headers);
                return;
            }
        }
        else
        {
            client.is_active = false;
            break;
        }
    case 2: // Done
        client.is_active = false;
        break;
    }
}