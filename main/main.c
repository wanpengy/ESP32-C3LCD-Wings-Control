#include "wings_control_ux_enhanced.h"
#include "http_server_ux_enhanced.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "io_extension.h"
#include "ST7789.h"
#include "Wireless.h"
#include "sequence_manager.h"  // 添加序列管理器头文件

// LVGL相关头文件
#include "lvgl.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "main";

#define SERVER_PORT 80

// LVGL相关全局变量
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf1[EXAMPLE_LCD_H_RES * 50];
static lv_color_t buf2[EXAMPLE_LCD_H_RES * 50];

// LCD显示对象
static lv_obj_t *status_label = NULL;
static lv_obj_t *angle_label = NULL;
static lv_obj_t *progress_label = NULL;

// 声明面板句柄（从ST7789.c中获取）
extern esp_lcd_panel_handle_t panel_handle;

static httpd_handle_t server = NULL;

// 序列管理器全局变量
sequence_manager_t g_sequence_mgr;

// LVGL刷新回调
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

// LVGL定时器回调
static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(2);
}

// LVGL显示初始化
static void lvgl_display_init(void)
{
    lv_init();

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 50);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    static esp_timer_handle_t lvgl_tick_timer = NULL;
    if (lvgl_tick_timer == NULL) {
        const esp_timer_create_args_t lvgl_tick_args = {
            .callback = &lvgl_tick_cb,
            .name = "lvgl_tick",
        };
        ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_args, &lvgl_tick_timer));
    }
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2000));
}

// 更新LCD状态显示
void update_lcd_status(const char* status)
{
    if (status_label == NULL) {
        status_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
        lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 10);
    }
    lv_label_set_text(status_label, status);
}

// 更新LCD角度显示
void update_lcd_angles(int left_angle, int right_angle, int min_angle, int max_angle)
{
    char angle_text[64];
    snprintf(angle_text, sizeof(angle_text), "L:%d° R:%d° [%d-%d]", 
             left_angle, right_angle, min_angle, max_angle);
    
    if (angle_label == NULL) {
        angle_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(angle_label, &lv_font_montserrat_12, 0);
        lv_obj_align(angle_label, LV_ALIGN_TOP_MID, 0, 40);
    }
    lv_label_set_text(angle_label, angle_text);
}

// 更新LCD进度显示
void update_lcd_progress(const char* progress)
{
    if (progress_label == NULL) {
        progress_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_12, 0);
        lv_obj_align(progress_label, LV_ALIGN_TOP_MID, 0, 70);
    }
    lv_label_set_text(progress_label, progress);
}

// 强制刷新LCD
void refresh_lcd_display(void)
{
    lv_refr_now(NULL);
}

static httpd_handle_t start_webserver(void) {
    server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = SERVER_PORT;
    // 增加URI处理器数量（默认可能是8个，我们需要至少12个）
    config.max_uri_handlers = 25;  // 添加这行
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Web server started on port %d", SERVER_PORT);
        // 使用统一的注册函数
        extern void start_http_server(httpd_handle_t server);
        start_http_server(server);
        return server;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return NULL;
}

void app_main(void) {
    ESP_LOGI(TAG, "Wings Control System (Final Embedded HTML Version) starting...");
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化硬件组件
    ESP_LOGI(TAG, "Initializing hardware components...");
    ESP_ERROR_CHECK(IO_EXTENSION_Init());
    ESP_ERROR_CHECK(LCD_Init());
    Wireless_Init();
    
    // 初始化序列管理器
    ESP_LOGI(TAG, "Initializing sequence manager...");
    esp_err_t seq_ret = seq_mgr_init(&g_sequence_mgr);
    if (seq_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sequence manager");
    }
    
    // 初始化LVGL显示系统
    ESP_LOGI(TAG, "Initializing LVGL display...");
    lvgl_display_init();
    
    // 创建默认屏幕并初始化显示对象
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr); // 清空屏幕
    
    // 初始化LCD显示
    update_lcd_status("系统启动中...");
    update_lcd_angles(135, 135, 120, 150);
    update_lcd_progress("准备就绪");
    refresh_lcd_display();
    
    // 启动HTTP服务器
    httpd_handle_t server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        update_lcd_status("HTTP服务器启动失败");
        refresh_lcd_display();
        return;
    }
    
    update_lcd_status("WiFi已连接");
    refresh_lcd_display();
    
    // 创建翅膀控制任务
    xTaskCreate(wings_control_task, "wings_control", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Wings Control System ready!");
    ESP_LOGI(TAG, "Connect to WiFi and visit http://<ESP32_IP>");
    
    update_lcd_status("系统就绪 - 访问Web界面");
    refresh_lcd_display();
    
    // LVGL主循环
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}