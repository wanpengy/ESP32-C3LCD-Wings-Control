                httpd_query_key_value(buf, "left", left_str, sizeof(left_str)) == ESP_OK &&
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
                 "{"success":false,"message":"已在编辑中"}");
    } else if (sequence_is_full(&g_sequence_ctrl)) {
        snprintf(response, sizeof(response), 
                 "{"success":false,"message":"序列已满"}");
    } else {
        int next_step = sequence_start_editing(&g_sequence_ctrl);
        if (next_step >= 0) {
            snprintf(response, sizeof(response), 
                     "{"success":true,"step":%d}", next_step);
        } else {
            snprintf(response, sizeof(response), 
                     "{"success":false,"message":"无法开始编辑"}");
        }
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列保存步骤处理
static esp_err_t sequence_save_handler(httpd_req_t *req) {
    char param[32];
    size_t param_len;
    
    param_len = httpd_req_get_url_query_len(req) + 1;
    if (param_len > 1) {
        char* buf = malloc(param_len);
        if (httpd_req_get_url_query_str(req, buf, param_len) == ESP_OK) {
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
                             "{"success":true,"full":%s}", 
                             sequence_is_full(&g_sequence_ctrl) ? "true" : "false");
                } else {
                    snprintf(response, sizeof(response), 
                             "{"success":false,"message":"保存失败"}");
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
    char response[] = "{"success":true}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列清除处理
static esp_err_t sequence_clear_handler(httpd_req_t *req) {
    sequence_clear(&g_sequence_ctrl);
    char response[] = "{"success":true}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 序列播放处理
static esp_err_t sequence_play_handler(httpd_req_t *req) {
    bool success = sequence_play(&g_sequence_ctrl);
    char response[50];
    snprintf(response, sizeof(response), 
             "{"success":%s,"message":"%s"}", 
             success ? "true" : "false",
             success ? "播放成功" : "播放失败");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 获取序列步骤处理
static esp_err_t sequence_steps_handler(httpd_req_t *req) {
    char response[500];
    snprintf(response, sizeof(response), "{"steps":[");
    
    for (int i = 0; i < g_sequence_ctrl.step_count; i++) {
        if (i > 0) strcat(response, ",");
        snprintf(response + strlen(response), sizeof(response) - strlen(response),
                 "{"valid":%s,"left":%d,"right":%d,"delay":%d}",
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
    char param[32];
    size_t param_len;
    
    param_len = httpd_req_get_url_query_len(req) + 1;
    if (param_len > 1) {
        char* buf = malloc(param_len);
        if (httpd_req_get_url_query_str(req, buf, param_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "servo_id", param, sizeof(param)) == ESP_OK) {
                int servo_id = atoi(param);
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
    // 主页面路由
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);
    
    // 序列页面路由
    httpd_uri_t sequence_uri = {
        .uri = "/sequence",
        .method = HTTP_GET,
        .handler = sequence_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sequence_uri);
    
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
    
    // 序列相关路由
    httpd_uri_t sequence_start_uri = {
        .uri = "/sequence/start",
        .method = HTTP_GET,
        .handler = sequence_start_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sequence_start_uri);
    
    httpd_uri_t sequence_save_uri = {
        .uri = "/sequence/save",
        .method = HTTP_GET,
        .handler = sequence_save_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sequence_save_uri);
    
    httpd_uri_t sequence_cancel_uri = {
        .uri = "/sequence/cancel",
        .method = HTTP_GET,
        .handler = sequence_cancel_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sequence_cancel_uri);
    
    httpd_uri_t sequence_clear_uri = {
        .uri = "/sequence/clear",
        .method = HTTP_GET,
        .handler = sequence_clear_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sequence_clear_uri);
    
    httpd_uri_t sequence_play_uri = {
        .uri = "/sequence/play",
        .method = HTTP_GET,
        .handler = sequence_play_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sequence_play_uri);
    
    httpd_uri_t sequence_steps_uri = {
        .uri = "/sequence/steps",
        .method = HTTP_GET,
        .handler = sequence_steps_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sequence_steps_uri);
    
    // 舵机解锁路由
    httpd_uri_t servo_unlock_uri = {
        .uri = "/servo/unlock",
        .method = HTTP_GET,
        .handler = servo_unlock_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &servo_unlock_uri);
    
    // Favicon处理
    httpd_uri_t favicon_uri = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &favicon_uri);
    
    ESP_LOGI(TAG, "All HTTP handlers registered successfully");
}