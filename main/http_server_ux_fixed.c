#include "wings_control_ux_fixed.h"
#include "http_server_ux_fixed.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "http_server";

// 外部变量声明
extern servo_controller_t g_servo_ctrl;
extern motor_controller_t g_motor_ctrl;
extern sequence_controller_t g_sequence_ctrl;
extern int g_min_angle;
extern int g_max_angle;
extern bool g_symmetric_lock;

// UX优化的主控制页面HTML（修正默认值）
static const char main_html[] = 
"<!DOCTYPE html>"
"<html lang=\"zh-CN\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no\">"
"<title>🕊️ 翅膀控制终端</title>"
"<style>"
"body{font-family:'Microsoft YaHei',Arial,sans-serif;margin:0;padding:20px;background-color:#fff9c4;}"
".container{max-width:400px;margin:0 auto;}"
".card{background:white;padding:20px;margin:15px 0;border-radius:16px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}"
"h1{color:#ff8f00;text-align:center;font-size:24px;margin-bottom:20px;}"
"h2{color:#4caf50;font-size:18px;margin:15px 0 10px 0;padding-bottom:8px;border-bottom:2px solid #e0e0e0;}"
"label{display:block;margin:12px 0 6px;font-weight:bold;font-size:15px;color:#333;}"
"input[type=\"range\"]{width:100%;margin:8px 0;height:36px;-webkit-appearance:none;background:#ffe082;border-radius:18px;outline:none;}"
"input[type=\"range\"]::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:28px;border-radius:50%;background:#ff8f00;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.3);}"
".current-indicator{width:100%;height:6px;background:#e0e0e0;border-radius:3px;margin:5px 0;position:relative;}"
".current-indicator-fill{height:100%;background:#4caf50;border-radius:3px;}"
".speed-input{width:70px;padding:6px;margin:5px;border:2px solid #ffe082;border-radius:6px;font-size:14px;}"
".angle-limit-input{width:60px;padding:5px;margin:0 5px;border:1px solid #ccc;border-radius:4px;font-size:13px;}"
".symmetric-toggle{display:flex;align-items:center;margin:10px 0;}"
".symmetric-toggle input{margin-right:8px;transform:scale(1.2);}"
".button-group{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px;}"
".button-group button{flex:1;min-width:80px;background:linear-gradient(135deg,#4caf50,#43a047);color:white;border:none;padding:10px;margin:0;border-radius:12px;cursor:pointer;font-size:14px;font-weight:bold;box-shadow:0 2px 4px rgba(0,0,0,0.2);}"
".motor-button{background:linear-gradient(135deg,#2196f3,#1976d2)!important;}"
".sequence-button{background:linear-gradient(135deg,#ff9800,#f57c00)!important;}"
".status-text{font-size:13px;color:#666;margin-top:5px;}"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<h1>🦋 翅膀控制系统</h1>"

"<!-- 角度限制和对称控制 -->"
"<div class=\"card\">"
"<h2>⚙️ 控制设置</h2>"
"<div>"
"<label>角度范围限制:</label>"
"<div>最小角度: <input type=\"number\" id=\"min-angle\" class=\"angle-limit-input\" min=\"0\" max=\"270\" value=\"120\" onchange=\"updateAngleLimits()\">°</div>"
"<div>最大角度: <input type=\"number\" id=\"max-angle\" class=\"angle-limit-input\" min=\"0\" max=\"270\" value=\"150\" onchange=\"updateAngleLimits()\">°</div>"
"</div>"
"<div class=\"symmetric-toggle\">"
"<input type=\"checkbox\" id=\"symmetric-lock\" checked onchange=\"toggleSymmetricLock()\">"
"<label for=\"symmetric-lock\">对称锁定 (左右翅膀同步)</label>"
"</div>"
"</div>"

"<!-- 舵机控制 -->"
"<div class=\"card\">"
"<h2>🕊️ 舵机控制</h2>"
"<div>"
"<label>左翅膀角度: <span id=\"left-angle-value\">135°</span></label>"
"<input type=\"range\" id=\"left-servo\" min=\"120\" max=\"150\" value=\"135\" oninput=\"updateAngle('left')\">"
"<div class=\"current-indicator\"><div id=\"left-current-fill\" class=\"current-indicator-fill\" style=\"width:50%\"></div></div>"
"<label>左翅膀速度 (100-1500ms): <input type=\"number\" id=\"left-speed\" class=\"speed-input\" min=\"100\" max=\"1500\" value=\"100\" onchange=\"updateSpeed('left')\"></label>"
"</div>"
"<div style=\"margin-top:20px;\">"
"<label>右翅膀角度: <span id=\"right-angle-value\">135°</span></label>"
"<input type=\"range\" id=\"right-servo\" min=\"120\" max=\"150\" value=\"135\" oninput=\"updateAngle('right')\">"
"<div class=\"current-indicator\"><div id=\"right-current-fill\" class=\"current-indicator-fill\" style=\"width:50%\"></div></div>"
"<label>右翅膀速度 (100-1500ms): <input type=\"number\" id=\"right-speed\" class=\"speed-input\" min=\"100\" max=\"1500\" value=\"100\" onchange=\"updateSpeed('right')\"></label>"
"</div>"
"</div>"

