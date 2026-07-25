#include "wings_control_esp5_noimu.h"
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

// 舵机初始化（基于样例代码）
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

// 发送舵机命令（基于样例协议）
static void servo_send_command(servo_controller_t *ctrl, const char* command) {
    uart_write_bytes(ctrl->uart_num, command, strlen(command));
    vTaskDelay(pdMS_TO_TICKS(50));
}

// 读取舵机响应
static char* servo_read_response(servo_controller_t *ctrl) {
    static char response[64];
    memset(response, 0, sizeof(response));
    
    uint32_t start_time = xTaskGetTickCount();
    int index = 0;
    
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(1000)) {
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

// 设置舵机角度（基于样例的众灵总线协议）
esp_err_t servo_set_angle(servo_controller_t *ctrl, int servo_id, int angle) {
    if (!ctrl->initialized || angle < 0 || angle > 270) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 使用样例中的位置计算公式
    int position = 1500 + (angle - 135) * 11.11;
    char command[32];
    snprintf(command, sizeof(command), "#%03dP%dT1000!", servo_id, position);
    
    servo_send_command(ctrl, command);
    char* response = servo_read_response(ctrl);
    
    if (strstr(response, "#OK!")) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

// 读取舵机角度
int servo_read_angle(servo_controller_t *ctrl, int servo_id) {
    if (!ctrl->initialized) {
        return -1;
    }
    
    char command[20];
    snprintf(command, sizeof(command), "#%03dPRAD!", servo_id);
    servo_send_command(ctrl, command);
    char* response = servo_read_response(ctrl);
    
    char* p_start = strchr(response, 'P');
    char* p_end = strchr(response, '!');
    
    if (p_start && p_end && p_end > p_start) {
        char position_str[10];
        int len = p_end - p_start - 1;
        if (len < sizeof(position_str)) {
            strncpy(position_str, p_start + 1, len);
            position_str[len] = '\0';
            int position = atoi(position_str);
            return 135 + (position - 1500) / 11.11;
        }
    }
    return -1;
}

// 电机初始化（使用IO_EXTENSION，基于样例）
esp_err_t motor_init(motor_controller_t *ctrl) {
    // IO_EXTENSION已经在main.c中初始化
    ctrl->initialized = true;
    return ESP_OK;
}

// 设置电机方向（使用IO_EXTENSION输出，完全基于样例）
static void motor_set_direction(motor_controller_t *ctrl, bool in1_high, bool in2_high) {
    if (!ctrl->initialized) return;
    
    // 使用样例中的IO_EXTENSION_Output函数
    if (in1_high) {
        IO_EXTENSION_Output(IO_EXTENSION_IO_4, 1);  // EXIO4高电平
    } else {
        IO_EXTENSION_Output(IO_EXTENSION_IO_4, 0);  // EXIO4低电平
    }
    
    if (in2_high) {
        IO_EXTENSION_Output(IO_EXTENSION_IO_5, 1);  // EXIO5高电平
    } else {
        IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);  // EXIO5低电平
    }
}

// 展开翅膀（正转2秒，基于样例逻辑）
void motor_expand_wings(motor_controller_t *ctrl) {
    if (!ctrl->initialized) return;
    motor_set_direction(ctrl, true, false); // IN1高, IN2低 = 正转
    vTaskDelay(pdMS_TO_TICKS(4000));
    motor_stop(ctrl);
}

// 闭合翅膀（反转2秒，基于样例逻辑）  
void motor_close_wings(motor_controller_t *ctrl) {
    if (!ctrl->initialized) return;
    motor_set_direction(ctrl, false, true); // IN1低, IN2高 = 反转
    vTaskDelay(pdMS_TO_TICKS(4000));
    motor_stop(ctrl);
}

// 复位翅膀（基于样例）
void motor_reset_wings(motor_controller_t *ctrl) {
    motor_close_wings(ctrl);
}

// 停止电机（基于样例）
void motor_stop(motor_controller_t *ctrl) {
    if (!ctrl->initialized) return;
    motor_set_direction(ctrl, false, false); // 两个都低 = 停止
}

// 翅膀控制主任务（无IMU，基于样例硬件配置）
void wings_control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Wings control task started (ESP-IDF 5.4.1 No-IMU version)");
    
    // 初始化舵机（基于样例引脚配置）
    if (servo_init(&g_servo_ctrl) != ESP_OK) {
        ESP_LOGE(TAG, "Servo initialization failed");
    } else {
        ESP_LOGI(TAG, "Servo initialized on GPIO21/GPIO20");
        servo_set_angle(&g_servo_ctrl, 0, 135);
        servo_set_angle(&g_servo_ctrl, 1, 135);
    }
    
    // 初始化电机（使用IO_EXTENSION，基于样例EXIO4/EXIO5配置）
    if (motor_init(&g_motor_ctrl) != ESP_OK) {
        ESP_LOGE(TAG, "Motor initialization failed");
    } else {
        ESP_LOGI(TAG, "Motor initialized using EXIO4/EXIO5 (IO_EXTENSION)");
        motor_reset_wings(&g_motor_ctrl);
    }
    
    // 主循环（无IMU自动控制，纯手动）
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}