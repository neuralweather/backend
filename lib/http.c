// Copyright (C) 2021 Lennard Walter
// License: MIT

#include "http.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

static char* HTTP_METHOD_STRINGS[] = {"GET", "POST"};

http_server_t* http_server_new() {
    http_server_t* server = malloc(sizeof(http_server_t));
    server->handlers = LIST_NEW(http_handlers_t);
    return server;
}

void http_server_free(http_server_t* server) {
    LIST_FREE(server->handlers);
    free(server);
}

void http_server_add_handler(http_server_t* server, char* path,
                             http_handler_callback_t callback) {
    http_handler_t* handler = http_handler_new(path, callback);
    LIST_APPEND(server->handlers, handler);
}

void http_server_handle_connection(http_thread_args_t* args) {
    http_server_t* server = args->server;
    int sock_fd = args->sock_fd;

    char* buffer = malloc(HTTP_MAX_REQUEST_SIZE + 1); // +1 for \0
    HTTP_EXPECT(buffer != NULL, "malloc()");

    // TODO: write a macro to handle error but don't kill like HTTP_ERROR
    ssize_t bytes_read = read(sock_fd, buffer, HTTP_MAX_REQUEST_SIZE);
    if (bytes_read == -1) {
        HTTP_DEBUG("read() failed: %s", strerror(errno));
        goto shared_cleanup;
    } else if (bytes_read == 0) {
        HTTP_DEBUG("read() returned 0");
        goto shared_cleanup;
    }

    HTTP_DEBUG("read() %zd bytes", bytes_read);

    http_request_t* request = http_request_parse(buffer, bytes_read);
    if (request == NULL) {
        HTTP_DEBUG("request parse failed");
        goto shared_cleanup;
    }

    http_handler_callback_t callback = NULL;

    LIST_FOREACH(server->handlers, handler) {
        if (strcmp(handler->path, request->path) == 0) {
            callback = handler->callback;
            goto after_loop;
        }
    }

after_loop:

    http_response_t* response;

    if (callback != NULL) {
        response = callback(request);
        if (response == NULL) {
            HTTP_DEBUG("route handler returned NULL");
            response = http_response_new(HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL,
                                         "Internal Server Error", 22);
        }
    } else {
        HTTP_DEBUG("no handler for path: %s", request->path);
        response = http_response_new(HTTP_STATUS_NOT_FOUND, NULL, "Not Found", 9);
    }

    http_server_send_response(server, response, sock_fd);

    HTTP_INFO("%s %s %d", HTTP_METHOD_STRINGS[request->method], request->path,
              response->status);

    http_response_free(response);

    http_request_free(request);

shared_cleanup:
    // TODO: write a macro to handle error but don't kill like HTTP_ERROR
    if (close(sock_fd) != 0) {
        HTTP_DEBUG("close() failed %s", strerror(errno));
    }

    http_thread_args_free(args);

    free(buffer);
}

void http_server_send_response(http_server_t* server, http_response_t* response,
                               int sock_fd) {
    char* buffer = malloc(HTTP_MAX_RESPONSE_HEAD_SIZE);
    HTTP_EXPECT(buffer != NULL, "malloc()");

    size_t response_size =
        http_response_head_to_buffer(response, buffer, HTTP_MAX_RESPONSE_HEAD_SIZE);

    ssize_t bytes_written = write(sock_fd, buffer, response_size);
    if (bytes_written == -1) {
        HTTP_WARN("write() failed");
    } else {
        HTTP_DEBUG("write() %zd bytes", bytes_written);
    }

    if (bytes_written != response_size) {
        HTTP_ERROR("write() failed to write all bytes");
    }

    if (response->body != NULL) {
        bytes_written = write(sock_fd, response->body, response->body_size);
        if (bytes_written == -1) {
            HTTP_WARN("write() failed");
        } else {
            HTTP_DEBUG("write() %zd bytes", bytes_written);
        }

        if (bytes_written != response->body_size) {
            HTTP_ERROR("write() failed to write all bytes");
        }
    }

    free(buffer);
}