"<!-- 开合控制 -->"
"<div class=\"card\">"
"<h2>⚡ 电机控制</h2>"
"<div class=\"button-group\">"
"<button class=\"motor-button\" onclick=\"motorAction('expand')\">展开翅膀</button>"
"<button class=\"motor-button\" onclick=\"motorAction('close')\">闭合翅膀</button>"
"<button class=\"motor-button\" onclick=\"motorAction('reset')\">复位</button>"
"<button class=\"motor-button\" onclick=\"motorAction('stop')\">停止</button>"
"</div>"
"</div>"

"<!-- 编程与执行功能 -->"
"<div class=\"card\">"
"<h2>🧪 编程与执行 (测试功能)</h2>"
"<div class=\"button-group\">"
"<button class=\"sequence-button\" onclick=\"window.location.href='/sequence'\">动作编程</button>"
"<button class=\"sequence-button\" onclick=\"playSequence()\">播放序列</button>"
"<button class=\"sequence-button\" onclick=\"clearSequence()\">清除序列</button>"
"</div>"
"<div class=\"status-text\">支持最多10步动作序列编程</div>"
"</div>"
"</div>"

"<script>"
"// 全局变量"
"let minAngle = 120;"  // 默认最小角度
"let maxAngle = 150;"  // 默认最大角度
"let symmetricLock = true;"
""
"// 初始化滑条范围"
"function initSliderRanges() {"
"  const leftSlider = document.getElementById('left-servo');"
"  const rightSlider = document.getElementById('right-servo');"
"  leftSlider.min = minAngle;"
"  leftSlider.max = maxAngle;"
"  rightSlider.min = minAngle;"
"  rightSlider.max = maxAngle;"
"  "
"  // 如果超出范围，调整到范围内"
"  if (parseInt(leftSlider.value) < minAngle) leftSlider.value = minAngle;"
"  if (parseInt(leftSlider.value) > maxAngle) leftSlider.value = maxAngle;"
"  if (parseInt(rightSlider.value) < minAngle) rightSlider.value = minAngle;"
"  if (parseInt(rightSlider.value) > maxAngle) rightSlider.value = maxAngle;"
"  "
"  updateAngleDisplay();"
"}"
""
"// 更新角度限制"
"function updateAngleLimits() {"
"  const newMin = parseInt(document.getElementById('min-angle').value);"
"  const newMax = parseInt(document.getElementById('max-angle').value);"
"  "
"  if (newMin >= 0 && newMin <= 270 && newMax >= 0 && newMax <= 270 && newMin < newMax) {"
"    minAngle = newMin;"
"    maxAngle = newMax;"
"    initSliderRanges();"
"    fetch('/set_limits?min=' + minAngle + '&max=' + maxAngle);"
"  } else {"
"    alert('角度范围无效！最小角度必须小于最大角度，且在0-270范围内。');"
"    document.getElementById('min-angle').value = minAngle;"
"    document.getElementById('max-angle').value = maxAngle;"
"  }"
"}"
""
"// 切换对称锁定"
"function toggleSymmetricLock() {"
"  symmetricLock = document.getElementById('symmetric-lock').checked;"
"  fetch('/set_symmetric?locked=' + (symmetricLock ? '1' : '0'));"
"  "
"  if (symmetricLock) {"
"    // 同步角度"
"    const leftValue = parseInt(document.getElementById('left-servo').value);"
"    document.getElementById('right-servo').value = leftValue;"
"    document.getElementById('right-angle-value').textContent = leftValue + '°';"
"    updateCurrentIndicator('right', leftValue);"
"  }"
"}"
""
"// 更新角度显示"
"function updateAngleDisplay() {"
"  const leftSlider = document.getElementById('left-servo');"
"  const rightSlider = document.getElementById('right-servo');"
"  document.getElementById('left-angle-value').textContent = leftSlider.value + '°';"
"  document.getElementById('right-angle-value').textContent = rightSlider.value + '°';"
"}"
""
"// 更新角度控制"
"function updateAngle(side) {"
"  const slider = document.getElementById(side + '-servo');"
"  const angle = parseInt(slider.value);"
"  const speed = parseInt(document.getElementById(side + '-speed').value);"
"  const servoId = side === 'left' ? 0 : 1;"
"  "
"  fetch('/set_servo?servo_id=' + servoId + '&angle=' + angle + '&speed=' + speed);"
"  updateAngleDisplay();"
"  "
"  // 对称锁定时同步另一侧"
"  if (symmetricLock) {"
"    const otherSide = side === 'left' ? 'right' : 'left';"
"    document.getElementById(otherSide + '-servo').value = angle;"
"    document.getElementById(otherSide + '-angle-value').textContent = angle + '°';"
"    // 不发送同步命令，让服务器处理对称逻辑"
"  }"
"}"
""
"// 更新速度"
"function updateSpeed(side) {"
"  const angle = parseInt(document.getElementById(side + '-servo').value);"
"  const speed = parseInt(document.getElementById(side + '-speed').value);"
"  const servoId = side === 'left' ? 0 : 1;"
"  fetch('/set_servo?servo_id=' + servoId + '&angle=' + angle + '&speed=' + speed);"
"}"
""
"// 更新当前角度指示器"
"function updateCurrentIndicator(side, currentAngle) {"
"  if (currentAngle < 0) {"
"    document.getElementById(side + '-current-fill').style.width = '0%';"
"    return;"
"  }"
"  "
"  const min = minAngle;"
"  const max = maxAngle;"
"  const range = max - min;"
"  if (range <= 0) return;"
"  "
"  let percentage = ((currentAngle - min) / range) * 100;"
"  if (percentage < 0) percentage = 0;"
"  if (percentage > 100) percentage = 100;"
"  "
"  document.getElementById(side + '-current-fill').style.width = percentage + '%';"
"}"
""
"// 电机控制"
"function motorAction(action) {"
"  fetch('/motor?action=' + action);"
"}"
""
"// 序列控制"
"function playSequence() {"
"  fetch('/sequence/play');"
"}"
""
"function clearSequence() {"
"  if (confirm('确定要清除所有序列步骤吗？')) {"
"    fetch('/sequence/clear');"
"  }"
"}"
""
"// 轮询当前角度更新指示器"
"setInterval(() => {"
"  fetch('/get_angles')"
"    .then(response => response.json())"
"    .then(data => {"
"      updateCurrentIndicator('left', data.left);"
"      updateCurrentIndicator('right', data.right);"
"      "
"      // 如果有有效回传角度，更新控制滑条（仅当不在手动拖动时）"
"      const leftSlider = document.getElementById('left-servo');"
"      const rightSlider = document.getElementById('right-servo');"
"      "
"      if (data.left >= 0 && !leftSlider.matches(':active')) {"
"        leftSlider.value = data.left;"
"        document.getElementById('left-angle-value').textContent = data.left + '°';"
"      }"
"      if (data.right >= 0 && !rightSlider.matches(':active')) {"
"        rightSlider.value = data.right;"
"        document.getElementById('right-angle-value').textContent = data.right + '°';"
"      }"
"    })"
"    .catch(error => {"
"      // 如果无法获取角度，保持指示器为空"
"      updateCurrentIndicator('left', -1);"
"      updateCurrentIndicator('right', -1);"
"    });"
"}, 500);"
""
"// 页面加载完成时初始化"
"document.addEventListener('DOMContentLoaded', function() {"
"  initSliderRanges();"
"});"
"</script>"
"</body>"
"</html>";

