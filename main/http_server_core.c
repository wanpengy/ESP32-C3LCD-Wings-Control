#include "wings_control_ux_enhanced.h"
#include "http_server_ux_enhanced.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "http_server";

// 外部变量声明
extern servo_controller_t g_servo_ctrl;
extern motor_controller_t g_motor_ctrl;
extern sequence_controller_t g_sequence_ctrl;
extern int g_min_angle;
extern int g_max_angle;
extern bool g_symmetric_lock;

// 声明LCD更新函数
void update_lcd_angles(int left_angle, int right_angle, int min_angle, int max_angle);
void update_lcd_progress(const char* progress);
void refresh_lcd_display(void);

// 设置舵机角度和速度处理
static esp_err_t set_servo_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char servo_id_str[10], angle_str[10], speed_str[10];
            if (httpd_query_key_value(buf, "servo_id", servo_id_str, sizeof(servo_id_str)) == ESP_OK &&
                httpd_query_key_value(buf, "angle", angle_str, sizeof(angle_str)) == ESP_OK &&
                httpd_query_key_value(buf, "speed", speed_str, sizeof(speed_str)) == ESP_OK) {
                
                int servo_id = atoi(servo_id_str);
                int angle = atoi(angle_str);
                int speed = atoi(speed_str);
                
                if (servo_set_angle_and_speed(&g_servo_ctrl, servo_id, angle, speed) == ESP_OK) {
                    // 更新LCD进度
                    char progress[50];
                    snprintf(progress, sizeof(progress), "舵机%d设为%d°", servo_id, angle);
                    update_lcd_progress(progress);
                    refresh_lcd_display();
                    httpd_resp_sendstr(req, "OK");
                } else {
                    httpd_resp_set_status(req, "400 Bad Request");
                    httpd_resp_sendstr(req, "Error setting servo");
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
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char action_param[32];
            if (httpd_query_key_value(buf, "action", action_param, sizeof(action_param)) == ESP_OK) {
                if (strcmp(action_param, "expand") == 0) {
                    motor_expand_wings(&g_motor_ctrl);
                    update_lcd_progress("电机: 展开翅膀");
                    refresh_lcd_display();
                    httpd_resp_sendstr(req, "Expanded");
                } else if (strcmp(action_param, "close") == 0) {
                    motor_close_wings(&g_motor_ctrl);
                    update_lcd_progress("电机: 闭合翅膀");
                    refresh_lcd_display();
                    httpd_resp_sendstr(req, "Closed");
                } else if (strcmp(action_param, "reset") == 0) {
                    motor_reset_wings(&g_motor_ctrl);
                    update_lcd_progress("电机: 复位");
                    refresh_lcd_display();
                    httpd_resp_sendstr(req, "Reset");
                } else if (strcmp(action_param, "stop") == 0) {
                    motor_stop(&g_motor_ctrl);
                    update_lcd_progress("电机: 停止");
                    refresh_lcd_display();
                    httpd_resp_sendstr(req, "Stopped");
                } else {
                    httpd_resp_set_status(req, "400 Bad Request");
                    httpd_resp_sendstr(req, "Invalid action");
                }
            } else {
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "Missing action parameter");
            }
        }
        free(buf);
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Missing parameters");
    }
    return ESP_OK;
}