void http_server_run(http_server_t* server, char* address, uint16_t port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    HTTP_EXPECT(sock_fd > 0, "socket()");

    int enable = 1;
    HTTP_EXPECT(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == 0,
                "setsockopt()");

    in_addr_t in_addr = inet_addr(address);
    HTTP_EXPECT(in_addr != INADDR_NONE, "inet_addr()");

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = in_addr,
    };

    HTTP_EXPECT(bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0, "bind()");

    HTTP_EXPECT(listen(sock_fd, 5) == 0, "listen()");
    HTTP_INFO("Listening on http://%s:%d", address, port);

    while (1) {
        int client_sock_fd = accept(sock_fd, NULL, NULL);
        HTTP_EXPECT(client_sock_fd > 0, "accept()");

        HTTP_DEBUG("Connection accepted, client_sock_fd = %d", client_sock_fd);

        http_thread_args_t* args = http_thread_args_new(server, client_sock_fd);

        pthread_t thread;
        HTTP_EXPECT(pthread_create(&thread, NULL,
                                   (void* (*)(void*))http_server_handle_connection,
                                   args) == 0,
                    "pthread_create()");

        HTTP_EXPECT(pthread_detach(thread) == 0, "pthread_detach()");
    }
}

http_request_t* http_request_new(http_method_t method, char* path,
                                 http_query_params_t* query_params,
                                 http_headers_t* headers, char* body, size_t body_size) {
    http_request_t* request = malloc(sizeof(http_request_t));
    request->method = method;
    request->path = path;
    request->query_params = query_params;
    request->headers = headers;
    request->body = body;
    request->body_size = body_size;
    return request;
}

http_request_t* http_request_parse(char* buffer, size_t size) {
    // null terminate so strtok() doesn't segfault on faulty http requests
    buffer[size] = '\0';

    char* saveptr;

    char* method_str = strtok_r(buffer, " ", &saveptr);
    if (method_str == NULL) {
        HTTP_DEBUG("request method is missing");
        return NULL;
    }

    http_method_t method;
    if (strcmp(method_str, "GET") == 0) {
        method = HTTP_METHOD_GET;
    } else if (strcmp(method_str, "POST") == 0) {
        method = HTTP_METHOD_POST;
    } else {
        HTTP_DEBUG("server only supports GET and POST, but got %s", method_str);
        return NULL;
    }

    char* full_path = strtok_r(NULL, " ", &saveptr);
    if (full_path == NULL) {
        HTTP_DEBUG("request path is missing");
        return NULL;
    }

    char* query_params = strchr(full_path, '?');
    if (query_params != NULL) {
        *query_params = '\0';
        query_params++;
    }

    http_query_params_t* query_params_obj = http_query_params_new();
    if (query_params != NULL) {
        char* query_params_saveptr;
        char* query_param = strtok_r(query_params, "&", &query_params_saveptr);
        while (query_param != NULL) {
            char* query_param_saveptr;
            char* name = strtok_r(query_param, "=", &query_param_saveptr);
            char* value = strtok_r(NULL, "=", &query_param_saveptr);
            if (name != NULL && value != NULL) {
                http_query_param_t* query_param_obj = http_query_param_new(name, value);
                http_query_params_add(query_params_obj, query_param_obj);
            }
            query_param = strtok_r(NULL, "&", &query_params_saveptr);
        }
    }

    char* path = full_path;

    char* version = strtok_r(NULL, "\r\n", &saveptr);
    if (version == NULL) {
        HTTP_DEBUG("request version is missing");
        return NULL;
    }

    if (strcmp(version, "HTTP/1.1") != 0) {
        HTTP_DEBUG("server only supports HTTP/1.1, but got %s", version);
        return NULL;
    }

    http_headers_t* headers = http_headers_new();

    char* header = strtok_r(NULL, "\r\n", &saveptr);
    while (header != NULL) {
        char* headers_saveptr;
        char* header_name = strtok_r(header, ": ", &headers_saveptr);
        if (header_name == NULL) {
            HTTP_DEBUG("header name is missing");
            return NULL;
        }

        char* header_value = strtok_r(NULL, "\r\n", &headers_saveptr);
        if (header_value == NULL) {
            HTTP_DEBUG("header value is missing");
            return NULL;
        }

        http_header_t* header_obj = http_header_new(header_name, header_value);
        LIST_APPEND(headers, header_obj);

        // for some reason, the first CR is removed???
        // but it works like this, soo....
        if (memcmp(saveptr, "\n\r\n", 3) == 0) {
            saveptr += 3;
            break;
        }

        header = strtok_r(NULL, "\r\n", &saveptr);
    }

    char* body = saveptr;
    size_t body_size = size - (body - buffer);

    http_request_t* request =
        http_request_new(method, path, query_params_obj, headers, body, body_size);
    return request;
}

void http_request_free(http_request_t* request) {
    http_headers_free(request->headers);
    http_query_params_free(request->query_params);

    free(request);
}

