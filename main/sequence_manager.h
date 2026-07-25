#ifndef SEQUENCE_MANAGER_H
#define SEQUENCE_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// 最大序列步骤数
#define MAX_SEQUENCE_STEPS 10

// 动作帧结构
typedef struct {
    int left_angle;      // 左侧舵机角度 (120-150)
    int right_angle;     // 右侧舵机角度 (120-150)
    uint32_t timestamp;  // 时间戳 (毫秒)
    bool valid;          // 步骤是否有效
} action_frame_t;

// 序列管理器状态枚举
typedef enum {
    SEQ_STATE_IDLE = 0,      // 空闲状态（正常操作）
    SEQ_STATE_RECORDING,     // 录制状态
    SEQ_STATE_EDITING        // 编辑状态（保留扩展性）
} sequence_state_t;

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 序列管理器结构体
typedef struct {
    action_frame_t frames[MAX_SEQUENCE_STEPS];  // 动作帧数组
    int frame_count;                           // 当前帧数量
    sequence_state_t state;                    // 当前状态
    uint32_t start_time;                       // 录制开始时间戳
    bool initialized;                          // 是否已初始化
    bool stop_play_requested;                  // 请求停止播放的标志
    TaskHandle_t play_task_handle;             // 播放任务句柄
    esp_err_t (*servo_set_callback)(int servo_id, int angle, int speed); // 舵机回调
} sequence_manager_t;

// 函数声明
/**
 * @brief 初始化序列管理器
 * @param mgr 序列管理器指针
 * @return ESP_OK 成功，其他失败
 */
esp_err_t seq_mgr_init(sequence_manager_t* mgr);

/**
 * @brief 开始录制模式
 * @param mgr 序列管理器指针
 * @return ESP_OK 成功，ESP_ERR_INVALID_STATE 如果已在录制中
 */
esp_err_t seq_mgr_start_recording(sequence_manager_t* mgr);

/**
 * @brief 记录当前动作帧
 * @param mgr 序列管理器指针
 * @param left_angle 左侧舵机角度
 * @param right_angle 右侧舵机角度
 * @param current_timestamp 当前时间戳
 * @return ESP_OK 成功，ESP_ERR_NO_MEM 如果序列已满，ESP_ERR_INVALID_STATE 如果不在录制状态
 */
esp_err_t seq_mgr_record_frame(sequence_manager_t* mgr, int left_angle, int right_angle, uint32_t current_timestamp);

/**
 * @brief 停止录制模式
 * @param mgr 序列管理器指针
 * @return ESP_OK 成功，ESP_ERR_INVALID_STATE 如果不在录制状态
 */
esp_err_t seq_mgr_stop_recording(sequence_manager_t* mgr);

/**
 * @brief 撤回上一步（删除最后一个动作帧）
 * @param mgr 序列管理器指针
 * @return ESP_OK 成功，ESP_ERR_INVALID_STATE 如果没有可撤回的步骤
 */
esp_err_t seq_mgr_undo_last_frame(sequence_manager_t* mgr);

/**
 * @brief 清空所有序列数据
 * @param mgr 序列管理器指针
 * @return ESP_OK 总是成功
 */
esp_err_t seq_mgr_clear_all(sequence_manager_t* mgr);

/**
 * @brief 获取当前序列状态信息
 * @param mgr 序列管理器指针
 * @param[out] state 当前状态
 * @param[out] frame_count 当前帧数量
 * @param[out] is_full 是否已满
 * @return ESP_OK 成功
 */
esp_err_t seq_mgr_get_status(sequence_manager_t* mgr, sequence_state_t* state, int* frame_count, bool* is_full);

/**
 * @brief 获取指定索引的动作帧信息
 * @param mgr 序列管理器指针
 * @param index 帧索引 (0-based)
 * @param[out] frame 返回的动作帧
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 如果索引无效
 */
esp_err_t seq_mgr_get_frame(sequence_manager_t* mgr, int index, action_frame_t* frame);

/**
 * @brief 设置默认开合动作序列（120° ↔ 150°，5秒间隔）
 * @param mgr 序列管理器指针
 * @return ESP_OK 成功，ESP_ERR_NO_MEM 如果空间不足
 */
esp_err_t seq_mgr_set_default_sequence(sequence_manager_t* mgr);

/**
 * @brief 播放当前序列（模拟执行）
 * @param mgr 序列管理器指针
 * @param servo_set_callback 舵机设置回调函数指针
 * @return ESP_OK 成功，ESP_ERR_INVALID_STATE 如果序列为空
 */
esp_err_t seq_mgr_play_sequence(sequence_manager_t* mgr, esp_err_t (*servo_set_callback)(int servo_id, int angle, int speed));

/**
 * @brief 请求停止播放
 * @param mgr 序列管理器指针
 * @return ESP_OK 成功
 */
esp_err_t seq_mgr_request_stop_play(sequence_manager_t* mgr);

#endif // SEQUENCE_MANAGER_H