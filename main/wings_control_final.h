#ifndef WINGS_CONTROL_FINAL_H
#define WINGS_CONTROL_FINAL_H

#include "esp_err.h"
#include "driver/uart.h"
#include "io_extension.h"

// 舵机控制相关
typedef struct {
    uart_port_t uart_num;
    int baud_rate;
    bool initialized;
} servo_controller_t;

// 电机控制相关  
typedef struct {
    uint8_t i2c_addr;  // PCA9555地址
    bool initialized;
} motor_controller_t;

// 多步动作编程相关
#define MAX_SEQUENCE_STEPS 10
typedef struct {
    int left_angle;      // 左舵机角度
    int right_angle;     // 右舵机角度  
    int left_speed;      // 左舵机速度 (500-2500)
    int right_speed;     // 右舵机速度 (500-2500)
    int delay_ms;        // 延时毫秒
    bool valid;          // 步骤是否有效
} sequence_step_t;

typedef struct {
    sequence_step_t steps[MAX_SEQUENCE_STEPS];
    int current_step;    // 当前编辑步骤 (0-9, -1表示未编辑)
    bool playing;        // 是否正在播放序列
    bool editing;        // 是否正在编辑模式
} sequence_controller_t;

// 函数声明
void wings_control_task(void *pvParameters);

// 舵机控制函数
esp_err_t servo_init(servo_controller_t *ctrl);
esp_err_t servo_set_angle_and_speed(servo_controller_t *ctrl, int servo_id, int angle, int speed);
esp_err_t servo_unlock(servo_controller_t *ctrl, int servo_id);
int servo_read_angle(servo_controller_t *ctrl, int servo_id);
char* servo_send_and_receive(servo_controller_t *ctrl, const char* command);

// 电机控制函数
esp_err_t motor_init(motor_controller_t *ctrl);
void motor_expand_wings(motor_controller_t *ctrl);
void motor_close_wings(motor_controller_t *ctrl);
void motor_reset_wings(motor_controller_t *ctrl);
void motor_stop(motor_controller_t *ctrl);

// 序列控制函数
void sequence_init(sequence_controller_t *seq_ctrl);
esp_err_t sequence_add_step(sequence_controller_t *seq_ctrl, int left_angle, int right_angle, 
                           int left_speed, int right_speed, int delay_ms);
esp_err_t sequence_play(sequence_controller_t *seq_ctrl, servo_controller_t *servo_ctrl);
esp_err_t sequence_clear(sequence_controller_t *seq_ctrl);
bool sequence_is_full(sequence_controller_t *seq_ctrl);

#endif