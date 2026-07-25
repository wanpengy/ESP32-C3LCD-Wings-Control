#ifndef WINGS_CONTROL_ESPIDF5_NOIMU_H
#define WINGS_CONTROL_ESPIDF5_NOIMU_H

#include "esp_err.h"
#include "driver/uart.h"
#include "io_extension.h"  // 使用IO_EXTENSION而不是i2c_master

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

// 函数声明
void wings_control_task(void *pvParameters);

// 舵机控制函数
esp_err_t servo_init(servo_controller_t *ctrl);
esp_err_t servo_set_angle(servo_controller_t *ctrl, int servo_id, int angle);
int servo_read_angle(servo_controller_t *ctrl, int servo_id);

// 电机控制函数
esp_err_t motor_init(motor_controller_t *ctrl);
void motor_expand_wings(motor_controller_t *ctrl);
void motor_close_wings(motor_controller_t *ctrl);
void motor_reset_wings(motor_controller_t *ctrl);
void motor_stop(motor_controller_t *ctrl);

#endif