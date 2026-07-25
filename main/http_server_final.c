#include "wings_control_final.h"
#include "http_server_final.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "http_server";

// 外部变量声明
extern servo_controller_t g_servo_ctrl;
extern motor_controller_t g_motor_ctrl;
extern sequence_controller_t g_sequence_ctrl;

// 主控制页面HTML
static const char main_html[] = 
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
".speed-input{width:80px;padding:8px;margin:5px;border:2px solid #ffe082;border-radius:8px;}"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<h1>🦋 ESP32 Wings Control</h1>"
"<div class=\"card\">"
"<label>Left Wing Angle: <span id=\"left-angle\">135°</span></label>"
"<input type=\"range\" id=\"left-servo\" min=\"45\" max=\"225\" value=\"135\" oninput=\"updateAngle('left')\">"
"<label>Left Speed (100-1500): <input type=\"number\" id=\"left-speed\" class=\"speed-input\" min=\"100\" max=\"1500\" value=\"1000\" onchange=\"updateSpeed('left')\"></label>"
"<label>Right Wing Angle: <span id=\"right-angle\">135°</span></label>"
"<input type=\"range\" id=\"right-servo\" min=\"45\" max=\"225\" value=\"135\" oninput=\"updateAngle('right')\">"
"<label>Right Speed (100-1500): <input type=\"number\" id=\"right-speed\" class=\"speed-input\" min=\"100\" max=\"1500\" value=\"1000\" onchange=\"updateSpeed('right')\"></label>"
"</div>"
"<div class=\"card\">"
"<button onclick=\"motorAction('expand')\">🕊️ Expand</button>"
"<button onclick=\"motorAction('close')\">🦅 Close</button>"
"<button onclick=\"motorAction('reset')\">🔄 Reset</button>"
"<button onclick=\"motorAction('stop')\">⏹️ Stop</button>"
"<button onclick=\"window.location.href='/sequence'\">🎬 Sequence Programming</button>"
"</div>"
"</div>"
"<script>"
"function updateAngle(side){"
"const slider = document.getElementById(side + '-servo');"
"const angleSpan = document.getElementById(side + '-angle');"
"const angle = parseInt(slider.value);"
"angleSpan.textContent = angle + '°';"
"const speed = parseInt(document.getElementById(side + '-speed').value);"
"const servoId = side === 'left' ? 0 : 1;"
"fetch('/set_servo?servo_id=' + servoId + '&angle=' + angle + '&speed=' + speed);"
"}"
"function updateSpeed(side){"
"const angle = parseInt(document.getElementById(side + '-servo').value);"
"const speed = parseInt(document.getElementById(side + '-speed').value);"
"const servoId = side === 'left' ? 0 : 1;"
"fetch('/set_servo?servo_id=' + servoId + '&angle=' + angle + '&speed=' + speed);"
"}"
"function motorAction(action){"
"fetch('/motor?action=' + action);"
"}"
"// 轮询当前角度更新显示"
"setInterval(() => {"
"  fetch('/get_angles')"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.left >= 0) {"
"        document.getElementById('left-servo').value = data.left;"
"        document.getElementById('left-angle').textContent = data.left + '°';"
"      }"
"      if (data.right >= 0) {"
"        document.getElementById('right-servo').value = data.right;"
"        document.getElementById('right-angle').textContent = data.right + '°';"
"      }"
"    });"
"}, 500);"
"</script>"
"</body>"
"</html>";

// 序列编程页面HTML
static const char sequence_html[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no\">"
"<title>Sequence Programming</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:0;padding:20px;background-color:#e3f2fd;}"
".container{max-width:400px;margin:0 auto;}"
".card{background:white;padding:20px;margin:10px 0;border-radius:16px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}"
"h1{color:#1976d2;text-align:center;font-size:24px;margin-bottom:20px;}"
"button{background:linear-gradient(135deg,#2196f3,#1976d2);color:white;border:none;padding:12px 20px;margin:6px;border-radius:12px;cursor:pointer;font-size:14px;font-weight:bold;box-shadow:0 2px 4px rgba(0,0,0,0.2);}"
".step-btn{background:linear-gradient(135deg,#9c27b0,#7b1fa2);}"
".action-btn{background:linear-gradient(135deg,#ff9800,#f57c00);}"
".back-btn{background:linear-gradient(135deg,#607d8b,#455a64);}"
".delay-input{width:80px;padding:6px;margin:5px;border:2px solid #bbdefb;border-radius:6px;}"
".status{padding:10px;margin:10px 0;background:#f5f5f5;border-radius:8px;font-size:14px;}"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<h1>🎬 Sequence Programming</h1>"
"<div class=\"card\">"
"<button class=\"back-btn\" onclick=\"window.location.href='/'\">⬅️ Back to Main</button>"
"<button onclick=\"startEditing()\">🆕 Start New Sequence</button>"
"<button onclick=\"clearSequence()\">🗑️ Clear All Steps</button>"
"<button onclick=\"playSequence()\">▶️ Play Sequence</button>"
"</div>"
"<div id=\"editing-section\" style=\"display:none;\">"
"<div class=\"card\">"
"<h3>Step <span id=\"current-step\">1</span>/10</h3>"
"<p>1. Click \"Unlock Servos\" below</p>"
"<p>2. Manually rotate wings to desired position</p>"
"<p>3. Click \"Read Current Angles\"</p>"
"<p>4. Set delay and click \"Save Step\"</p>"
"<button class=\"action-btn\" onclick=\"unlockServos()\">🔓 Unlock Servos</button>"
"<button class=\"action-btn\" onclick=\"readAngles()\">📏 Read Current Angles</button>"
"<div>Delay (ms): <input type=\"number\" id=\"step-delay\" class=\"delay-input\" min=\"0\" max=\"10000\" value=\"1000\"></div>"
"<button class=\"step-btn\" onclick=\"saveStep()\">💾 Save Step</button>"
"<button class=\"step-btn\" onclick=\"cancelStep()\">❌ Cancel Step</button>"
"<div class=\"status\" id=\"status\">Ready to start editing...</div>"
"</div>"
"</div>"
"<div id=\"sequence-steps\">"
"<!-- Steps will be displayed here -->"
"</div>"
"</div>"
"<script>"
"let currentStep = -1;"
"let leftAngle = 135, rightAngle = 135;"
"let isEditing = false;"
""
"function startEditing() {"
"  fetch('/sequence/start')"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.success) {"
"        currentStep = data.step;"
"        isEditing = true;"
"        document.getElementById('editing-section').style.display = 'block';"
"        document.getElementById('current-step').textContent = currentStep + 1;"
"        updateStatus('Editing step ' + (currentStep + 1) + '. Unlock servos first.');"
"      } else {"
"        alert('Cannot start editing: ' + data.message);"
"      }"
"    });"
"}"
""
"function unlockServos() {"
"  fetch('/servo/unlock?servo_id=0');"
"  fetch('/servo/unlock?servo_id=1');"
"  updateStatus('Servos unlocked! Rotate wings manually, then read angles.');"
"}"
""
"function readAngles() {"
"  fetch('/get_angles')"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.left >= 0 && data.right >= 0) {"
"        leftAngle = data.left;"
"        rightAngle = data.right;"
"        updateStatus('Current angles: Left=' + leftAngle + '°, Right=' + rightAngle + '°');"
"      } else {"
"        updateStatus('Failed to read angles. Try again.');"
"      }"
"    });"
"}"
""
"function saveStep() {"
"  const delay = parseInt(document.getElementById('step-delay').value);"
"  fetch('/sequence/save?left_angle=' + leftAngle + '&right_angle=' + rightAngle + '&delay=' + delay)"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.success) {"
"        updateStatus('Step ' + (currentStep + 1) + ' saved successfully!');"
"        loadSequenceSteps();"
"        // Check if sequence is full"
"        if (data.full) {"
"          isEditing = false;"
"          document.getElementById('editing-section').style.display = 'none';"
"          updateStatus('Sequence is full (10 steps).');"
"        } else {"
"          // Prepare for next step"
"          currentStep++;"
"          if (currentStep < 10) {"
"            document.getElementById('current-step').textContent = currentStep + 1;"
"            updateStatus('Ready for step ' + (currentStep + 1) + '. Unlock servos.');"
"          } else {"
"            isEditing = false;"
"            document.getElementById('editing-section').style.display = 'none';"
"          }"
"        }"
"      } else {"
"        updateStatus('Failed to save step: ' + data.message);"
"      }"
"    });"
"}"
""
"function cancelStep() {"
"  fetch('/sequence/cancel')"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.success) {"
"        isEditing = false;"
"        document.getElementById('editing-section').style.display = 'none';"
"        updateStatus('Editing cancelled.');"
"        loadSequenceSteps();"
"      }"
"    });"
"}"
""
"function clearSequence() {"
"  if (confirm('Clear all sequence steps?')) {"
"    fetch('/sequence/clear')"
"      .then(() => {"
"        updateStatus('Sequence cleared.');"
"        loadSequenceSteps();"
"        isEditing = false;"
"        document.getElementById('editing-section').style.display = 'none';"
"      });"
"  }"
"}"
""
"function playSequence() {"
"  fetch('/sequence/play')"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.success) {"
"        updateStatus('Playing sequence...');"
"      } else {"
"        updateStatus('Failed to play: ' + data.message);"
"      }"
"    });"
"}"
""
"function updateStatus(message) {"
"  document.getElementById('status').textContent = message;"
"}"
""
"function loadSequenceSteps() {"
"  fetch('/sequence/steps')"
"    .then(response => response.json())"
"    .then(data => {"
"      let html = '<div class=\"card\"><h3>Saved Steps:</h3>';"
"      if (data.steps.length === 0) {"
"        html += '<p>No steps saved yet.</p>';"
"      } else {"
"        data.steps.forEach((step, index) => {"
"          if (step.valid) {"
"            html += `<div>Step ${index + 1}: L=${step.left}° R=${step.right}° Delay=${step.delay}ms</div>`;"
"          }"
"        });"
"      }"
"      html += '</div>';"
"      document.getElementById('sequence-steps').innerHTML = html;"
"    });"
"}"
""
"// Load initial sequence steps"
"loadSequenceSteps();"
"</script>"
"</body>"
"</html>";

// 根页面处理
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, main_html, HTTPD_RESP_USE_STRLEN);
}

