#include "http_server_ux_enhanced.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "string.h"
#include "sequence_manager.h"

// 外部序列管理器声明（在main.c中定义）
extern sequence_manager_t g_sequence_mgr;

static const char *TAG = "http_server_sequence";

// 序列开始录制处理
static esp_err_t sequence_start_handler(httpd_req_t *req) {
    char response[200];
    
    esp_err_t err = seq_mgr_start_recording(&g_sequence_mgr);
    if (err == ESP_OK) {
        int frame_count;
        seq_mgr_get_status(&g_sequence_mgr, NULL, &frame_count, NULL);
        snprintf(response, sizeof(response), 
                 "{\"success\":true,\"message\":\"录制已开始\",\"frame_count\":%d}", 
                 frame_count);
    } else if (err == ESP_ERR_INVALID_STATE) {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"已在录制中\"}");
    } else {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"启动录制失败: %s\"}", 
                 esp_err_to_name(err));
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列记录步骤处理
static esp_err_t sequence_record_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    char response[300];
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char left_angle_str[10], right_angle_str[10];
            if (httpd_query_key_value(buf, "left_angle", left_angle_str, sizeof(left_angle_str)) == ESP_OK &&
                httpd_query_key_value(buf, "right_angle", right_angle_str, sizeof(right_angle_str)) == ESP_OK) {
                
                int left_angle = atoi(left_angle_str);
                int right_angle = atoi(right_angle_str);
                uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000);
                
                esp_err_t err = seq_mgr_record_frame(&g_sequence_mgr, left_angle, right_angle, current_time);
                if (err == ESP_OK) {
                    // 获取上一步信息用于反馈
                    int total_frames;
                    seq_mgr_get_status(&g_sequence_mgr, NULL, &total_frames, NULL);
                    
                    action_frame_t last_frame, prev_frame;
                    int delay_ms = 0;
                    
                    if (total_frames >= 1 && seq_mgr_get_frame(&g_sequence_mgr, total_frames - 1, &last_frame) == ESP_OK) {
                        if (total_frames >= 2 && seq_mgr_get_frame(&g_sequence_mgr, total_frames - 2, &prev_frame) == ESP_OK) {
                            delay_ms = (int)(last_frame.timestamp - prev_frame.timestamp);
                        }
                        snprintf(response, sizeof(response), 
                                 "{\"success\":true,\"message\":\"步骤已记录\",\"frame_count\":%d,\"last_left\":%d,\"last_right\":%d,\"delay_ms\":%d}",
                                 total_frames, last_frame.left_angle, last_frame.right_angle, delay_ms);
                    } else {
                        snprintf(response, sizeof(response), 
                                 "{\"success\":true,\"message\":\"步骤已记录\",\"frame_count\":%d}",
                                 total_frames);
                    }
                } else if (err == ESP_ERR_NO_MEM) {
                    snprintf(response, sizeof(response), 
                             "{\"success\":false,\"message\":\"序列已满（最多%d步）\"}", MAX_SEQUENCE_STEPS);
                } else if (err == ESP_ERR_INVALID_STATE) {
                    snprintf(response, sizeof(response), 
                             "{\"success\":false,\"message\":\"未在录制状态，请先开始录制\"}");
                } else {
                    snprintf(response, sizeof(response), 
                             "{\"success\":false,\"message\":\"记录失败: %s\"}", esp_err_to_name(err));
                }
            } else {
                snprintf(response, sizeof(response), 
                         "{\"success\":false,\"message\":\"缺少角度参数\"}");
            }
        }
        free(buf);
    } else {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"缺少参数\"}");
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列停止录制处理
static esp_err_t sequence_stop_handler(httpd_req_t *req) {
    char response[200];
    
    esp_err_t err = seq_mgr_stop_recording(&g_sequence_mgr);
    if (err == ESP_OK) {
        int frame_count;
        seq_mgr_get_status(&g_sequence_mgr, NULL, &frame_count, NULL);
        snprintf(response, sizeof(response), 
                 "{\"success\":true,\"message\":\"录制已停止\",\"frame_count\":%d}", 
                 frame_count);
    } else if (err == ESP_ERR_INVALID_STATE) {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"未在录制状态\"}");
    } else {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"停止录制失败: %s\"}", 
                 esp_err_to_name(err));
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列撤回处理
static esp_err_t sequence_undo_handler(httpd_req_t *req) {
    char response[200];
    
    esp_err_t err = seq_mgr_undo_last_frame(&g_sequence_mgr);
    if (err == ESP_OK) {
        int frame_count;
        seq_mgr_get_status(&g_sequence_mgr, NULL, &frame_count, NULL);
        snprintf(response, sizeof(response), 
                 "{\"success\":true,\"message\":\"已撤回上一步\",\"frame_count\":%d}", 
                 frame_count);
    } else if (err == ESP_ERR_INVALID_STATE) {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"没有可撤回的步骤\"}");
    } else {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"撤回失败: %s\"}", 
                 esp_err_to_name(err));
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列清除处理
static esp_err_t sequence_clear_handler(httpd_req_t *req) {
    seq_mgr_clear_all(&g_sequence_mgr);
    char response[] = "{\"success\":true,\"message\":\"序列已清空\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 获取序列状态处理
static esp_err_t sequence_status_handler(httpd_req_t *req) {
    char response[500];
    
    sequence_state_t state;
    int frame_count;
    bool is_full;
    
    esp_err_t err = seq_mgr_get_status(&g_sequence_mgr, &state, &frame_count, &is_full);
    if (err == ESP_OK) {
        const char* state_str = (state == SEQ_STATE_RECORDING) ? "recording" : "idle";
        snprintf(response, sizeof(response), 
                 "{\"success\":true,\"state\":\"%s\",\"frame_count\":%d,\"is_full\":%s,\"max_steps\":%d}",
                 state_str, frame_count, is_full ? "true" : "false", MAX_SEQUENCE_STEPS);
    } else {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"获取状态失败: %s\"}", 
                 esp_err_to_name(err));
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 获取序列步骤详情处理
static esp_err_t sequence_steps_handler(httpd_req_t *req) {
    char response[1000];
    char steps_json[800] = "[";
    bool first = true;
    
    sequence_state_t state;
    int frame_count;
    seq_mgr_get_status(&g_sequence_mgr, &state, &frame_count, NULL);
    
    for (int i = 0; i < frame_count && i < MAX_SEQUENCE_STEPS; i++) {
        action_frame_t frame;
        if (seq_mgr_get_frame(&g_sequence_mgr, i, &frame) == ESP_OK) {
            if (!first) strcat(steps_json, ",");
            char step_str[100];
            snprintf(step_str, sizeof(step_str), 
                     "{\"index\":%d,\"left\":%d,\"right\":%d,\"timestamp\":%lu}",
                     i, frame.left_angle, frame.right_angle, frame.timestamp);
            strcat(steps_json, step_str);
            first = false;
        }
    }
    strcat(steps_json, "]");
    
    snprintf(response, sizeof(response), 
             "{\"success\":true,\"steps\":%s,\"frame_count\":%d,\"state\":\"%s\"}",
             steps_json, frame_count, 
             (state == SEQ_STATE_RECORDING) ? "recording" : "idle");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 设置默认序列处理
static esp_err_t sequence_default_handler(httpd_req_t *req) {
    esp_err_t err = seq_mgr_set_default_sequence(&g_sequence_mgr);
    char response[200];
    
    if (err == ESP_OK) {
        int frame_count;
        seq_mgr_get_status(&g_sequence_mgr, NULL, &frame_count, NULL);
        snprintf(response, sizeof(response), 
                 "{\"success\":true,\"message\":\"默认序列已设置\",\"frame_count\":%d}",
                 frame_count);
    } else {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"设置默认序列失败: %s\"}", 
                 esp_err_to_name(err));
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 播放序列处理（需要集成到舵机控制）
static esp_err_t sequence_play_handler(httpd_req_t *req) {
    // 这里需要调用实际的播放函数
    // 由于播放涉及舵机控制，需要在wings_control模块中实现
    char response[] = "{\"success\":true,\"message\":\"序列播放功能待集成\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 注册序列相关的HTTP处理器
void register_sequence_handlers(httpd_handle_t server) {
    // 开始录制
    httpd_uri_t start_uri = {
        .uri = "/sequence/start",
        .method = HTTP_GET,
        .handler = sequence_start_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &start_uri);
    
    // 记录步骤
    httpd_uri_t record_uri = {
        .uri = "/sequence/record",
        .method = HTTP_GET,
        .handler = sequence_record_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &record_uri);
    
    // 停止录制
    httpd_uri_t stop_uri = {
        .uri = "/sequence/stop",
        .method = HTTP_GET,
        .handler = sequence_stop_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &stop_uri);
    
    // 撤回
    httpd_uri_t undo_uri = {
        .uri = "/sequence/undo",
        .method = HTTP_GET,
        .handler = sequence_undo_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &undo_uri);
    
    // 清除
    httpd_uri_t clear_uri = {
        .uri = "/sequence/clear",
        .method = HTTP_GET,
        .handler = sequence_clear_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &clear_uri);
    
    // 状态查询
    httpd_uri_t status_uri = {
        .uri = "/sequence/status",
        .method = HTTP_GET,
        .handler = sequence_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &status_uri);
    
    // 步骤详情
    httpd_uri_t steps_uri = {
        .uri = "/sequence/steps",
        .method = HTTP_GET,
        .handler = sequence_steps_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &steps_uri);
    
    // 默认序列
    httpd_uri_t default_uri = {
        .uri = "/sequence/default",
        .method = HTTP_GET,
        .handler = sequence_default_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &default_uri);
    
    // 播放序列
    httpd_uri_t play_uri = {
        .uri = "/sequence/play",
        .method = HTTP_GET,
        .handler = sequence_play_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &play_uri);
}