#include "wings_control_ux_enhanced.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "io_extension.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wings_control";

// 全局变量定义
servo_controller_t g_servo_ctrl = {0};
motor_controller_t g_motor_ctrl = {0};
sequence_controller_t g_sequence_ctrl = {0};
int g_min_angle = 120;
int g_max_angle = 150;
bool g_symmetric_lock = true;

// 声明LCD更新函数
void update_lcd_angles(int left_angle, int right_angle, int min_angle, int max_angle);
void update_lcd_progress(const char* progress);
void refresh_lcd_display(void);

// 初始化舵机控制器
void servo_init(servo_controller_t* ctrl) {
    // 初始化UART0用于舵机通信 (GPIO21 = UART0_TX)
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 21, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0));
    
    ctrl->initialized = true;
    ESP_LOGI(TAG, "Servo controller initialized on UART0 (GPIO21)");
}

// 发送众灵舵机命令
esp_err_t servo_send_command(servo_controller_t* ctrl, const char* command) {
    if (!ctrl->initialized) {
        return ESP_FAIL;
    }
    
    size_t len = strlen(command);
    if (len > 0) {
        uart_write_bytes(UART_NUM_0, command, len);
        uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(100));
        return ESP_OK;
    }
    return ESP_FAIL;
}

// 设置单个舵机角度和速度
esp_err_t servo_set_angle_and_speed(servo_controller_t* ctrl, int servo_id, int angle, int speed) {
    if (servo_id < 0 || servo_id > 1) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 角度范围检查和限制
    if (angle < g_min_angle) angle = g_min_angle;
    if (angle > g_max_angle) angle = g_max_angle;
    
    // 将角度转换为PWM值 (120°=1200, 150°=1500, 270°=2700)
    int pwm_value = angle * 10;
    if (pwm_value < 500) pwm_value = 500;
    if (pwm_value > 2500) pwm_value = 2500;
    
    // 速度范围限制
    if (speed < 100) speed = 100;
    if (speed > 1500) speed = 1500;
    
    char command[32];
    snprintf(command, sizeof(command), "#%03dP%04dT%04d!", servo_id, pwm_value, speed);
    
    esp_err_t ret = servo_send_command(ctrl, command);
    if (ret == ESP_OK) {
        ctrl->current_angles[servo_id] = angle;
        ctrl->current_speeds[servo_id] = speed;
        
        // 更新LCD显示
        int other_servo = 1 - servo_id;
        int other_angle = ctrl->current_angles[other_servo];
        if (other_angle < 0) other_angle = 135;
        update_lcd_angles(
            servo_id == 0 ? angle : other_angle,
            servo_id == 1 ? angle : other_angle,
            g_min_angle, g_max_angle
        );
        refresh_lcd_display();
    }
    
    return ret;
}

// 解锁舵机（设置速度为0）
esp_err_t servo_unlock(servo_controller_t* ctrl, int servo_id) {
    if (servo_id < 0 || servo_id > 1) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char command[32];
    snprintf(command, sizeof(command), "#%03dP%04dT0000!", servo_id, ctrl->current_angles[servo_id] * 10);
    return servo_send_command(ctrl, command);
}

// 读取当前角度（模拟回读）
int servo_read_angle(servo_controller_t* ctrl, int servo_id) {
    if (servo_id < 0 || servo_id > 1) {
        return -1;
    }
    return ctrl->current_angles[servo_id];
}

// 设置角度限制
void set_angle_limits(int min_angle, int max_angle) {
    if (min_angle >= 0 && min_angle <= 270 && max_angle >= 0 && max_angle <= 270 && min_angle < max_angle) {
        g_min_angle = min_angle;
        g_max_angle = max_angle;
        
        // 确保当前角度在新范围内
        if (g_servo_ctrl.current_angles[0] < g_min_angle) g_servo_ctrl.current_angles[0] = g_min_angle;
        if (g_servo_ctrl.current_angles[0] > g_max_angle) g_servo_ctrl.current_angles[0] = g_max_angle;
        if (g_servo_ctrl.current_angles[1] < g_min_angle) g_servo_ctrl.current_angles[1] = g_min_angle;
        if (g_servo_ctrl.current_angles[1] > g_max_angle) g_servo_ctrl.current_angles[1] = g_max_angle;
    }
}

