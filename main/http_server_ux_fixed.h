#ifndef HTTP_SERVER_UX_H
#define HTTP_SERVER_UX_H

#include "esp_http_server.h"

// 注册HTTP处理器
void register_http_handlers(httpd_handle_t server);

#endif