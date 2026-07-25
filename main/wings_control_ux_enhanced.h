#ifndef WINGS_CONTROL_UX_ENHANCED_H
#define WINGS_CONTROL_UX_ENHANCED_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 最大序列步骤数
#define MAX_SEQUENCE_STEPS 10

// 舵机控制器结构
typedef struct {
    bool initialized;
    int current_angles[2];   // [0]=左舵机, [1]=右舵机
    int current_speeds[2];
} servo_controller_t;

// 电机状态枚举
typedef enum {
    MOTOR_STATE_STOPPED = 0,
    MOTOR_STATE_EXPANDING,
    MOTOR_STATE_CLOSING,
    MOTOR_STATE_RESET
} motor_state_t;

// 电机控制器结构
typedef struct {
    bool initialized;
    motor_state_t current_state;
} motor_controller_t;

// 序列步骤结构
typedef struct {
    int left_angle;
    int right_angle;
    int delay_ms;
    bool valid;
} sequence_step_t;

// 序列控制器结构
typedef struct {
    sequence_step_t steps[MAX_SEQUENCE_STEPS];
    int step_count;
    bool editing;
    bool initialized;
} sequence_controller_t;

// 函数声明
void servo_init(servo_controller_t* ctrl);
esp_err_t servo_send_command(servo_controller_t* ctrl, const char* command);
esp_err_t servo_set_angle_and_speed(servo_controller_t* ctrl, int servo_id, int angle, int speed);
esp_err_t servo_unlock(servo_controller_t* ctrl, int servo_id);
int servo_read_angle(servo_controller_t* ctrl, int servo_id);

void set_angle_limits(int min_angle, int max_angle);
void set_symmetric_lock(bool locked);

void motor_init(motor_controller_t* ctrl);
void motor_expand_wings(motor_controller_t* ctrl);
void motor_close_wings(motor_controller_t* ctrl);
void motor_reset_wings(motor_controller_t* ctrl);
void motor_stop(motor_controller_t* ctrl);

void sequence_init(sequence_controller_t* ctrl);
int sequence_start_editing(sequence_controller_t* ctrl);
bool sequence_save_step(sequence_controller_t* ctrl, int left_angle, int right_angle, int delay_ms);
void sequence_cancel_editing(sequence_controller_t* ctrl);
void sequence_clear(sequence_controller_t* ctrl);
bool sequence_play(sequence_controller_t* ctrl);
bool sequence_is_full(sequence_controller_t* ctrl);

void wings_control_task(void* pvParameters);

// 全局变量声明
extern servo_controller_t g_servo_ctrl;
extern motor_controller_t g_motor_ctrl;
extern sequence_controller_t g_sequence_ctrl;
extern int g_min_angle;
extern int g_max_angle;
extern bool g_symmetric_lock;

#endif // WINGS_CONTROL_UX_ENHANCED_H