// 设置对称锁定
void set_symmetric_lock(bool locked) {
    g_symmetric_lock = locked;
    if (locked) {
        // 同步角度：右 = 270 - 左
        int left_angle = g_servo_ctrl.current_angles[0];
        if (left_angle >= 0) {
            int right_angle = 270 - left_angle;
            // 确保右角度在限制范围内
            if (right_angle < g_min_angle) right_angle = g_min_angle;
            if (right_angle > g_max_angle) right_angle = g_max_angle;
            g_servo_ctrl.current_angles[1] = right_angle;
            
            // 发送同步命令
            servo_set_angle_and_speed(&g_servo_ctrl, 1, right_angle, g_servo_ctrl.current_speeds[1]);
        }
    }
}

// 初始化电机控制器
void motor_init(motor_controller_t* ctrl) {
    // L298N电机控制使用EXIO4(EXIO4/PD4)和EXIO5(EXIO5/PD5)
    // 通过PCA9555 I2C GPIO扩展器控制
    ctrl->initialized = true;
    ESP_LOGI(TAG, "Motor controller initialized (EXIO4/EXIO5 -> IO_4/IO_5)");
}

// 展开翅膀（电机正转）- 使用正确的IO_EXTENSION_Output API
void motor_expand_wings(motor_controller_t* ctrl) {
    if (!ctrl->initialized) return;
    
    // 设置EXIO4(IO_4)=1, EXIO5(IO_5)=0 (正转)
    IO_EXTENSION_Output(IO_EXTENSION_IO_4, 1);  // PD4 = 1
    IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);  // PD5 = 0
    
    ctrl->current_state = MOTOR_STATE_EXPANDING;
    ESP_LOGI(TAG, "Motor: Expanding wings");
}

// 闭合翅膀（电机反转）- 使用正确的IO_EXTENSION_Output API
void motor_close_wings(motor_controller_t* ctrl) {
    if (!ctrl->initialized) return;
    
    // 设置EXIO4(IO_4)=0, EXIO5(IO_5)=1 (反转)
    IO_EXTENSION_Output(IO_EXTENSION_IO_4, 0);  // PD4 = 0
    IO_EXTENSION_Output(IO_EXTENSION_IO_5, 1);  // PD5 = 1
    
    ctrl->current_state = MOTOR_STATE_CLOSING;
    ESP_LOGI(TAG, "Motor: Closing wings");
}

// 复位电机 - 使用正确的IO_EXTENSION_Output API
void motor_reset_wings(motor_controller_t* ctrl) {
    if (!ctrl->initialized) return;
    
    // 设置EXIO4(IO_4)=1, EXIO5(IO_5)=1 (复位状态)
    IO_EXTENSION_Output(IO_EXTENSION_IO_4, 1);  // PD4 = 1
    IO_EXTENSION_Output(IO_EXTENSION_IO_5, 1);  // PD5 = 1
    
    ctrl->current_state = MOTOR_STATE_RESET;
    ESP_LOGI(TAG, "Motor: Reset wings");
}

// 停止电机 - 使用正确的IO_EXTENSION_Output API
void motor_stop(motor_controller_t* ctrl) {
    if (!ctrl->initialized) return;
    
    // 设置EXIO4(IO_4)=0, EXIO5(IO_5)=0 (停止)
    IO_EXTENSION_Output(IO_EXTENSION_IO_4, 0);  // PD4 = 0
    IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);  // PD5 = 0
    
    ctrl->current_state = MOTOR_STATE_STOPPED;
    ESP_LOGI(TAG, "Motor: Stopped");
}