void http_request_print(http_request_t* request) {
    HTTP_DEBUG("request method: %s", "GET");
    HTTP_DEBUG("request path: %s", request->path);
    HTTP_DEBUG("request query params:")
    LIST_FOREACH(request->query_params, query_param) {
        HTTP_DEBUG("  %s=%s", query_param->name, query_param->value);
    }
    HTTP_DEBUG("request headers:");
    LIST_FOREACH(request->headers, header) {
        HTTP_DEBUG("  %s: %s", header->name, header->value);
    }
}

http_query_param_t* http_query_param_new(char* name, char* value) {
    http_query_param_t* param = malloc(sizeof(http_query_param_t));
    param->name = name;
    param->value = value;
    return param;
}

void http_query_param_free(http_query_param_t* param) {
    free(param);
}

http_query_params_t* http_query_params_new() {
    http_query_params_t* params = LIST_NEW(http_query_params_t);
    return params;
}

void http_query_params_free(http_query_params_t* params) {
    LIST_FOREACH(params, param) {
        http_query_param_free(param);
    }

    LIST_FREE(params);
}

void http_query_params_add(http_query_params_t* params, http_query_param_t* param) {
    LIST_APPEND(params, param);
}

http_query_param_t* http_query_params_get(http_query_params_t* params, char* name) {
    LIST_FOREACH(params, param) {
        if (strcmp(param->name, name) == 0) {
            return param;
        }
    }

    return NULL;
}

http_response_t* http_response_new(http_status_t status, http_headers_t* headers,
                                   char* body, size_t body_size) {
    http_response_t* response = malloc(sizeof(http_response_t));
    HTTP_EXPECT(response != NULL, "malloc()");

    response->status = status;

    if (headers != NULL) {
        response->headers = headers;
    } else {
        response->headers = http_headers_new();
    }

    if (body != NULL) {
        response->body = body;
        response->body_size = body_size;
    } else {
        response->body = NULL;
        response->body_size = 0;
    }

    return response;
}

void http_response_free(http_response_t* response) {
    LIST_FOREACH(response->headers, header) {
        http_header_free(header);
    }

    LIST_FREE(response->headers);

    free(response);
}

size_t http_response_head_to_buffer(http_response_t* response, char* buffer,
                                    size_t size) {
    size_t offset = 0;

    offset += snprintf(buffer + offset, size - offset, "HTTP/1.1 %d %s\r\n",
                       response->status, http_status_to_string(response->status));

    LIST_FOREACH(response->headers, header) {
        offset += snprintf(buffer + offset, size - offset, "%s: %s\r\n", header->name,
                           header->value);
    }

    offset += snprintf(buffer + offset, size - offset, "\r\n");
    return offset;
}

http_header_t* http_header_new(char* name, char* value) {
    http_header_t* header = malloc(sizeof(http_header_t));
    HTTP_EXPECT(header != NULL, "malloc()");
    header->name = name;
    header->value = value;
    return header;
}

void http_header_free(http_header_t* header) {
    free(header);
}

http_headers_t* http_headers_new() {
    http_headers_t* headers = LIST_NEW(http_headers_t);
    return headers;
}

void http_headers_free(http_headers_t* headers) {
    LIST_FOREACH(headers, header) {
        http_header_free(header);
    }

    LIST_FREE(headers);
}

void http_headers_add(http_headers_t* headers, http_header_t* header) {
    LIST_APPEND(headers, header);
}

http_header_t* http_headers_get(http_headers_t* headers, char* name) {
    LIST_FOREACH(headers, header) {
        if (strcmp(header->name, name) == 0) {
            return header;
        }
    }

    return NULL;
}

http_handler_t* http_handler_new(char* path, http_handler_callback_t callback) {
    http_handler_t* handler = malloc(sizeof(http_handler_t));
    handler->path = path;
    handler->callback = callback;
    return handler;
}

void http_handler_free(http_handler_t* handler) {
    free(handler);
}

http_thread_args_t* http_thread_args_new(http_server_t* server, int sock_fd) {
    http_thread_args_t* thread_args = malloc(sizeof(http_thread_args_t));
    thread_args->server = server;
    thread_args->sock_fd = sock_fd;
    return thread_args;
}

void http_thread_args_free(http_thread_args_t* thread_args) {
    free(thread_args);
}

char* http_status_to_string(http_status_t status) {
    switch (status) {
    case HTTP_STATUS_OK:
        return "OK";
    case HTTP_STATUS_BAD_REQUEST:
        return "Bad Request";
    case HTTP_STATUS_NOT_FOUND:
        return "Not Found";
    case HTTP_STATUS_METHOD_NOT_ALLOWED:
        return "Method Not Allowed";
    case HTTP_STATUS_INTERNAL_SERVER_ERROR:
        return "Internal Server Error";
    default:
        return "Unknown";
    }
}
