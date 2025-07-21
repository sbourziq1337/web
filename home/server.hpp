#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <ctime>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 12000
#define PORT 8080
#define MAX_EVENTS 1000
#define CHUNK_SIZE 12000 // 300B chunks
class Request;           // Forward declaration
struct LocationConfig
{
    std::string path;
    std::vector<std::string> methods;
    bool autoindex;
    std::string root;
    std::string redirection;
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
    ssize_t client_max_body_size;
    std::vector<LocationConfig> locations;
};

class Request
{
public:
    std::string mthod;
    std::string path;
    std::string version;
    std::string root;
    size_t size1;
    int listen_fd; // File descriptor for listening socket
    std::string info_body;
    std::ofstream all_body;
    std::map<std::string, std::string> mimitype;
    std::map<std::string, std::string> post_res;
    std::vector<LocationConfig> local_data;
    std::string test_path;
    std::string uri;
    int server_port;
    std::vector<ServerConfig> same_name_servers;
    std::string _server_name_config;
    std::string server_host;
    std::string post_path;
    std::string response_red;
    ServerConfig server;
    ServerConfig *server_config;
    ssize_t content_ch;
    bool found_redirection;
    std::string cgj_path;
    int epfd ;
    std::vector<ServerConfig> server_configs;
    int fd_client; // File descriptor for client connection
    Request() : fd_client(-1) {}

    Request(const Request &other)
        : mthod(other.mthod), fd_client(other.fd_client), path(other.path), version(other.version),
          root(other.root), size1(other.size1), info_body(other.info_body),
          mimitype(other.mimitype), post_res(other.post_res),
          local_data(other.local_data), test_path(other.test_path),
          uri(other.uri), post_path(other.post_path), server_configs(other.server_configs)
    {
        // all_body is not copied as std::ofstream is not copyable
    }

    Request &operator=(const Request &other)
    {
        if (this != &other)
        {
            mthod = other.mthod;
            path = other.path;
            version = other.version;
            root = other.root;
            size1 = other.size1;
            info_body = other.info_body;
            mimitype = other.mimitype;
            post_res = other.post_res;
            local_data = other.local_data;
            test_path = other.test_path;
            uri = other.uri;
            post_path = other.post_path;
            server_configs = other.server_configs;
            // Do NOT assign all_body
        }
        return *this;
    }
};
class ChunkedClientInfo
{
public:
    bool is_active;
    std::string cgi_headrs;

    time_t last_active;
    int upload_state; // 0=reading headers, 1=reading body, 2=done
    ssize_t content_length;
    ssize_t bytes_read;
    ssize_t bytes_chunked; // For chunked transfer encoding
    std::string transfer_encod;
    std::string partial_data;
    std::string temp_buffer;
    int flag; // For multipart/form-data processing
    std::string headers;
    std::ofstream file_stream; // non-copyable
    std::string filename;
    std::string boundary;
    std::string chunk_buffer;
    size_t server_index;
    Request request_obj;
    std::map<std::string, std::string> parsed_headers;
    bool headers_complete;

    // C++98 compatible default constructor
    ChunkedClientInfo()
        : is_active(true),
        cgi_headrs(""),

          last_active(0),
          upload_state(0),
          content_length(-1),
          bytes_read(0),
          bytes_chunked(0),
          transfer_encod(""),
          partial_data(""),
          temp_buffer(""),
          flag(0),
          headers(""),
          filename(""),
          boundary(""),
          chunk_buffer(""),
          server_index(SIZE_MAX),
          request_obj(),
          parsed_headers(),
          headers_complete(false)
    {
    }

    // Copy constructor
    ChunkedClientInfo(const ChunkedClientInfo &other)
        : is_active(other.is_active),
          last_active(other.last_active),
          upload_state(other.upload_state),
          content_length(other.content_length),
          bytes_read(other.bytes_read),
          bytes_chunked(other.bytes_chunked),
          transfer_encod(other.transfer_encod),
          partial_data(other.partial_data),
          cgi_headrs(other.cgi_headrs),
          temp_buffer(other.temp_buffer),
          flag(other.flag),
          headers(other.headers),
          filename(other.filename),
          boundary(other.boundary),
          chunk_buffer(other.chunk_buffer),
          server_index(other.server_index),
          request_obj(other.request_obj),
          parsed_headers(other.parsed_headers),
          headers_complete(other.headers_complete)
    {
        // file_stream is not copyable, so we don't copy it
    }

