NAME = servio


CC = c++

PREFIX_FOLDER = $(shell pwd)

CFLAGS = -Wall -Wextra -Werror -ggdb -std=c++98 -D PREFIX_FOLDER=\"$(PREFIX_FOLDER)\"




# core files
CORE_SRC = servio.cpp\
	sio_config.cpp\
	sio_cmdline_opts.cpp\
	sio_lexer.cpp\
	sio_ast.cpp\
	sio_parser.cpp\

# core headers
CORE_HEADER = servio.hpp\
	sio_config.hpp\
	sio_cmdline_opts.hpp\
	sio_lexer.hpp\
	sio_ast.hpp\
	sio_parser.hpp\

# http files
HTTP_SRC = sio_request.cpp\
	sio_request_body.cpp\
	sio_response.cpp\
	sio_http_codes.cpp\
	sio_client.cpp\
	sio_mime_types.cpp\
	sio_http_range.cpp\
	sio_cgi.cpp\
	sio_header.cpp\
	sio_boundary.cpp\
	sio_streamsearch.cpp

# http headers
HTTP_HEADER = sio_request.hpp\
	sio_request_body.hpp\
	sio_response.hpp\
	sio_http_codes.hpp\
	sio_client.hpp\
	sio_mime_types.hpp\
	sio_http_range.hpp\
	sio_cgi.hpp\
	sio_header.hpp\
	sio_boundary.hpp\
	sio_streamsearch.hpp

# utility files
UTILITY_SRC = sio_socket.cpp\
	sio_helpers.cpp\
	sio_utils.cpp\
	sio_logger.cpp\

# utility headers
UTILITY_HEADER = sio_socket.hpp\
	sio_helpers.hpp\
	sio_utils.hpp\
	sio_logger.hpp\

SRCS = sio_main.cpp\
	$(addprefix http/,$(HTTP_SRC))\
	$(addprefix core/, $(CORE_SRC))\
	$(addprefix utility/, $(UTILITY_SRC))

SRCS := $(addprefix src/, $(SRCS))

HEADERS = $(addprefix core/, $(CORE_HEADER))\
	$(addprefix  utility/, $(UTILITY_HEADER))\
	$(addprefix http/, $(HTTP_HEADER))

HEADERS := $(HEADERS:%=src/%)

OBJS = $(SRCS:%.cpp=%.o)

INCLUDES = core http utility

INCLUDES := -I src/\
	$(addprefix -I src/, $(INCLUDES))


all: $(NAME)

$(NAME): $(OBJS)
	$(CC) -o $(NAME) $^ -fsanitize=address

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -vf $(OBJS)

fclean: clean
	rm -vf $(NAME)

re: fclean all
