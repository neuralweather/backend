// Copyright (C) 2021 Lennard Walter
// License: MIT

#ifndef __HTTP_H
#define __HTTP_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

// max size for everything except the body as it is written from a user provided buffer
#define HTTP_MAX_RESPONSE_HEAD_SIZE 1024
// max size for a http request (includes body)
#define HTTP_MAX_REQUEST_SIZE 1024

#define HTTP_EXPECT(expr, s, ...)                                                        \
    if (!(expr)) {                                                                       \
        fprintf(stderr, "\033[31mERROR\033[0m " s ": %s\n", ##__VA_ARGS__,               \
                strerror(errno));                                                        \
        exit(EXIT_FAILURE);                                                              \
    }

#define HTTP_ERROR(s, ...)                                                               \
    fprintf(stderr, "\033[31mERROR\033[0m " s "\n", ##__VA_ARGS__), exit(EXIT_FAILURE);
#define HTTP_WARN(s, ...) fprintf(stderr, "\033[33mWARN\033[0m " s "\n", ##__VA_ARGS__);
#define HTTP_INFO(s, ...) fprintf(stderr, "\033[32mINFO\033[0m " s "\n", ##__VA_ARGS__);
#define HTTP_DEBUG(s, ...) fprintf(stderr, "\033[34mDEBUG\033[0m " s "\n", ##__VA_ARGS__);

typedef struct http_server http_server_t;
typedef struct http_handler http_handler_t;
typedef struct http_request http_request_t;
typedef struct http_query_param http_query_param_t;
typedef struct http_response http_response_t;
typedef struct http_header http_header_t;
typedef struct http_thread_args http_thread_args_t;
typedef enum http_method http_method_t;
typedef enum http_status http_status_t;
LIST_DEF(http_handler_t*, http_handlers_t);
LIST_DEF(http_header_t*, http_headers_t);
LIST_DEF(http_query_param_t*, http_query_params_t);

enum http_method {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST = 1,
};

enum http_status {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
};

struct http_server {
    http_handlers_t* handlers;
};

typedef http_response_t* (*http_handler_callback_t)(http_request_t*);

struct http_handler {
    http_handler_callback_t callback;
    char* path;
};

struct http_request {
    http_method_t method;
    char* path;
    http_query_params_t* query_params;
    http_headers_t* headers;
    char* body;
    size_t body_size;
};

struct http_query_param {
    char* name;
    char* value;
};

struct http_response {
    http_status_t status;
    http_headers_t* headers;
    char* body;
    size_t body_size;
};

struct http_header {
    char* name;
    char* value;
};

struct http_thread_args {
    http_server_t* server;
    int sock_fd;
};

http_server_t* http_server_new();
void http_server_free(http_server_t* server);

void http_server_add_handler(http_server_t* server, char* path,
                             http_handler_callback_t callback);
void http_server_handle_connection(http_thread_args_t* args);
void http_server_send_response(http_server_t* server, http_response_t* response,
                               int sock_fd);
void http_server_run(http_server_t* server, char* address, uint16_t port);

http_request_t* http_request_new(http_method_t method, char* path,
                                 http_query_params_t* query_params,
                                 http_headers_t* headers, char* body, size_t body_size);
http_request_t* http_request_parse(char* buffer, size_t size);
void http_request_free(http_request_t* request);
void http_request_print(http_request_t* request);

http_query_param_t* http_query_param_new(char* name, char* value);
void http_query_param_free(http_query_param_t* param);

http_query_params_t* http_query_params_new();
void http_query_params_free(http_query_params_t* params);
void http_query_params_add(http_query_params_t* params, http_query_param_t* param);
http_query_param_t* http_query_params_get(http_query_params_t* params, char* name);

http_response_t* http_response_new(http_status_t status, http_headers_t* headers,
                                   char* body, size_t body_size);
void http_response_free(http_response_t* response);
size_t http_response_head_to_buffer(http_response_t* response, char* buffer, size_t size);

http_header_t* http_header_new(char* name, char* value);
void http_header_free(http_header_t* header);

http_headers_t* http_headers_new();
void http_headers_free(http_headers_t* headers);
void http_headers_add(http_headers_t* headers, http_header_t* header);
http_header_t* http_headers_get(http_headers_t* headers, char* name);

http_handler_t* http_handler_new(char* path, http_handler_callback_t callback);
void http_handler_free(http_handler_t* handler);

http_thread_args_t* http_thread_args_new(http_server_t* server, int sock_fd);
void http_thread_args_free(http_thread_args_t* thread_args);

char* http_status_to_string(http_status_t status);

// evil macro magic for the HTTP_HEADER macro below
// don't even try to read, just use it like this:
// http_headers_t* headers = HTTP_HEADERS(
//     ("Content-Type", "text/plain"),
//     ("Content-Length", "42")
// );
// the variadic HTTP_RESPONSE macro is defined here as well, usage:
// HTTP_RESPONSE("success") -> status 200, no headers, body length = strlen("success")
// HTTP_RESPONSE("success", HTTP_STATUS_OK) -> status 200, no headers, body length =
//                                             strlen("success")
// HTTP_RESPONSE("success", HTTP_STATUS_OK, HTTP_HEADERS(("Content-Type", "text/plain")))

#define _HTTP_EVAL0(...) __VA_ARGS__
#define _HTTP_EVAL1(...) _HTTP_EVAL0(_HTTP_EVAL0(_HTTP_EVAL0(__VA_ARGS__)))
#define _HTTP_EVAL2(...) _HTTP_EVAL1(_HTTP_EVAL1(_HTTP_EVAL1(__VA_ARGS__)))
#define _HTTP_EVAL3(...) _HTTP_EVAL2(_HTTP_EVAL2(_HTTP_EVAL2(__VA_ARGS__)))
#define _HTTP_EVAL4(...) _HTTP_EVAL3(_HTTP_EVAL3(_HTTP_EVAL3(__VA_ARGS__)))
#define _HTTP_EVAL(...) _HTTP_EVAL4(_HTTP_EVAL4(_HTTP_EVAL4(__VA_ARGS__)))

#define _HTTP_MAP_END(...)
#define _HTTP_MAP_OUT

#define _HTTP_MAP_GET_END2() 0, _HTTP_MAP_END
#define _HTTP_MAP_GET_END1(...) _HTTP_MAP_GET_END2
#define _HTTP_MAP_GET_END(...) _HTTP_MAP_GET_END1
#define _HTTP_MAP_NEXT0(test, next, ...) next _HTTP_MAP_OUT
#define _HTTP_MAP_NEXT1(test, next) _HTTP_MAP_NEXT0(test, next, 0)
#define _HTTP_MAP_NEXT(test, next) _HTTP_MAP_NEXT1(_HTTP_MAP_GET_END test, next)

#define _HTTP_MAP0(f, x, peek, ...)                                                      \
    f(x) _HTTP_MAP_NEXT(peek, _HTTP_MAP1)(f, peek, __VA_ARGS__)
#define _HTTP_MAP1(f, x, peek, ...)                                                      \
    f(x) _HTTP_MAP_NEXT(peek, _HTTP_MAP0)(f, peek, __VA_ARGS__)

#define _HTTP_MAP(f, ...)                                                                \
    _HTTP_EVAL(_HTTP_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

#define _HTTP_HEADERS_UNPACK(k, v) k, v
#define _HTTP_HEADERS_ADD(keyval) http_headers_add(_http_headers, http_header_new keyval);

#define HTTP_HEADERS(...)                                                                \
    ({                                                                                   \
        http_headers_t* _http_headers = http_headers_new();                              \
        _HTTP_MAP(_HTTP_HEADERS_ADD, __VA_ARGS__)                                        \
        _http_headers;                                                                   \
    })

#define _HTTP_RESPONSE1(body) http_response_new(HTTP_STATUS_OK, NULL, body, strlen(body))
#define _HTTP_RESPONSE2(body, status) http_response_new(status, NULL, body, strlen(body))
#define _HTTP_RESPONSE3(body, status, headers)                                           \
    http_response_new(status, headers, body, strlen(body))
#define _HTTP_RESPONSE4(body, status, headers, body_size)                                \
    http_response_new(status, headers, body, body_size)

#define _HTTP_GET_RESPONSE(_1, _2, _3, _4, NAME, ...) NAME

#define HTTP_RESPONSE(...)                                                               \
    _HTTP_GET_RESPONSE(__VA_ARGS__, _HTTP_RESPONSE4, _HTTP_RESPONSE3, _HTTP_RESPONSE2,   \
                       _HTTP_RESPONSE1)                                                  \
    (__VA_ARGS__)

#endif // __HTTP_H