    // Assignment operator
    ChunkedClientInfo &operator=(const ChunkedClientInfo &other)
    {
        if (this != &other)
        {
            is_active = other.is_active;
            last_active = other.last_active;
            upload_state = other.upload_state;
            content_length = other.content_length;
            bytes_read = other.bytes_read;
            bytes_chunked = other.bytes_chunked;
            transfer_encod = other.transfer_encod;
            partial_data = other.partial_data;
            temp_buffer = other.temp_buffer;
            flag = other.flag;
            headers = other.headers;
            // file_stream is not assigned
            filename = other.filename;
            boundary = other.boundary;
            chunk_buffer = other.chunk_buffer;
            server_index = other.server_index;
            request_obj = other.request_obj;
            parsed_headers = other.parsed_headers;
            headers_complete = other.headers_complete;
        }
        return *this;
    }
};
void process_multipart_data(ChunkedClientInfo &client, const std::string &data);
void parsing_method(Request &rec, const std::string &line);
void handle_directory_request(const std::string &path, const std::string &uri, int &fd, Request &obj, const std::string &type);
void parsing_Get(std::map<std::string, std::string> head, std::string path, int &fd, std::string type, std::string uri, Request &obj);
void ft_error(const char *msg);
void response(std::string name_file, int fd, std::string header);
long getFileSize(const std::string &filename);
bool hasEnding(const std::string &fullString, const std::string &ending);
std::string getContentType(const std::string &filename);
bool is_file(const std::string &path);
bool is_directory(const std::string &path);
std::string remove_slash(std::string path);
std::string urlDecode(const std::string &str);
std::string Format_urlencoded(std::string path, std::map<std::string, std::string> &post_res,
                              std::map<std::string, std::string> &form_data, Request &obj);
void all_type(std::map<std::string, std::string> &mimitype);
std::vector<ServerConfig> check_configfile();
void serve_not_found(int &fd);
void response_post(std::string name_file, int fd, std::string header);
std::string handle_authentication(const std::string &path, const std::string &username,
                                  const std::string &password, std::map<std::string, std::string> &post_res);
std::string remove_first_slash(const std::string &path);
bool parseRangeHeader(const std::string &rangeHeader, long fileSize, long &start, long &end);
bool sendDataReliably(int fd, const char *data, size_t size);
void sendChunk(int fd, const char *data, size_t size);
void response_plus(std::string name_file, int fd, std::string header, std::map<std::string, std::string> &headers);
void make_nonblocking(int fd);
int create_socket();
void setup_server_address(sockaddr_in &serv_add, int port);
void bind_and_listen(int socket_fd, sockaddr_in &serv_add);
int setup_server_socket(Request &global_obj);
int accept_client(int socket_fd);
std::string normalize_path(const std::string &path);
std::string remove_last_path_component(std::string path);
void parse_headers(std::istringstream &stream, std::map<std::string, std::string> &headers);
bool parse_method_line(ChunkedClientInfo &client);
void extract_content_length(ChunkedClientInfo &client);
bool check_headers_complete(ChunkedClientInfo &client);
// bool read_headers_chunked(int fd, ChunkedClientInfo &client);
std::string extract_boundary(const std::string &content_type);
bool process_multipart_request(ChunkedClientInfo &client, const std::string &content_type);
bool process_urlencoded_request(ChunkedClientInfo &client);
int accept_new_client(int socket_fd);
// bool add_client_to_epoll(int epfd, int client_fd, std::map<int, ChunkedClientInfo> &clients,
//                          const Request &global_obj);
// void handle_new_connections(int socket_fd, int epfd, std::map<int, ChunkedClientInfo> &clients,
//                              const Request &global_obj);
bool should_cleanup_client(const ChunkedClientInfo &client);
void cleanup_client(int fd, ChunkedClientInfo &client);
void cleanup_inactive_clients(int epfd, std::map<int, ChunkedClientInfo> &clients);
int setup_epoll(int socket_fd);
bool initialize_server_config(std::vector<Request> &global_obj);
std::string extract_filename(const std::string &data);
bool open_file_for_writing(ChunkedClientInfo &client, const std::string &filename);
bool write_file_data(ChunkedClientInfo &client, const std::string &data);
void process_multipart_data(ChunkedClientInfo &client, const std::string &data);
bool check_body_boundary(ChunkedClientInfo &client, const std::string &data);
void write_body_to_file(ChunkedClientInfo &client, const char *buffer, ssize_t bytes_read);
bool check_upload_complete(ChunkedClientInfo &client, int &fd_socket);
void show_upload_progress(const ChunkedClientInfo &client);
void send_response(int fd, ChunkedClientInfo &client);
// void handle_request_chunked(int fd, ChunkedClientInfo &client, Request &global_obj);
bool process_request_headers(ChunkedClientInfo &client);
bool read_body_chunk(int fd, ChunkedClientInfo &client);
bool process_post_request(ChunkedClientInfo &client);
void response(std::string name_file, int fd, std::string header);
void chunked_transfer_encoding(ChunkedClientInfo &client, std::string &data);
void handle_new_connections(int socket_fd, int epfd, std::map<int, ChunkedClientInfo> &clients,
                            const Request &global_obj, size_t server_index);
bool initialize_server_config(std::vector<Request> &global_obj,
                              std::map<std::string, std::vector<size_t> > &hostport_to_indexes);
void handle_request_chunked(int fd, ChunkedClientInfo &client, std::vector<Request> &global_obj,
                            std::map<std::string, std::vector<size_t> > &hostport_to_indexes,
                            size_t client_server_idx);
bool read_headers_chunked(int fd,
                          ChunkedClientInfo &client,
                          std::vector<Request> &global_obj,
                          const std::map<std::string, std::vector<size_t> > &hostport_to_indexes, size_t client_server_idx);
void sendErrorResponse(int fd, int error_code, const std::string &error_message, std::string path_file);
void handle_cgi_request(ChunkedClientInfo &client, int new_socket, std::map<std::string, std::string> &headers);
bool is_cgi_request(const std::string &path);