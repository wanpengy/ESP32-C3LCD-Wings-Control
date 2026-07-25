#include "sequence_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "sequence_manager";

// 内部辅助函数声明
static uint32_t get_current_timestamp(void);

esp_err_t seq_mgr_init(sequence_manager_t* mgr) {
    if (!mgr) {
        ESP_LOGE(TAG, "Invalid manager pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(mgr->frames, 0, sizeof(mgr->frames));
    mgr->frame_count = 0;
    mgr->state = SEQ_STATE_IDLE;
    mgr->start_time = 0;
    mgr->initialized = true;
    mgr->stop_play_requested = false;
    mgr->play_task_handle = NULL;
    mgr->servo_set_callback = NULL;
    
    ESP_LOGI(TAG, "Sequence manager initialized");
    return ESP_OK;
}

esp_err_t seq_mgr_start_recording(sequence_manager_t* mgr) {
    if (!mgr || !mgr->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (mgr->state == SEQ_STATE_RECORDING) {
        ESP_LOGW(TAG, "Already in recording state");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 清空现有数据
    memset(mgr->frames, 0, sizeof(mgr->frames));
    mgr->frame_count = 0;
    mgr->state = SEQ_STATE_RECORDING;
    mgr->start_time = get_current_timestamp();
    
    ESP_LOGI(TAG, "Recording started, start_time: %lu", mgr->start_time);
    return ESP_OK;
}

esp_err_t seq_mgr_record_frame(sequence_manager_t* mgr, int left_angle, int right_angle, uint32_t current_timestamp) {
    if (!mgr || !mgr->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 验证角度范围
    if (left_angle < 120 || left_angle > 150 || right_angle < 120 || right_angle > 150) {
        ESP_LOGE(TAG, "Invalid angle range: L=%d, R=%d", left_angle, right_angle);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (mgr->state != SEQ_STATE_RECORDING) {
        ESP_LOGE(TAG, "Not in recording state");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (mgr->frame_count >= MAX_SEQUENCE_STEPS) {
        ESP_LOGE(TAG, "Sequence is full, cannot add more frames");
        return ESP_ERR_NO_MEM;
    }
    
    // 边界检查
    if (mgr->frame_count < 0 || mgr->frame_count >= MAX_SEQUENCE_STEPS) {
        ESP_LOGE(TAG, "Frame count out of bounds: %d", mgr->frame_count);
        return ESP_ERR_INVALID_STATE;
    }
    
    // 记录动作帧
    mgr->frames[mgr->frame_count].left_angle = left_angle;
    mgr->frames[mgr->frame_count].right_angle = right_angle;
    mgr->frames[mgr->frame_count].timestamp = current_timestamp;
    mgr->frames[mgr->frame_count].valid = true;
    mgr->frame_count++;
    
    ESP_LOGI(TAG, "Frame recorded: L=%d, R=%d, timestamp=%lu, total=%d", 
             left_angle, right_angle, current_timestamp, mgr->frame_count);
    return ESP_OK;
}

esp_err_t seq_mgr_stop_recording(sequence_manager_t* mgr) {
    if (!mgr || !mgr->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (mgr->state != SEQ_STATE_RECORDING) {
        ESP_LOGE(TAG, "Not in recording state");
        return ESP_ERR_INVALID_STATE;
    }
    
    mgr->state = SEQ_STATE_IDLE;
    ESP_LOGI(TAG, "Recording stopped, total frames: %d", mgr->frame_count);
    return ESP_OK;
}

esp_err_t seq_mgr_undo_last_frame(sequence_manager_t* mgr) {
    if (!mgr || !mgr->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 只有在录制状态或空闲状态（但有数据）时才能撤回
    if (mgr->frame_count <= 0) {
        ESP_LOGW(TAG, "No frames to undo");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 边界检查
    if (mgr->frame_count > MAX_SEQUENCE_STEPS || mgr->frame_count <= 0) {
        ESP_LOGE(TAG, "Invalid frame count for undo: %d", mgr->frame_count);
        return ESP_ERR_INVALID_STATE;
    }
    
    // 清除最后一个帧
    mgr->frame_count--;
    memset(&mgr->frames[mgr->frame_count], 0, sizeof(action_frame_t));
    
    ESP_LOGI(TAG, "Undo last frame, remaining frames: %d", mgr->frame_count);
    return ESP_OK;
}

esp_err_t seq_mgr_clear_all(sequence_manager_t* mgr) {
    if (!mgr) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(mgr->frames, 0, sizeof(mgr->frames));
    mgr->frame_count = 0;
    mgr->state = SEQ_STATE_IDLE;
    mgr->start_time = 0;
    mgr->stop_play_requested = false;
    mgr->play_task_handle = NULL;
    mgr->servo_set_callback = NULL;
    
    ESP_LOGI(TAG, "All sequence data cleared");
    return ESP_OK;
}

esp_err_t seq_mgr_get_status(sequence_manager_t* mgr, sequence_state_t* state, int* frame_count, bool* is_full) {
    if (!mgr || !mgr->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (state) *state = mgr->state;
    if (frame_count) *frame_count = mgr->frame_count;
    if (is_full) *is_full = (mgr->frame_count >= MAX_SEQUENCE_STEPS);
    
    return ESP_OK;
}

esp_err_t seq_mgr_get_frame(sequence_manager_t* mgr, int index, action_frame_t* frame) {
    if (!mgr || !mgr->initialized || !frame) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 边界检查
    if (index < 0 || index >= mgr->frame_count || index >= MAX_SEQUENCE_STEPS) {
        ESP_LOGE(TAG, "Invalid frame index: %d, total frames: %d", index, mgr->frame_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!mgr->frames[index].valid) {
        ESP_LOGE(TAG, "Frame at index %d is invalid", index);
        return ESP_ERR_INVALID_STATE;
    }
    
    *frame = mgr->frames[index];
    return ESP_OK;
}

esp_err_t seq_mgr_set_default_sequence(sequence_manager_t* mgr) {
    if (!mgr || !mgr->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 清空现有数据
    seq_mgr_clear_all(mgr);
    
    // 设置默认开合动作：120° ↔ 150°，5秒间隔
    uint32_t base_time = get_current_timestamp();
    
    // 步骤1: 收拢到120°
    if (mgr->frame_count < MAX_SEQUENCE_STEPS) {
        mgr->frames[mgr->frame_count].left_angle = 120;
        mgr->frames[mgr->frame_count].right_angle = 120;
        mgr->frames[mgr->frame_count].timestamp = base_time;
        mgr->frames[mgr->frame_count].valid = true;
        mgr->frame_count++;
    }
    
    // 步骤2: 展开到150° (5秒后)
    if (mgr->frame_count < MAX_SEQUENCE_STEPS) {
        mgr->frames[mgr->frame_count].left_angle = 150;
        mgr->frames[mgr->frame_count].right_angle = 150;
        mgr->frames[mgr->frame_count].timestamp = base_time + 5000;
        mgr->frames[mgr->frame_count].valid = true;
        mgr->frame_count++;
    }
    
    // 步骤3: 回到收拢120° (再5秒后)
    if (mgr->frame_count < MAX_SEQUENCE_STEPS) {
        mgr->frames[mgr->frame_count].left_angle = 120;
        mgr->frames[mgr->frame_count].right_angle = 120;
        mgr->frames[mgr->frame_count].timestamp = base_time + 10000;
        mgr->frames[mgr->frame_count].valid = true;
        mgr->frame_count++;
    }
    
    mgr->state = SEQ_STATE_IDLE;
    ESP_LOGI(TAG, "Default sequence set with %d frames", mgr->frame_count);
    return ESP_OK;
}

// 播放任务函数
static void play_sequence_task(void* pvParameters) {
    sequence_manager_t* mgr = (sequence_manager_t*)pvParameters;
    
    if (!mgr || !mgr->initialized || !mgr->servo_set_callback) {
        ESP_LOGE(TAG, "Invalid parameters for play task");
        vTaskDelete(NULL);
        return;
    }
    
    if (mgr->frame_count <= 0) {
        ESP_LOGE(TAG, "No frames to play");
        vTaskDelete(NULL);
        return;
    }
    
    uint32_t last_timestamp = mgr->frames[0].timestamp;
    
    for (int i = 0; i < mgr->frame_count; i++) {
        // 检查是否请求停止播放
        if (mgr->stop_play_requested) {
            mgr->stop_play_requested = false; // 重置标志
            ESP_LOGI(TAG, "Playback stopped by request at frame %d", i);
            break;
        }
        
        if (!mgr->frames[i].valid) {
            continue;
        }
        
        // 计算延时
        uint32_t delay_ms = mgr->frames[i].timestamp - last_timestamp;
        if (delay_ms > 0 && i > 0) {
            // 实际延时，同时检查停止请求
            uint32_t elapsed = 0;
            while (elapsed < delay_ms) {
                if (mgr->stop_play_requested) {
                    mgr->stop_play_requested = false;
                    ESP_LOGI(TAG, "Playback stopped during delay at frame %d", i);
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(10)); // 每10ms检查一次
                elapsed += 10;
            }
            if (mgr->stop_play_requested) break;
        }
        
        // 执行舵机动作（使用默认速度1000ms）
        mgr->servo_set_callback(0, mgr->frames[i].left_angle, 1000); // 左舵机
        mgr->servo_set_callback(1, mgr->frames[i].right_angle, 1000); // 右舵机
        
        last_timestamp = mgr->frames[i].timestamp;
        ESP_LOGI(TAG, "Playing frame %d: L=%d, R=%d", i, mgr->frames[i].left_angle, mgr->frames[i].right_angle);
    }
    
    // 清理任务句柄
    mgr->play_task_handle = NULL;
    mgr->servo_set_callback = NULL;
    ESP_LOGI(TAG, "Playback task completed");
    vTaskDelete(NULL);
}

// 异步播放序列
esp_err_t seq_mgr_play_sequence(sequence_manager_t* mgr, esp_err_t (*servo_set_callback)(int servo_id, int angle, int speed)) {
    if (!mgr || !mgr->initialized || !servo_set_callback) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (mgr->frame_count <= 0) {
        ESP_LOGE(TAG, "No frames to play");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 如果已有播放任务在运行，先停止它
    if (mgr->play_task_handle != NULL) {
        mgr->stop_play_requested = true;
        // 等待任务结束（最多1秒）
        uint32_t wait_time = 0;
        while (mgr->play_task_handle != NULL && wait_time < 1000) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_time += 10;
        }
    }
    
    // 保存回调函数
    mgr->servo_set_callback = servo_set_callback;
    mgr->stop_play_requested = false;
    
    // 创建播放任务
    BaseType_t result = xTaskCreate(play_sequence_task, "play_seq", 4096, mgr, 5, &mgr->play_task_handle);
    if (result != pdPASS) {
        mgr->play_task_handle = NULL;
        mgr->servo_set_callback = NULL;
        ESP_LOGE(TAG, "Failed to create playback task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// 内部辅助函数实现
static uint32_t get_current_timestamp(void) {
    return (uint32_t)(esp_timer_get_time() / 1000); // 转换为毫秒
}