// 获取当前角度处理
static esp_err_t get_angles_handler(httpd_req_t *req) {
    char response[100];
    int left_angle = servo_read_angle(&g_servo_ctrl, 0);
    int right_angle = servo_read_angle(&g_servo_ctrl, 1);
    
    snprintf(response, sizeof(response), 
             "{\"left\":%d,\"right\":%d}", left_angle, right_angle);
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 设置角度限制处理
static esp_err_t set_limits_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char min_str[10], max_str[10];
            if (httpd_query_key_value(buf, "min", min_str, sizeof(min_str)) == ESP_OK &&
                httpd_query_key_value(buf, "max", max_str, sizeof(max_str)) == ESP_OK) {
                
                int min_angle = atoi(min_str);
                int max_angle = atoi(max_str);
                set_angle_limits(min_angle, max_angle);
                
                // 更新LCD显示
                int left_current = servo_read_angle(&g_servo_ctrl, 0);
                int right_current = servo_read_angle(&g_servo_ctrl, 1);
                if (left_current < 0) left_current = 135;
                if (right_current < 0) right_current = 135;
                update_lcd_angles(left_current, right_current, min_angle, max_angle);
                update_lcd_progress("角度范围已更新");
                refresh_lcd_display();
                
                httpd_resp_sendstr(req, "Limits updated");
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

// 设置对称锁定处理
static esp_err_t set_symmetric_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char locked_param[32];
            if (httpd_query_key_value(buf, "locked", locked_param, sizeof(locked_param)) == ESP_OK) {
                int locked = atoi(locked_param);
                set_symmetric_lock(locked != 0);
                
                // 更新LCD进度
                update_lcd_progress(locked ? "对称锁定: 开启" : "对称锁定: 关闭");
                refresh_lcd_display();
                
                httpd_resp_sendstr(req, "Symmetric lock updated");
            } else {
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "Missing locked parameter");
            }
        }
        free(buf);
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Missing parameters");
    }
    return ESP_OK;
}

// LCD角度更新处理
static esp_err_t lcd_angles_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char left_str[10], right_str[10], min_str[10], max_str[10];
            if (httpd_query_key_value(buf, "left", left_str, sizeof(left_str)) == ESP_OK &&
                httpd_query_key_value(buf, "right", right_str, sizeof(right_str)) == ESP_OK &&
                httpd_query_key_value(buf, "min", min_str, sizeof(min_str)) == ESP_OK &&
                httpd_query_key_value(buf, "max", max_str, sizeof(max_str)) == ESP_OK) {
                
                int left_angle = atoi(left_str);
                int right_angle = atoi(right_str);
                int min_angle = atoi(min_str);
                int max_angle = atoi(max_str);
                
                update_lcd_angles(left_angle, right_angle, min_angle, max_angle);
                refresh_lcd_display();
                httpd_resp_sendstr(req, "LCD updated");
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

// 序列开始编辑处理
static esp_err_t sequence_start_handler(httpd_req_t *req) {
    char response[100];
    
    if (g_sequence_ctrl.editing) {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"已在编辑中\"}");
    } else if (sequence_is_full(&g_sequence_ctrl)) {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"序列已满\"}");
    } else {
        int next_step = sequence_start_editing(&g_sequence_ctrl);
        if (next_step >= 0) {
            snprintf(response, sizeof(response), 
                     "{\"success\":true,\"step\":%d}", next_step);
        } else {
            snprintf(response, sizeof(response), 
                     "{\"success\":false,\"message\":\"无法开始编辑\"}");
        }
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列保存步骤处理
static esp_err_t sequence_save_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char left_str[10], right_str[10], delay_str[10];
            if (httpd_query_key_value(buf, "left_angle", left_str, sizeof(left_str)) == ESP_OK &&
                httpd_query_key_value(buf, "right_angle", right_str, sizeof(right_str)) == ESP_OK &&
                httpd_query_key_value(buf, "delay", delay_str, sizeof(delay_str)) == ESP_OK) {
                
                int left_angle = atoi(left_str);
                int right_angle = atoi(right_str);
                int delay_ms = atoi(delay_str);
                
                bool success = sequence_save_step(&g_sequence_ctrl, left_angle, right_angle, delay_ms);
                char response[100];
                if (success) {
                    snprintf(response, sizeof(response), 
                             "{\"success\":true,\"full\":%s}", 
                             sequence_is_full(&g_sequence_ctrl) ? "true" : "false");
                } else {
                    snprintf(response, sizeof(response), 
                             "{\"success\":false,\"message\":\"保存失败\"}");
                }
                
                httpd_resp_set_type(req, "application/json");
                free(buf);
                return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
            }
        }
        free(buf);
    }
    
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "Missing parameters");
}