// Favicon处理（避免404错误）
static esp_err_t favicon_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_sendstr(req, "");
}

// 根页面处理
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, main_html, HTTPD_RESP_USE_STRLEN);
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
        if (httpd_query_key_value(buf, "locked", buf, buf_len) == ESP_OK) {
            int locked = atoi(buf);
            set_symmetric_lock(locked != 0);
            httpd_resp_sendstr(req, "Symmetric lock updated");
        } else {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_sendstr(req, "Missing locked parameter");
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
    
    static const httpd_uri_t get_angles_uri = {
        .uri       = "/get_angles",
        .method    = HTTP_GET,
        .handler   = get_angles_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t set_limits_uri = {
        .uri       = "/set_limits",
        .method    = HTTP_GET,
        .handler   = set_limits_handler,
        .user_ctx  = NULL
    };
    
    static const httpd_uri_t set_symmetric_uri = {
        .uri       = "/set_symmetric",
        .method    = HTTP_GET,
        .handler   = set_symmetric_handler,
        .user_ctx  = NULL
    };
    
    // 添加favicon处理以避免404错误
    static const httpd_uri_t favicon_uri = {
        .uri       = "/favicon.ico",
        .method    = HTTP_GET,
        .handler   = favicon_handler,
        .user_ctx  = NULL
    };
    
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &set_servo_uri);
    httpd_register_uri_handler(server, &motor_uri);
    httpd_register_uri_handler(server, &get_angles_uri);
    httpd_register_uri_handler(server, &set_limits_uri);
    httpd_register_uri_handler(server, &set_symmetric_uri);
    httpd_register_uri_handler(server, &favicon_uri);
}