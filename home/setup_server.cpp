#include "server.hpp"
#include <netdb.h>      // getaddrinfo
#include <sys/socket.h> // AF_INET, SOCK_STREAM
#include <netinet/in.h> // sockaddr_in
#include <cstring>      // memset, strerror
#include <iostream>
#include <cstdlib> // freeaddrinfo
void make_nonblocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("fcntl - set nonblocking");
        return;
    }
}

int create_socket()
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        return -1;
    }

    make_nonblocking(socket_fd);
    return socket_fd;
}

bool setup_server_address(struct sockaddr_in &serv_addr, const std::string &ip, int port)
{
    struct addrinfo hints, *res = NULL;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4 only
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_NUMERICHOST; // Ensures 'ip' is a numeric IP (like 127.0.0.1), not a domain

    // Use getaddrinfo to resolve the IP
    int ret = getaddrinfo(ip.c_str(), NULL, &hints, &res);
    if (ret != 0 || !res)
    {
        std::cerr << "getaddrinfo error: " << gai_strerror(ret) << std::endl;
        return false;
    }

    struct sockaddr_in *addr_in = reinterpret_cast<sockaddr_in *>(res->ai_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = addr_in->sin_addr;

    freeaddrinfo(res);
    return true;
}

void bind_and_listen(int socket_fd, sockaddr_in &serv_add, Request &global_obj)
{
    if (bind(socket_fd, (struct sockaddr *)&serv_add, sizeof(serv_add)) < 0)
    {
        std::cerr << "[ERROR] Could not bind to " << global_obj.server.host << ":" << global_obj.server.port
                  << " — " << strerror(errno) << std::endl;
        return;
    }

    if (listen(socket_fd, 5) < 0)
    {
        std::cerr << "[ERROR] Could not listen on " << global_obj.server.host << ":" << global_obj.server.port
                  << " — " << strerror(errno) << std::endl;
    }
}

int setup_server_socket(Request &global_obj)
{
    sockaddr_in serv_add;

    std::string host = global_obj.server.host;
    if (host.empty() || host == "localhost")
        host = "0.0.0.0";
    // setup_server_address(serv_add, global_obj.server.port);
    if (!setup_server_address(serv_add, host, global_obj.server.port))
    {
        std::cerr << "Failed to set up server address for " << host << ":" << global_obj.server.port << std::endl;
        return -1; // setup failed
    }

    int socket_fd = create_socket();
    if (socket_fd < 0)
    {
        std::cerr << "Failed to create socket for " << host << ":" << global_obj.server.port << std::endl;
        return -1; // socket creation failed
    }
    bind_and_listen(socket_fd, serv_add, global_obj);

    return socket_fd;
}

int accept_client(int socket_fd)
{
    sockaddr_in cli_add;
    socklen_t cli_len = sizeof(cli_add);
    int new_socket = accept(socket_fd, (struct sockaddr *)&cli_add, &cli_len);
    if (new_socket < 0)
    {
        perror("Accept failed");
    }
    return new_socket;
}
// Initialize server configuration
bool initialize_server_config(std::vector<Request> &global_obj,
                              std::map<std::string, std::vector<size_t> > &hostport_to_indexes)
{
    std::vector<ServerConfig> all_servers = check_configfile();
    if (all_servers.empty())
    {
        std::cerr << "Error: No server configurations found" << std::endl;
        return false;
    }

    global_obj.resize(all_servers.size());

    for (size_t i = 0; i < all_servers.size(); ++i)
    {
        all_type(global_obj[i].mimitype);
        global_obj[i].server = all_servers[i];
        global_obj[i].local_data = all_servers[i].locations;
        global_obj[i].root = all_servers[i].root;

        std::ostringstream oss;
        oss << all_servers[i].host << ":" << all_servers[i].port;
        hostport_to_indexes[oss.str()].push_back(i);
        std::cout << "=======> " << oss.str() << std::endl;
    }
    return true;
}