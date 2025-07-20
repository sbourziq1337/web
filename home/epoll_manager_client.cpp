#include "server.hpp"

// Accept new client connection
int accept_new_client(int socket_fd)
{
    while (true)
    {
        int new_socket = accept_client(socket_fd);
        if (new_socket < 0)
            return -1;

        make_nonblocking(new_socket);
        return new_socket;
    }
}

// Add client to epoll with server index
bool add_client_to_epoll(int epfd, int client_fd, std::map<int, ChunkedClientInfo> &clients,
                         const Request &global_obj, size_t server_index)
{
    struct epoll_event client_event;
    client_event.events = EPOLLIN;
    client_event.data.fd = client_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_event) == -1)
    {
        perror("epoll_ctl: add client socket");
        close(client_fd);
        return false;
    }

    ChunkedClientInfo new_client;
    new_client.is_active = true;
    new_client.last_active = time(NULL);
    new_client.upload_state = 0;
    new_client.content_length = -1;
    new_client.bytes_read = 0;
    new_client.headers_complete = false;
    new_client.request_obj = global_obj;
    new_client.server_index = server_index;  // Store server association

    clients[client_fd] = new_client;
    std::cout << "New client " << client_fd << " connected to server " << server_index 
              << " (port " << global_obj.server.port << ")" << std::endl;
    return true;
}

// Handle new connections with server index
void handle_new_connections(int socket_fd, int epfd, std::map<int, ChunkedClientInfo> &clients,
                            const Request &global_obj, size_t server_index)
{
    while (true)
    {
        int new_socket = accept_new_client(socket_fd);
        if (new_socket < 0)
            break;

        if (!add_client_to_epoll(epfd, new_socket, clients, global_obj, server_index))
        {
            break;
        }
    }
}

// Clean up client resources
void cleanup_client(int fd, ChunkedClientInfo &client)
{
    if (client.file_stream.is_open())
    {
        client.file_stream.close();
        if (!client.filename.empty() && client.bytes_read < client.content_length)
        {
            std::remove(client.filename.c_str());
            std::cout << "Deleted incomplete file: " << client.filename << std::endl;
        }
    }
}

// Check if client should be cleaned up
bool should_cleanup_client(const ChunkedClientInfo &client)
{
    return !client.is_active || (time(NULL) - client.last_active > 60);
}

// Fixed cleanup function with proper debugging
void cleanup_inactive_clients(int epfd, std::map<int, ChunkedClientInfo> &clients)
{
    std::vector<int> clients_to_remove; // Collect clients to remove first
    
    for (std::map<int, ChunkedClientInfo>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        int fd = it->first;
        ChunkedClientInfo &client = it->second;

        if (should_cleanup_client(client))
        {
            std::cout << "DEBUG: Marking client " << fd << " for cleanup (server_index: " 
                      << client.server_index << ", is_active: " << client.is_active << ")" << std::endl;
            clients_to_remove.push_back(fd);
        }
    }
    
    // Now actually remove the clients
    for (std::vector<int>::iterator it = clients_to_remove.begin(); it != clients_to_remove.end(); ++it)
    {
        int fd = *it;
        std::map<int, ChunkedClientInfo>::iterator client_it = clients.find(fd);
        if (client_it != clients.end())
        {
            std::cout << "Cleaning up client " << fd << " from server " << client_it->second.server_index << std::endl;
            cleanup_client(fd, client_it->second);
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            clients.erase(client_it);
        }
    }
}

// Set up epoll (this function is no longer needed in the multi-server setup)
// But keeping it for backward compatibility if needed elsewhere
int setup_epoll(int socket_fd)
{
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1)
    {
        perror("epoll_create1 failed");
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = socket_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &ev) == -1)
    {
        perror("epoll_ctl: add server socket");
        close(epfd);
        return -1;
    }

    return epfd;
}