// 序列页面处理
static esp_err_t sequence_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, sequence_html, HTTPD_RESP_USE_STRLEN);
}

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

// 解锁舵机处理
static esp_err_t servo_unlock_handler(httpd_req_t *req) {
    char* buf = NULL;
    size_t buf_len;
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_query_key_value(buf, "servo_id", buf, buf_len) == ESP_OK) {
            int servo_id = atoi(buf);
            if (servo_unlock(&g_servo_ctrl, servo_id) == ESP_OK) {
                httpd_resp_sendstr(req, "Unlocked");
            } else {
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "Unlock failed");
            }
        } else {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_sendstr(req, "Missing servo_id");
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
                 "{\"success\":false,\"message\":\"Already editing\"}");
    } else if (sequence_is_full(&g_sequence_ctrl)) {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"Sequence full\"}");
    } else {
        // 找到第一个空步骤
        int next_step = -1;
        for (int i = 0; i < MAX_SEQUENCE_STEPS; i++) {
            if (!g_sequence_ctrl.steps[i].valid) {
                next_step = i;
                break;
            }
        }
        
        if (next_step == -1) {
            snprintf(response, sizeof(response), 
                     "{\"success\":false,\"message\":\"Sequence full\"}");
        } else {
            g_sequence_ctrl.current_step = next_step;
            g_sequence_ctrl.editing = true;
            snprintf(response, sizeof(response), 
                     "{\"success\":true,\"step\":%d}", next_step);
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
            char left_angle_str[10], right_angle_str[10], delay_str[10];
            if (httpd_query_key_value(buf, "left_angle", left_angle_str, sizeof(left_angle_str)) == ESP_OK &&
                httpd_query_key_value(buf, "right_angle", right_angle_str, sizeof(right_angle_str)) == ESP_OK &&
                httpd_query_key_value(buf, "delay", delay_str, sizeof(delay_str)) == ESP_OK) {
                
                int left_angle = atoi(left_angle_str);
                int right_angle = atoi(right_angle_str);
                int delay = atoi(delay_str);
                
                if (g_sequence_ctrl.editing && g_sequence_ctrl.current_step >= 0) {
                    // 使用默认速度1000
                    sequence_add_step(&g_sequence_ctrl, left_angle, right_angle, 1000, 1000, delay);
                    
                    bool full = sequence_is_full(&g_sequence_ctrl);
                    char response[100];
                    snprintf(response, sizeof(response), 
                             "{\"success\":true,\"full\":%s}", full ? "true" : "false");
                    
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
                } else {
                    httpd_resp_set_status(req, "400 Bad Request");
                    httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Not editing\"}");
                }
            } else {
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Missing parameters\"}");
            }
        }
        free(buf);
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Missing parameters\"}");
    }
    return ESP_OK;
}

