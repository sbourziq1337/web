SRC = server.cpp Request.cpp get_method.cpp post_method.cpp conf.cpp chunck_request.cpp setup_server.cpp \
	parse_headers.cpp epoll_manager_client.cpp http_chunked_handler.cpp http_body_processing.cpp cgi.cpp
cpp= c++ -g3

CFLAGS = -std=c++98 

RM = rm -rf

OBJ = $(SRC:.cpp=.o)

TARGET = server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(cpp) -o $(TARGET) $(OBJ)

%.o: %.cpp
	$(cpp) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ)
fclean: clean
	$(RM) $(TARGET)
re: clean all

.PHONY: all re clean fclean