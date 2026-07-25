#include "wings_control_final.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "io_extension.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "wings_control";

// 硬件配置（基于原始样例）
#define SERVO_UART_NUM      UART_NUM_0
#define SERVO_UART_TX_PIN   21    // GPIO21 (EXIO3) - 样例中的舵机引脚
#define SERVO_UART_RX_PIN   20    // GPIO20 (EXIO2)

#define PCA9555_ADDR        0x20  // I2C地址
#define MOTOR_IN1_BIT       4     // EXIO4 -> PD4 (Bit 4) - 样例中的电机控制
#define MOTOR_IN2_BIT       5     // EXIO5 -> PD5 (Bit 5)

// 全局控制器实例
static servo_controller_t g_servo_ctrl = {
    .uart_num = SERVO_UART_NUM,
    .baud_rate = 115200,
    .initialized = false
};

static motor_controller_t g_motor_ctrl = {
    .i2c_addr = PCA9555_ADDR,
    .initialized = false
};

static sequence_controller_t g_sequence_ctrl;

// 舵机初始化
esp_err_t servo_init(servo_controller_t *ctrl) {
    uart_config_t uart_config = {
        .baud_rate = ctrl->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_param_config(ctrl->uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ctrl->uart_num, SERVO_UART_TX_PIN, SERVO_UART_RX_PIN, 
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(ctrl->uart_num, 2048, 0, 0, NULL, 0));

    ctrl->initialized = true;
    return ESP_OK;
}

// 发送命令并接收响应（众灵协议）
char* servo_send_and_receive(servo_controller_t *ctrl, const char* command) {
    static char response[128];
    memset(response, 0, sizeof(response));
    
    // 发送命令
    uart_write_bytes(ctrl->uart_num, command, strlen(command));
    vTaskDelay(pdMS_TO_TICKS(100)); // 给舵机足够时间响应
    
    // 读取响应
    uint32_t start_time = xTaskGetTickCount();
    int index = 0;
    
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(2000)) {
        if (uart_read_bytes(ctrl->uart_num, &response[index], 1, portMAX_DELAY) > 0) {
            if (response[index] == '!') {
                response[index + 1] = '\0';
                break;
            }
            index++;
            if (index >= sizeof(response) - 1) break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    return response;
}

// 设置舵机角度和速度（众灵协议格式）
esp_err_t servo_set_angle_and_speed(servo_controller_t *ctrl, int servo_id, int angle, int speed) {
    if (!ctrl->initialized || servo_id < 0 || servo_id > 254) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 角度范围检查和转换
    if (angle < 0) angle = 0;
    if (angle > 270) angle = 270;
    
    // PWM计算: 1500 + (angle - 135) * 11.11
    int pwm = 1500 + (int)((angle - 135) * 11.11);
    if (pwm < 500) pwm = 500;
    if (pwm > 2500) pwm = 2500;
    
    // 速度范围检查
    if (speed < 100) speed = 100;
    if (speed > 1500) speed = 1500;
    
    // 构建众灵协议命令: #000P1500T1000!
    char command[32];
    snprintf(command, sizeof(command), "#%03dP%04dT%04d!", servo_id, pwm, speed);
    
    char* response = servo_send_and_receive(ctrl, command);
    
    if (strstr(response, "#OK!") || strstr(response, "!")) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

// 解锁舵机（众灵协议）
esp_err_t servo_unlock(servo_controller_t *ctrl, int servo_id) {
    if (!ctrl->initialized || servo_id < 0 || servo_id > 254) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char command[20];
    snprintf(command, sizeof(command), "#%03dPULK!", servo_id);
    
    char* response = servo_send_and_receive(ctrl, command);
    
    if (strstr(response, "#OK!") || strstr(response, "!")) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

// 读取舵机当前角度
int servo_read_angle(servo_controller_t *ctrl, int servo_id) {
    if (!ctrl->initialized || servo_id < 0 || servo_id > 254) {
        return -1;
    }
    
    char command[20];
    snprintf(command, sizeof(command), "#%03dPRAD!", servo_id);
    
    char* response = servo_send_and_receive(ctrl, command);
    
    // 解析响应: #000P1500!
    char* p_start = strchr(response, 'P');
    char* p_end = strchr(response, '!');
    
    if (p_start && p_end && p_end > p_start) {
        char pwm_str[10];
        int len = p_end - p_start - 1;
        if (len < sizeof(pwm_str)) {
            strncpy(pwm_str, p_start + 1, len);
            pwm_str[len] = '\0';
            int pwm = atoi(pwm_str);
            // 转换回角度: 135 + (pwm - 1500) / 11.11
            return 135 + (int)((pwm - 1500) / 11.11);
        }
    }
    return -1;
}

// 电机初始化（使用IO_EXTENSION）
esp_err_t motor_init(motor_controller_t *ctrl) {
    ctrl->initialized = true;
    return ESP_OK;
}

// 设置电机方向（使用IO_EXTENSION输出）
static void motor_set_direction(motor_controller_t *ctrl, bool in1_high, bool in2_high) {
    if (!ctrl->initialized) return;
    
    IO_EXTENSION_Output(IO_EXTENSION_EXIO4, in1_high ? 1 : 0);
    IO_EXTENSION_Output(IO_EXTENSION_EXIO5, in2_high ? 1 : 0);
}

// 展开翅膀
void motor_expand_wings(motor_controller_t *ctrl) {
    if (!ctrl->initialized) return;
    motor_set_direction(ctrl, true, false);
    vTaskDelay(pdMS_TO_TICKS(2000));
    motor_stop(ctrl);
}

// 闭合翅膀  
void motor_close_wings(motor_controller_t *ctrl) {
    if (!ctrl->initialized) return;
    motor_set_direction(ctrl, false, true);
    vTaskDelay(pdMS_TO_TICKS(2000));
    motor_stop(ctrl);
}

// 复位翅膀
void motor_reset_wings(motor_controller_t *ctrl) {
    motor_close_wings(ctrl);
}

// 停止电机
void motor_stop(motor_controller_t *ctrl) {
    if (!ctrl->initialized) return;
    motor_set_direction(ctrl, false, false);
}

// 序列控制初始化
void sequence_init(sequence_controller_t *seq_ctrl) {
    memset(seq_ctrl, 0, sizeof(sequence_controller_t));
    seq_ctrl->current_step = -1;
    seq_ctrl->playing = false;
    seq_ctrl->editing = false;
}

// 添加序列步骤
esp_err_t sequence_add_step(sequence_controller_t *seq_ctrl, int left_angle, int right_angle, 
                           int left_speed, int right_speed, int delay_ms) {
    if (seq_ctrl->current_step < 0 || seq_ctrl->current_step >= MAX_SEQUENCE_STEPS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    seq_ctrl->steps[seq_ctrl->current_step].left_angle = left_angle;
    seq_ctrl->steps[seq_ctrl->current_step].right_angle = right_angle;
    seq_ctrl->steps[seq_ctrl->current_step].left_speed = left_speed;
    seq_ctrl->steps[seq_ctrl->current_step].right_speed = right_speed;
    seq_ctrl->steps[seq_ctrl->current_step].delay_ms = delay_ms;
    seq_ctrl->steps[seq_ctrl->current_step].valid = true;
    
    return ESP_OK;
}

// 播放序列
esp_err_t sequence_play(sequence_controller_t *seq_ctrl, servo_controller_t *servo_ctrl) {
    if (seq_ctrl->playing) {
        return ESP_ERR_INVALID_STATE;
    }
    
    seq_ctrl->playing = true;
    
    for (int i = 0; i < MAX_SEQUENCE_STEPS; i++) {
        if (!seq_ctrl->steps[i].valid) break;
        
        // 同步控制：右舵机角度取反（对称安装）
        int right_angle_symmetric = 270 - seq_ctrl->steps[i].right_angle;
        
        // 发送同步命令: {#000PxxxxTxxxx!#001PxxxxTxxxx!}
        char sync_command[80];
        int left_pwm = 1500 + (int)((seq_ctrl->steps[i].left_angle - 135) * 11.11);
        int right_pwm = 1500 + (int)((right_angle_symmetric - 135) * 11.11);
        
        if (left_pwm < 500) left_pwm = 500;
        if (left_pwm > 2500) left_pwm = 2500;
        if (right_pwm < 500) right_pwm = 500;
        if (right_pwm > 2500) right_pwm = 2500;
        
        snprintf(sync_command, sizeof(sync_command), 
                "{#000P%04dT%04d!#001P%04dT%04d!}", 
                left_pwm, seq_ctrl->steps[i].left_speed,
                right_pwm, seq_ctrl->steps[i].right_speed);
        
        servo_send_and_receive(servo_ctrl, sync_command);
        vTaskDelay(pdMS_TO_TICKS(seq_ctrl->steps[i].delay_ms));
    }
    
    seq_ctrl->playing = false;
    return ESP_OK;
}

// 清除序列
esp_err_t sequence_clear(sequence_controller_t *seq_ctrl) {
    memset(seq_ctrl->steps, 0, sizeof(seq_ctrl->steps));
    seq_ctrl->current_step = -1;
    seq_ctrl->editing = false;
    return ESP_OK;
}

// 检查序列是否已满
bool sequence_is_full(sequence_controller_t *seq_ctrl) {
    for (int i = 0; i < MAX_SEQUENCE_STEPS; i++) {
        if (!seq_ctrl->steps[i].valid) {
            return false;
        }
    }
    return true;
}

// 翅膀控制主任务
void wings_control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Wings control task started (Final version with sequence programming)");
    
    // 初始化舵机
    if (servo_init(&g_servo_ctrl) != ESP_OK) {
        ESP_LOGE(TAG, "Servo initialization failed");
    } else {
        ESP_LOGI(TAG, "Servo initialized on GPIO21/GPIO20");
        servo_set_angle_and_speed(&g_servo_ctrl, 0, 135, 1000);
        servo_set_angle_and_speed(&g_servo_ctrl, 1, 135, 1000);
    }
    
    // 初始化电机
    if (motor_init(&g_motor_ctrl) != ESP_OK) {
        ESP_LOGE(TAG, "Motor initialization failed");
    } else {
        ESP_LOGI(TAG, "Motor initialized using EXIO4/EXIO5");
        motor_reset_wings(&g_motor_ctrl);
    }
    
    // 初始化序列控制器
    sequence_init(&g_sequence_ctrl);
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}