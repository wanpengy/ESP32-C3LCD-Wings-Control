# ESP32-C3 翅膀控制系统

![ESP32-C3-LCD-1.47](https://docs.waveshare.net/images/thumb/9/9c/ESP32-C3-LCD-1.47.jpg/400px-ESP32-C3-LCD-1.47.jpg)

基于 ESP32-C3-LCD-1.47 开发板的翅膀控制系统，支持舵机控制、电机控制和机器人编程功能。通过手机Web界面实现直观的操作体验，内置LCD显示屏提供实时状态反馈。
A web based ESP32C3 control system for Wing motor(L298N) control and sevro motor flap control with robotic programming func.
## 功能特性

### 🦾 舵机控制
- **双舵机独立/对称控制**：支持左右舵机单独调节或对称联动
- **角度范围限制**：可自定义最小/最大角度限制（默认120°-150°）
- **速度调节**：每个动作可设置执行速度（默认1000ms）
- **众灵总线舵机协议**：兼容 `#000P1500T1000!` 格式

### ⚡ 电机控制
- **L298N电机驱动**：通过EXIO4/EXIO5(PD4/PD5)控制展开/收拢
- **一键操作**：展开翅膀、收拢翅膀、重置位置、停止电机

### 🤖 机器人编程
- **序列录制**：开始录制 → 记录步骤 → 停止录制
- **默认序列**：一键设置120°↔150°开合动作（5秒间隔）
- **播放控制**：播放序列 + 实时停止功能
- **步骤管理**：撤回上一步、清空序列、查看步骤详情
- **时间间隔显示**：自动计算并显示步骤间的时间间隔

### 📱 Web界面
- **手机端优化**：竖屏1080×1920分辨率适配
- **黄色主题**：圆角按钮，友好UI设计
- **实时同步**：滑条操作即时反馈到舵机
- **中文界面**：完整中文字符支持

### 🖥️ LCD显示
- **LVGL图形库**：三行信息显示（状态+角度范围+进度）
- **实时更新**：操作状态、角度信息、执行进度

## 硬件连接

| 功能 | 连接方式 |
|------|----------|
| 舵机控制 | GPIO21 (UART0_TX) |
| 电机控制 | EXIO4/EXIO5 (PD4/PD5 via PCA9555) |
| LCD显示 | 内置ST7789驱动 |

## 快速开始

### 1. 环境准备

本项目基于 **ESP-IDF v5.4.1** 开发，请参考以下教程安装开发环境：

🔗 **[ESP-IDF 安装与编译烧写教程](https://docs.waveshare.net/ESP32-ESP-IDF-Tutorials/ESP-IDF-Installation/)**

### 2. 编译项目

```bash
# 克隆项目（如果从GitHub获取）
git clone https://github.com/your-username/ESP32-C3-Wings-Control.git
cd ESP32-C3-Wings-Control

# 配置项目（可选，默认配置已优化）
idf.py menuconfig

# 编译项目
idf.py build
```

### 3. 烧写固件

```bash
# 烧写到ESP32-C3开发板（替换COM_PORT为实际串口号）
idf.py -p COM_PORT flash monitor
```

### 4. 使用Web界面

1. 连接WiFi（SSID: `ESP32_WINGS`，密码: `12345678`）
2. 访问 `http://<ESP32_IP>/` 或直接访问开发板IP地址
3. 开始使用翅膀控制和机器人编程功能

## Web界面操作指南

### 控制设置区域
- **角度限制**：设置舵机运动的最小/最大角度范围
- **对称控制**：勾选后左右舵机角度自动对称（右 = 270° - 左）

### 舵机控制区域
- **单滑条模式**（对称控制开启）：统一控制两侧舵机角度
- **双滑条模式**（对称控制关闭）：独立控制左右舵机角度
- **速度设置**：每个舵机可单独设置动作执行速度

### 电机控制区域
- **展开翅膀**：启动电机展开翅膀
- **收拢翅膀**：启动电机收拢翅膀  
- **重置位置**：将翅膀重置到初始位置
- **停止电机**：立即停止电机运行

### 机器人编程区域
- **开始录制**：进入录制模式
- **记录步骤**：保存当前舵机位置为序列步骤
- **停止录制**：结束录制模式
- **撤回**：删除最后一个记录的步骤
- **默认开合**：设置120°↔150°的标准开合序列
- **清空序列**：删除所有序列步骤
- **播放序列**：按时间顺序执行序列
- **停止播放**：中断正在执行的序列

## 项目结构

```
ESP32-C3-Wings-Control/
├── main/                   # 主应用程序源码
│   ├── main.c              # 主函数入口
│   ├── wings_control_ux_enhanced.c  # 舵机/电机控制逻辑
│   ├── http_server_core.c  # 核心HTTP API处理器
│   ├── http_server_sequence.c      # 序列管理API
│   ├── sequence_manager.c  # 序列管理器实现
│   └── http_server_files_robot_simple_fixed.c  # Web界面HTML
├── components/             # 自定义硬件组件
│   ├── LCD_Driver/         # LCD显示屏驱动
│   ├── io_extension/       # IO扩展组件
│   └── Wireless/           # WiFi网络组件
├── partitions.csv          # 分区表配置
└── sdkconfig               # ESP-IDF项目配置
```

## 技术规格

- **开发板**: ESP32-C3-LCD-1.47
- **ESP-IDF版本**: v5.4.4
- **编译工具链**: RISC-V GCC
- **Web服务器**: esp_http_server
- **图形库**: LVGL v8.x
- **协议支持**: 众灵总线舵机协议
- **内存占用**: ~150KB Flash, ~80KB RAM

## 故障排除

### 编译问题
- 确保使用 **ESP-IDF v5.4.4** 版本，6.02等以上版本会出现I2C_master.c兼容性问题
- 参考 [WaveShare官方教程](https://docs.waveshare.net/ESP32-ESP-IDF-Tutorials/ESP-IDF-Installation/) 进行环境配置

### 连接问题
- 检查硬件连接是否正确（舵机→GPIO21，电机→EXIO4/EXIO5）
- 确认WiFi连接成功，可通过串口监视器查看IP地址

### 功能问题
- LCD不显示：确保LVGL正确初始化
- 舵机无响应：检查角度范围是否在有效区间内
- Web界面404：确认固件烧写成功且WiFi连接正常

## 许可证

本项目仅供学习和研究使用。硬件基于 WaveShare ESP32-C3-LCD-1.47 开发板。

---

**注意**: 详细的ESP-IDF环境搭建、编译和烧写步骤请参考官方文档：  
🔗 **[https://docs.waveshare.net/ESP32-ESP-IDF-Tutorials/ESP-IDF-Installation/](https://docs.waveshare.net/ESP32-ESP-IDF-Tutorials/ESP-IDF-Installation/)**