// 序列取消处理
static esp_err_t sequence_cancel_handler(httpd_req_t *req) {
    if (g_sequence_ctrl.editing) {
        g_sequence_ctrl.editing = false;
        g_sequence_ctrl.current_step = -1;
        httpd_resp_sendstr(req, "{\"success\":true}");
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Not editing\"}");
    }
    return ESP_OK;
}

// 序列清除处理
static esp_err_t sequence_clear_handler(httpd_req_t *req) {
    sequence_clear(&g_sequence_ctrl);
    httpd_resp_sendstr(req, "Cleared");
    return ESP_OK;
}

// 序列播放处理
static esp_err_t sequence_play_handler(httpd_req_t *req) {
    if (g_sequence_ctrl.playing) {
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Already playing\"}");
    } else {
        esp_err_t ret = sequence_play(&g_sequence_ctrl, &g_servo_ctrl);
        if (ret == ESP_OK) {
            httpd_resp_sendstr(req, "{\"success\":true}");
        } else {
            httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Play failed\"}");
        }
    }
    return ESP_OK;
}

// 获取序列步骤处理
static esp_err_t sequence_steps_handler(httpd_req_t *req) {
    char response[500];
    strcpy(response, "{\"steps\":[");
    
    for (int i = 0; i < MAX_SEQUENCE_STEPS; i++) {
        if (i > 0) strcat(response, ",");
        if (g_sequence_ctrl.steps[i].valid) {
            char step_str[100];
            snprintf(step_str, sizeof(step_str), 
                     "{\"valid\":true,\"left\":%d,\"right\":%d,\"delay\":%d}",
                     g_sequence_ctrl.steps[i].left_angle,
                     g_sequence_ctrl.steps[i].right_angle,
                     g_sequence_ctrl.steps[i].delay_ms);
            strcat(response, step_str);
        } else {
            strcat(response, "{\"valid\":false}");
        }
    }
    
    strcat(response, "]}");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// 注册URI处理器
void register_http_handlers(httpd_handle_t server) {
    static const httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t sequence_uri = {
        .uri       = "/sequence",
        .method    = HTTP_GET,
        .handler   = sequence_handler,
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
    
    static const httpd_uri_t get_angles_uri = {
        .uri       = "/get_angles",
        .method    = HTTP_GET,
        .handler   = get_angles_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t servo_unlock_uri = {
        .uri       = "/servo/unlock",
        .method    = HTTP_GET,
        .handler   = servo_unlock_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t sequence_start_uri = {
        .uri       = "/sequence/start",
        .method    = HTTP_GET,
        .handler   = sequence_start_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t sequence_save_uri = {
        .uri       = "/sequence/save",
        .method    = HTTP_GET,
        .handler   = sequence_save_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t sequence_cancel_uri = {
        .uri       = "/sequence/cancel",
        .method    = HTTP_GET,
        .handler   = sequence_cancel_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t sequence_clear_uri = {
        .uri       = "/sequence/clear",
        .method    = HTTP_GET,
        .handler   = sequence_clear_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t sequence_play_uri = {
        .uri       = "/sequence/play",
        .method    = HTTP_GET,
        .handler   = sequence_play_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t sequence_steps_uri = {
        .uri       = "/sequence/steps",
        .method    = HTTP_GET,
        .handler   = sequence_steps_handler,
        .user_ctx  = NULL
    };
    
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &sequence_uri);
    httpd_register_uri_handler(server, &set_servo_uri);
    httpd_register_uri_handler(server, &motor_uri);
    httpd_register_uri_handler(server, &get_angles_uri);
    httpd_register_uri_handler(server, &servo_unlock_uri);
    httpd_register_uri_handler(server, &sequence_start_uri);
    httpd_register_uri_handler(server, &sequence_save_uri);
    httpd_register_uri_handler(server, &sequence_cancel_uri);
    httpd_register_uri_handler(server, &sequence_clear_uri);
    httpd_register_uri_handler(server, &sequence_play_uri);
    httpd_register_uri_handler(server, &sequence_steps_uri);
}