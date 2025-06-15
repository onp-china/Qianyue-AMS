# Qianyue-AMS 自动换料系统

## 项目简介
这是一个基于ESP32的自动换料系统，用于3D打印机的自动换料控制。

## 系统功能
- 自动换料控制
- 龙门架位置管理
- 多料盘支持
- 远程监控和控制

## 硬件要求
- ESP32开发板
- 舵机
- 步进电机
- 限位开关
- LED指示灯

## 软件依赖
- Arduino IDE
- ESP32开发板支持包
- 相关库文件：
  - ArduinoJson
  - PubSubClient
  - ESP32Servo
  - Adafruit_NeoPixel

## 目录结构
```
.
├── Arduino/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── GantryControll.h
│   │   ├── XYcontrol.h
│   │   └── XYcontrol.cpp
│   └── platformio.ini
├── docs/
└── README.md
```

## 安装说明
1. 克隆仓库
```bash
git clone https://github.com/onp-china/Qianyue-AMS.git
```

2. 使用Arduino IDE打开项目
3. 安装必要的库文件
4. 配置platformio.ini
5. 编译并上传代码

## 配置说明
1. 首次运行需要配置：
   - WiFi信息
   - MQTT服务器信息
   - 设备序列号
   - 通道ID
   - 其他参数

2. 配置文件存储在SPIFFS中

## 使用说明
1. 上电后等待系统初始化
2. 通过串口监视器查看状态
3. 使用MQTT客户端监控系统状态
4. 通过Web界面进行远程控制

## 开发说明
- 主要开发在Arduino/src目录下
- 使用platformio进行项目管理
- 遵循C++编码规范

## 贡献指南
1. Fork本仓库
2. 创建特性分支
3. 提交更改
4. 推送到分支
5. 创建Pull Request

## 许可证
MIT License

## 联系方式
[添加您的联系方式]
