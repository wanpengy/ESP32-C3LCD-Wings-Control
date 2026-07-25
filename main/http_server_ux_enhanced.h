#ifndef HTTP_SERVER_UX_ENHANCED_H
#define HTTP_SERVER_UX_ENHANCED_H

#include "esp_http_server.h"

void register_http_handlers(httpd_handle_t server);
void register_robot_sequence_handler(httpd_handle_t server);
void register_sequence_handlers(httpd_handle_t server);

#endif // HTTP_SERVER_UX_ENHANCED_H