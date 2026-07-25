static esp_err_t sequence_stop_play_handler(httpd_req_t *req) {
    char response[200];
    
    esp_err_t err = seq_mgr_request_stop_play(&g_sequence_mgr);
    if (err == ESP_OK) {
        snprintf(response, sizeof(response), 
                 "{\"success\":true,\"message\":\"播放停止请求已发送\"}");
    } else {
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"message\":\"停止播放失败: %s\"}", 
                 esp_err_to_name(err));
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}