#include "http_server_ux_enhanced.h"
#include "esp_log.h"

static const char *TAG = "http_server_ux";

// 外部声明
extern void register_http_handlers(httpd_handle_t server);
extern void register_robot_sequence_handler(httpd_handle_t server);
extern void register_sequence_handlers(httpd_handle_t server);

void start_http_server(httpd_handle_t server) {
    // 注册核心HTTP处理器
    register_http_handlers(server);
    
    // 注册机器人编程界面处理器（同时处理 / 和 /robot 路由）
    register_robot_sequence_handler(server);
    
    // 注册序列管理API处理器
    register_sequence_handlers(server);
    
    ESP_LOGI(TAG, "All HTTP handlers registered successfully");
}