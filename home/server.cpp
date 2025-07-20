#include "server.hpp"
// Process request headers based on method
bool process_request_headers(ChunkedClientInfo &client)
{
    std::string index_path = client.request_obj.uri;
    bool found_method = false;
    bool found_local = false;
    // check for redirection
    if (client.request_obj.found_redirection == false)
    {
        std::string red_path = normalize_path(client.request_obj.uri);
        while (true)
        {
            for (size_t i = 0; i < client.request_obj.local_data.size(); ++i)
            {
                std::string location_path = normalize_path(client.request_obj.local_data[i].path);

                if (red_path == location_path && !client.request_obj.local_data[i].redirection.empty())
                {
                    client.request_obj.response_red = "HTTP/1.1 302 Found\r\n";
                    client.request_obj.response_red += "Location: " + client.request_obj.local_data[i].redirection + "\r\n";
                    client.request_obj.response_red += "Content-Length: 0\r\n";
                    client.request_obj.response_red += "Connection: close\r\n\r\n";

                    client.upload_state = 2;
                    client.request_obj.found_redirection = true;
                    client.request_obj.mthod = "Redirection";
                    return true;
                }
            }

            if (red_path.empty() || red_path == "/" || red_path == normalize_path(client.request_obj.root))
                break;
            red_path = remove_last_path_component(red_path);
        }
    }
    // handle methods GET, POST, etc.
    while (true)
    {
        for (size_t i = 0; i < client.request_obj.local_data.size(); i++)
        {
            std::string location_path = normalize_path(client.request_obj.local_data[i].path);
            index_path = normalize_path(index_path);
            if (index_path == location_path)
            {
                found_local = true;
                if (client.request_obj.local_data[i].methods.empty())
                {
                    found_method = true;
                    break;
                }
                for (size_t j = 0; j < client.request_obj.local_data[i].methods.size(); j++)
                {
                    if (client.request_obj.mthod == client.request_obj.local_data[i].methods[j])
                        found_method = true;
                }
                if (found_method)
                    break;
            }
        }
        if (found_method == true || found_local == true || index_path == "/" )
            break;
        index_path = remove_last_path_component(index_path);
    }
    if (found_method == false)
    {
        client.upload_state = 2;
        client.request_obj.mthod = "method not found";
        return true;
    }
    if ((client.request_obj.mthod == "POST"))
    {
        if (client.request_obj.server.client_max_body_size <= 0 || client.request_obj.server.client_max_body_size <= client.content_length)
        {
            client.request_obj.mthod = "content_length";
            client.upload_state = 2;
            return true;
        }
    }
    if (client.request_obj.mthod == "GET")
    {
        client.upload_state = 2;
        return true;
    }
    else if (client.request_obj.mthod == "DELETE")
    {
        client.upload_state = 2;
        return true;
    }
    else if (client.request_obj.mthod == "POST")
    {
        if (process_post_request(client))
        {
            if (client.upload_state != 2)
            {
                client.upload_state = 1;
                client.bytes_read = 0;
            }
            return true;
        }
        return false;
    }
    else
    {
        client.upload_state = 2;
        return true;
    }
}

struct ServerInfo
{
    int socket_fd;
    size_t config_index;

    ServerInfo() : socket_fd(-1), config_index(0) {}
    ServerInfo(int fd, size_t idx) : socket_fd(fd), config_index(idx) {}
};

void setup_all_sockets(std::vector<ServerInfo> &servers, std::vector<Request> &global_obj,
                       std::map<std::string, std::vector<size_t> > &hostport_to_indexes,
                       std::map<int, size_t> &socket_fd_to_default_index)
{
    for (std::map<std::string, std::vector<size_t> >::iterator it = hostport_to_indexes.begin(); it != hostport_to_indexes.end(); ++it)
    {
        const std::vector<size_t> &indexes = it->second;
        if (indexes.empty())
            continue;

        // Use the first config as default
        Request &default_request = global_obj[indexes[0]];

        int socket_fd = setup_server_socket(default_request);
        if (socket_fd == -1)
        {
            std::cerr << "Failed to set up socket for: " << it->first << std::endl;
            continue;
        }
        servers.push_back(ServerInfo(socket_fd, indexes[0]));

        socket_fd_to_default_index[socket_fd] = indexes[0];
    }
}
// Main server loop
int main()
{
    std::map<std::string, std::vector<size_t> > hostport_to_indexes;
    std::map<int, size_t> socket_fd_to_default_index;
    std::vector<Request> global_obj;

    if (!initialize_server_config(global_obj, hostport_to_indexes))
    {
        std::cerr << "Failed to initialize server configuration." << std::endl;
        return 1;
    }
    // Create all server sockets
    std::vector<ServerInfo> servers;
    // Reserve space for server sockets based on the number of configurations
    servers.reserve(global_obj.size());

    setup_all_sockets(servers, global_obj, hostport_to_indexes, socket_fd_to_default_index);
    // Create a single epoll instance for all servers
    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        perror("epoll_create1 failed");
        // Cleanup sockets
        for (size_t i = 0; i < servers.size(); ++i)
        {
            close(servers[i].socket_fd);
        }
        return 1;
    }

    // Add all server sockets to epoll
    for (size_t i = 0; i < servers.size(); ++i)
    {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = servers[i].socket_fd;
        //  std::cout << "Adding server socket " << servers[i].socket_fd << " to epoll" << std::endl;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, servers[i].socket_fd, &ev) == -1)
        {
            perror("epoll_ctl: server socket");
            close(epfd);
            for (size_t j = 0; j < servers.size(); ++j)
            {
                close(servers[j].socket_fd);
            }
            return 1;
        }
    }

    std::map<int, ChunkedClientInfo> clients;

    while (true)
    {
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 2000);

        if (nfds < 0)
        {
            perror("epoll_wait failed");
            continue;
        }

        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            // Check if this is a server socket (new connection)
            bool is_server_socket = false;
            size_t server_idx = SIZE_MAX;

            // Find which server socket this is
            for (size_t j = 0; j < servers.size(); j++)
            {
                if (servers[j].socket_fd == fd)
                {
                    is_server_socket = true;
                    server_idx = servers[j].config_index;
                    break;
                }
            }

            if (is_server_socket && (events[i].events & EPOLLIN))
            {
                // Handle new connection for this specific server
                handle_new_connections(fd, epfd, clients, global_obj[server_idx], server_idx);
            }
            else if (events[i].events & EPOLLIN)
            {
                // Handle existing client connection
                std::map<int, ChunkedClientInfo>::iterator client_it = clients.find(fd);
                if (client_it != clients.end() && client_it->second.is_active)
                {
                    size_t client_server_idx = client_it->second.server_index;
                    // Validate server index
                    if (client_server_idx < global_obj.size())
                    {
                        global_obj[client_server_idx].fd_client = fd;
                        // handle_request_chunked(fd, client_it->second, global_obj[client_server_idx]);
                        handle_request_chunked(fd, client_it->second, global_obj,
                                               hostport_to_indexes, client_server_idx);
                    }
                    else
                    {
                        // Clean up invalid client
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        clients.erase(client_it);
                    }
                }
            }
        }

        cleanup_inactive_clients(epfd, clients);
    }

    // Cleanup
    close(epfd);
    for (size_t i = 0; i < servers.size(); ++i)
    {
        close(servers[i].socket_fd);
    }

    return 0;
}