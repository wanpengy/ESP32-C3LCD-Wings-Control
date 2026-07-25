#include "wings_control_esp5_noimu.h"
#include "http_server_esp5_noimu.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "http_server";

// 外部变量声明
extern servo_controller_t g_servo_ctrl;
extern motor_controller_t g_motor_ctrl;

// 简化的HTML页面（无IMU控制，基于样例功能）
static const char html_page[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no\">"
"<title>ESP32 Wings Control</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:0;padding:20px;background-color:#fff9c4;}"
".container{max-width:400px;margin:0 auto;}"
".card{background:white;padding:20px;margin:10px 0;border-radius:16px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}"
"h1{color:#ff8f00;text-align:center;font-size:24px;margin-bottom:20px;}"
"label{display:block;margin:15px 0 8px;font-weight:bold;font-size:16px;}"
"input[type=\"range\"]{width:100%;margin:10px 0;height:40px;-webkit-appearance:none;background:#ffe082;border-radius:20px;outline:none;}"
"input[type=\"range\"]::-webkit-slider-thumb{-webkit-appearance:none;width:32px;height:32px;border-radius:50%;background:#ff8f00;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.3);}"
"button{background:linear-gradient(135deg,#4caf50,#43a047);color:white;border:none;padding:14px 24px;margin:8px 4px;border-radius:16px;cursor:pointer;font-size:16px;font-weight:bold;box-shadow:0 2px 4px rgba(0,0,0,0.2);}"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<h1>🦋 ESP32 Wings Control</h1>"
"<div class=\"card\">"
"<label>Left Wing Angle: <span id=\"left-angle\">135°</span></label>"
"<input type=\"range\" id=\"left-servo\" min=\"45\" max=\"225\" value=\"135\" oninput=\"updateAngle('left')\">"
"<label>Right Wing Angle: <span id=\"right-angle\">135°</span></label>"
"<input type=\"range\" id=\"right-servo\" min=\"45\" max=\"225\" value=\"135\" oninput=\"updateAngle('right')\">"
"</div>"
"<div class=\"card\">"
"<button onclick=\"motorAction('expand')\">🕊️ Expand</button>"
"<button onclick=\"motorAction('close')\">🦅 Close</button>"
"<button onclick=\"motorAction('reset')\">🔄 Reset</button>"
"<button onclick=\"motorAction('stop')\">⏹️ Stop</button>"
"</div>"
"</div>"
"<script>"
"function updateAngle(side){"
"const slider = document.getElementById(side + '-servo');"
"const angleSpan = document.getElementById(side + '-angle');"
"const angle = parseInt(slider.value);"
"angleSpan.textContent = angle + '°';"
"const servoId = side === 'left' ? 0 : 1;"
"fetch('/set_servo?servo_id=' + servoId + '&angle=' + angle);"
"}"
"function motorAction(action){"
"fetch('/motor?action=' + action);"
"}"
"</script>"
"</body>"
"</html>";

// 根页面处理
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
}

// 设置舵机角度处理
static esp_err_t set_servo_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char servo_id_str[10], angle_str[10];
            if (httpd_query_key_value(buf, "servo_id", servo_id_str, sizeof(servo_id_str)) == ESP_OK &&
                httpd_query_key_value(buf, "angle", angle_str, sizeof(angle_str)) == ESP_OK) {
                
                int servo_id = atoi(servo_id_str);
                int angle = atoi(angle_str);
                
                if (servo_set_angle(&g_servo_ctrl, servo_id, angle) == ESP_OK) {
                    httpd_resp_sendstr(req, "OK");
                } else {
                    httpd_resp_set_status(req, "400 Bad Request");
                    httpd_resp_sendstr(req, "Error setting servo angle");
                }
            } else {
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "Missing parameters");
            }
        }
        free(buf);
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Missing parameters");
    }
    return ESP_OK;
}

// 电机控制处理
static esp_err_t motor_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_query_key_value(buf, "action", buf, buf_len) == ESP_OK) {
            if (strcmp(buf, "expand") == 0) {
                motor_expand_wings(&g_motor_ctrl);
                httpd_resp_sendstr(req, "Expanded");
            } else if (strcmp(buf, "close") == 0) {
                motor_close_wings(&g_motor_ctrl);
                httpd_resp_sendstr(req, "Closed");
            } else if (strcmp(buf, "reset") == 0) {
                motor_reset_wings(&g_motor_ctrl);
                httpd_resp_sendstr(req, "Reset");
            } else if (strcmp(buf, "stop") == 0) {
                motor_stop(&g_motor_ctrl);
                httpd_resp_sendstr(req, "Stopped");
            } else {
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "Invalid action");
            }
        } else {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_sendstr(req, "Missing action parameter");
        }
        free(buf);
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Missing parameters");
    }
    return ESP_OK;
}

// 注册URI处理器
void register_http_handlers(httpd_handle_t server) {
    static const httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t set_servo_uri = {
        .uri       = "/set_servo",
        .method    = HTTP_GET,
        .handler   = set_servo_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t motor_uri = {
        .uri       = "/motor",
        .method    = HTTP_GET,
        .handler   = motor_handler,
        .user_ctx  = NULL
    };
    
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &set_servo_uri);
    httpd_register_uri_handler(server, &motor_uri);
}