// 初始化序列控制器
void sequence_init(sequence_controller_t* ctrl) {
    memset(ctrl->steps, 0, sizeof(ctrl->steps));
    ctrl->step_count = 0;
    ctrl->editing = false;
    ctrl->initialized = true;
    ESP_LOGI(TAG, "Sequence controller initialized");
}

// 开始编辑序列
int sequence_start_editing(sequence_controller_t* ctrl) {
    if (ctrl->step_count >= MAX_SEQUENCE_STEPS) {
        return -1; // 序列已满
    }
    ctrl->editing = true;
    return ctrl->step_count; // 返回下一个步骤索引
}

// 保存序列步骤
bool sequence_save_step(sequence_controller_t* ctrl, int left_angle, int right_angle, int delay_ms) {
    if (!ctrl->editing || ctrl->step_count >= MAX_SEQUENCE_STEPS) {
        return false;
    }
    
    ctrl->steps[ctrl->step_count].left_angle = left_angle;
    ctrl->steps[ctrl->step_count].right_angle = right_angle;
    ctrl->steps[ctrl->step_count].delay_ms = delay_ms;
    ctrl->steps[ctrl->step_count].valid = true;
    ctrl->step_count++;
    ctrl->editing = false;
    
    return true;
}

// 取消当前编辑
void sequence_cancel_editing(sequence_controller_t* ctrl) {
    ctrl->editing = false;
}

// 清除所有序列
void sequence_clear(sequence_controller_t* ctrl) {
    memset(ctrl->steps, 0, sizeof(ctrl->steps));
    ctrl->step_count = 0;
    ctrl->editing = false;
}

// 播放序列
bool sequence_play(sequence_controller_t* ctrl) {
    if (ctrl->step_count == 0) {
        return false;
    }
    
    for (int i = 0; i < ctrl->step_count; i++) {
        if (ctrl->steps[i].valid) {
            // 设置左舵机
            servo_set_angle_and_speed(&g_servo_ctrl, 0, 
                ctrl->steps[i].left_angle, 
                ctrl->steps[i].delay_ms > 100 ? ctrl->steps[i].delay_ms : 100);
            
            // 设置右舵机
            servo_set_angle_and_speed(&g_servo_ctrl, 1, 
                ctrl->steps[i].right_angle, 
                ctrl->steps[i].delay_ms > 100 ? ctrl->steps[i].delay_ms : 100);
            
            // 延时
            vTaskDelay(pdMS_TO_TICKS(ctrl->steps[i].delay_ms));
            
            // 更新LCD进度
            char progress[64];
            snprintf(progress, sizeof(progress), "播放步骤 %d/%d", i+1, ctrl->step_count);
            update_lcd_progress(progress);
            refresh_lcd_display();
        }
    }
    
    update_lcd_progress("序列播放完成");
    refresh_lcd_display();
    return true;
}

// 检查序列是否已满
bool sequence_is_full(sequence_controller_t* ctrl) {
    return ctrl->step_count >= MAX_SEQUENCE_STEPS;
}

// 翅膀控制主任务
void wings_control_task(void* pvParameters) {
    // 初始化控制器
    servo_init(&g_servo_ctrl);
    motor_init(&g_motor_ctrl);
    sequence_init(&g_sequence_ctrl);
    
    // 设置默认角度
    g_servo_ctrl.current_angles[0] = 135;
    g_servo_ctrl.current_angles[1] = 135;
    g_servo_ctrl.current_speeds[0] = 100;
    g_servo_ctrl.current_speeds[1] = 100;
    
    // 发送初始同步命令
    servo_set_angle_and_speed(&g_servo_ctrl, 0, 135, 100);
    servo_set_angle_and_speed(&g_servo_ctrl, 1, 135, 100);
    
    // 更新LCD初始状态
    update_lcd_angles(135, 135, g_min_angle, g_max_angle);
    update_lcd_progress("系统就绪");
    refresh_lcd_display();
    
    ESP_LOGI(TAG, "Wings control task started");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // 保持任务运行
    }
}