// 序列取消编辑处理
static esp_err_t sequence_cancel_handler(httpd_req_t *req) {
    sequence_cancel_editing(&g_sequence_ctrl);
    char response[] = "{\"success\":true}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列清除处理
static esp_err_t sequence_clear_handler(httpd_req_t *req) {
    sequence_clear(&g_sequence_ctrl);
    char response[] = "{\"success\":true}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列播放处理
static esp_err_t sequence_play_handler(httpd_req_t *req) {
    bool success = sequence_play(&g_sequence_ctrl);
    char response[50];
    snprintf(response, sizeof(response), 
             "{\"success\":%s,\"message\":\"%s\"}", 
             success ? "true" : "false",
             success ? "播放成功" : "播放失败");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 获取序列步骤处理
static esp_err_t sequence_steps_handler(httpd_req_t *req) {
    char response[500];
    snprintf(response, sizeof(response), "{\"steps\":[");
    
    for (int i = 0; i < g_sequence_ctrl.step_count; i++) {
        if (i > 0) strcat(response, ",");
        snprintf(response + strlen(response), sizeof(response) - strlen(response),
                 "{\"valid\":%s,\"left\":%d,\"right\":%d,\"delay\":%d}",
                 g_sequence_ctrl.steps[i].valid ? "true" : "false",
                 g_sequence_ctrl.steps[i].left_angle,
                 g_sequence_ctrl.steps[i].right_angle,
                 g_sequence_ctrl.steps[i].delay_ms);
    }
    strcat(response, "]}");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 舵机解锁处理
static esp_err_t servo_unlock_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char servo_id_param[32];
            if (httpd_query_key_value(buf, "servo_id", servo_id_param, sizeof(servo_id_param)) == ESP_OK) {
                int servo_id = atoi(servo_id_param);
                if (servo_id >= 0 && servo_id <= 1) {
                    servo_unlock(&g_servo_ctrl, servo_id);
                    httpd_resp_sendstr(req, "Unlocked");
                } else {
                    httpd_resp_set_status(req, "400 Bad Request");
                    httpd_resp_sendstr(req, "Invalid servo ID");
                }
            } else {
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "Missing servo_id parameter");
            }
        }
        free(buf);
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Missing parameters");
    }
    return ESP_OK;
}

// 注册HTTP处理器
void register_http_handlers(httpd_handle_t server) {
    // 舵机控制路由
    httpd_uri_t set_servo_uri = {
        .uri = "/set_servo",
        .method = HTTP_GET,
        .handler = set_servo_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &set_servo_uri);
    
    // 电机控制路由
    httpd_uri_t motor_uri = {
        .uri = "/motor",
        .method = HTTP_GET,
        .handler = motor_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &motor_uri);
    
    // 获取角度路由
    httpd_uri_t get_angles_uri = {
        .uri = "/get_angles",
        .method = HTTP_GET,
        .handler = get_angles_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_angles_uri);
    
    // 设置角度限制路由
    httpd_uri_t set_limits_uri = {
        .uri = "/set_limits",
        .method = HTTP_GET,
        .handler = set_limits_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &set_limits_uri);
    
    // 设置对称锁定路由
    httpd_uri_t set_symmetric_uri = {
        .uri = "/set_symmetric",
        .method = HTTP_GET,
        .handler = set_symmetric_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &set_symmetric_uri);
    
    // LCD角度更新路由
    httpd_uri_t lcd_angles_uri = {
        .uri = "/lcd/angles",
        .method = HTTP_GET,
        .handler = lcd_angles_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &lcd_angles_uri);
    
    // 舵机解锁路由
    httpd_uri_t servo_unlock_uri = {
        .uri = "/servo/unlock",
        .method = HTTP_GET,
        .handler = servo_unlock_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &servo_unlock_uri);
    
    ESP_LOGI(TAG, "Core HTTP handlers registered successfully");
}