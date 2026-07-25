#include "sequence_manager.h"
#include "esp_log.h"

static const char *TAG = "sequence_manager";

esp_err_t seq_mgr_request_stop_play(sequence_manager_t* mgr) {
    if (!mgr || !mgr->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    mgr->stop_play_requested = true;
    ESP_LOGI(TAG, "Stop play requested");
    return ESP_OK;
}