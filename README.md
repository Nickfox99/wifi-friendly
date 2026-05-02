# WiFiFriendly (WiFi 友好版)

一个基于 ESP32 的智能小车项目，支持时钟模式和小车模式双模式切换。

## 🌟 功能特性

### 双模式设计
- **时钟模式**（默认）：OLED 显示时间，NTP 自动对时，RoboEyes 大眼睛动画
- **小车模式**：电机控制、LED 灯效、语音控制

### 核心功能
- 🤖 **表情系统**：10 种情绪状态（默认、高兴、生气、疲惫、大笑、困惑、流汗、惊讶、困倦、无语）
- 💃 **动作模式**：跳舞、原地旋转、小动作随机、摇摆、警灯模式、呼吸氛围灯
- 📡 **无线控制**：WiFiManager 自动配网，Web 网页远程控制
- 🗣️ **语音控制**：ASR 串口语音指令识别
- ⏰ **时间同步**：NTP 网络时间协议自动校准
- 👀 **动态眼睛**：RoboEyes 库实现 OLED 表情动画

## 🛠️ 硬件需求

- ESP32 开发板
- OLED 显示屏 128×64 (SSD1306, I2C)
- 电机驱动模块
- LED 灯 (红色 + 蓝色)
- ASR 语音识别模块

## 📦 依赖库

```
FluxGarage_RoboEyes
WiFiManager
WiFi
Wire
Adafruit_GFX
Adafruit_SSD1306
NTPClient
WiFiUdp
WebServer
DNSServer
```

## 🔧 快速开始

1. 克隆仓库
2. 安装依赖库
3. 配置 WiFi 信息
4. 上传到 ESP32

## 📄 许可证

MIT License - 欢迎 fork 和修改！

## 🎬 来源

抖音【极客狐尼克】开源项目

---

Made with ❤️ by GitHub Open Source Community
