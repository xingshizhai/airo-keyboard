# Airo-Keyboard

> Type Smarter, Not Harder.

🚀 The AI-Powered Smart Keyboard

基于 ESP32-S3-USB-OTG 开发板的智能 USB HID 键盘，配备 240x240 LCD 虚拟键盘界面，支持 WiFi 联网和 OpenClaw AI 集成。

基于ESP32-S3-USB-OTG开发板的USB HID键盘，配备240x240 LCD虚拟键盘界面，支持ASCII字符输入和Shift修饰键。

> ⚠️ 当前版本仅支持 **ESP32-S3-USB-OTG** 开发板，且仅支持 **ESP32-S3 (`esp32s3`)** 目标芯片。

![项目状态](https://img.shields.io/badge/状态-已完成-brightgreen) ![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.0-blue) ![许可证](https://img.shields.io/badge/许可证-MIT-green)

## 📋 目录

- [功能特性](#功能特性)
- [硬件要求](#硬件要求)
- [软件要求](#软件要求)
- [快速开始](#快速开始)
- [使用说明](#使用说明)
- [项目结构](#项目结构)
- [技术细节](#技术细节)
- [已知问题](#已知问题)

## ✨ 功能特性

- ✅ **USB HID键盘设备** - 完整的USB HID键盘实现，自定义设备名称
- ✅ **虚拟键盘UI** - 240x240 ST7789 LCD显示，5×4网格布局，共4页80个按键
- ✅ **8×8位图字体** - 自定义字体库，覆盖ASCII 32-126所有可打印字符
- ✅ **按键导航** - UP/DOWN按键选择字符，增量刷新优化（快速响应）
- ✅ **页面切换** - MENU按键单击/双击切换4个页面
- ✅ **完整字符支持** - 数字、大小写字母、常用符号、Enter键
- ✅ **Shift修饰键** - 自动添加Shift修饰符支持大写字母和符号
- ✅ **设备识别** - 自定义USB设备名称"ESP32 K54 Macro Keyboard"，PID 0x4B54

## 🔧 硬件要求

### 兼容性限制（重要）

- ✅ **仅支持开发板**: ESP32-S3-USB-OTG
- ✅ **仅支持芯片目标**: ESP32-S3 (`esp32s3`)
- ❌ **不支持**: 其他ESP32系列开发板（如ESP32/ESP32-C3/ESP32-C6等）
- 📚 **官方硬件资料**: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-usb-otg/index.html

- **开发板**: ESP32-S3-USB-OTG开发板
  - ESP32-S3芯片（双核，240MHz）
  - 240×240 ST7789 LCD屏幕
  - 4个物理按键（OK、UP、DOWN、MENU）
  - USB OTG接口
- **连接线**: USB Type-C数据线
- **主机**: Windows/Linux/macOS电脑（USB HID标准兼容）

## 💻 软件要求

- **ESP-IDF**: v6.0 或更高版本
- **编译目标**: `esp32s3`（必须）
- **Python**: 3.8+ 环境（ESP-IDF依赖）
- **编译工具**: CMake, Ninja（ESP-IDF自带）

## 🚀 快速开始

### 1. 克隆项目

```bash
cd D:\Work\esp32\projects
# 项目已在本地：esp32-ai-keyboard
```

### 2. 配置ESP-IDF环境

**Windows PowerShell:**
```powershell
# 加载ESP-IDF v6.0环境
$env:IDF_PATH = 'C:\esp\release-v6.0\esp-idf'
. "C:\Espressif\tools\Microsoft.release-v6.0.PowerShell_profile.ps1"
```

### 3. 编译项目

```bash
cd D:\Work\esp32\projects\esp32-ai-keyboard
# 必须使用 esp32s3 目标
idf.py set-target esp32s3
idf.py build
```

### 4. 烧录固件

```bash
# 替换 COM16 为你的串口号
idf.py -p COM16 flash
```

### 5. 监控日志（可选）

```bash
idf.py -p COM16 monitor
# 退出监控: Ctrl + ]
```

## 📖 使用说明

### 按键功能

| 按键 | 功能 | 说明 |
|------|------|------|
| **UP** | 向前选择 | 移动到上一个可用按键，增量刷新（快速） |
| **DOWN** | 向后选择 | 移动到下一个可用按键，增量刷新（快速） |
| **MENU 单击** | 下一页 | 切换到下一页（共4页循环），全屏刷新 |
| **MENU 双击** | 上一页 | 切换到上一页（共4页循环），全屏刷新 |
| **OK** | 发送字符 | 发送当前选中的字符到USB主机 |

### 键盘布局

**页面1** - 数字和小写字母 a-j:
```
0  1  2  3  4
5  6  7  8  9
a  b  c  d  e
f  g  h  i  j
```

**页面2** - 小写字母 k-z 和大写 A-D:
```
k  l  m  n  o
p  q  r  s  t
u  v  w  x  y
z  A  B  C  D
```

**页面3** - 大写字母 E-X:
```
E  F  G  H  I
J  K  L  M  N
O  P  Q  R  S
T  U  V  W  X
```

**页面4** - 大写 Y-Z、符号和Enter:
```
Y  Z  !  @  #
$  %  ^  &  *
(  )  _  +  [
]  {  }  -  =
\  |  ;  :  '
"  <  >  ,  .
/  ?  ~  `  E  (E=Enter回车键)
```

### USB设备信息

- **厂商ID (VID)**: 0x303A (Espressif)
- **产品ID (PID)**: 0x4B54 (自定义，K54)
- **设备名称**: "ESP32 K54 Macro Keyboard"
- **序列号**: "K54-20260307"

## 📁 项目结构

```
esp32-ai-keyboard/
├── CMakeLists.txt                    # 顶层CMake配置
├── sdkconfig.defaults                # ESP-IDF预设配置
├── README.md                         # 本文档
├── main/
│   ├── app_main.c                    # 主应用入口和按键事件处理
│   ├── CMakeLists.txt
│   └── idf_component.yml             # 项目依赖声明
└── components/
    ├── bsp_esp32_s3_usb_otg_ev/      # 板级支持包（ESP-IDF 6.0兼容）
    │   ├── board_init.c              # 硬件初始化（LCD、按键）
    │   ├── bsp_esp32_s3_usb_otg_ev.h
    │   └── CMakeLists.txt
    ├── usb_hid/                       # USB HID键盘设备
    │   ├── usb_hid.c                 # TinyUSB配置、HID报告、ASCII映射
    │   ├── usb_hid.h                 # USB HID公共接口
    │   └── CMakeLists.txt
    └── ui/                            # 虚拟键盘UI
        ├── ui_keyboard.c             # 键盘布局、LCD绘制、事件处理
        ├── ui_keyboard.h             # UI键盘公共接口
        ├── font_8x8.h                # 8×8位图字体表（ASCII 32-126）
        └── CMakeLists.txt
```

## 🔬 技术细节

### USB HID实现

- **协议**: USB HID v1.11键盘设备类
- **端点**: INTERRUPT IN (键盘报告)
- **报告格式**: 8字节标准Boot Protocol键盘报告
  ```
  [Modifier] [Reserved] [Key1] [Key2] [Key3] [Key4] [Key5] [Key6]
  ```
- **Modifier支持**: Left Shift (0x02) 用于大写字母和符号
- **TinyUSB栈**: `espressif/esp_tinyusb` v1.4.5

### ASCII到HID映射

完整的ASCII字符映射逻辑：
- **数字** (0-9) → HID_KEY_0 - HID_KEY_9
- **小写字母** (a-z) → HID_KEY_A - HID_KEY_Z
- **大写字母** (A-Z) → HID_KEY_A - HID_KEY_Z + Shift修饰符
- **符号** (!@#$等) → 对应HID键码 + Shift修饰符（如需）
- **特殊键** (\n, \r, 0x1F) → HID_KEY_ENTER

### UI渲染优化

**增量刷新机制**（v1.1优化）:
- 导航按键（UP/DOWN）仅重绘2个单元格：
  - 取消选中的旧单元格（边框+字符）
  - 新选中的单元格（高亮边框+字符）
- 页面切换（MENU）进行全屏刷新（20个单元格+背景+指示器）
- **性能提升**: 导航响应速度提升约10倍

**绘制细节**:
- 8×8位图字体按单元格大小自动缩放
- 字符居中对齐
- 选中状态高亮边框（白色）和反色字符（黑色）
- 底部页面进度指示条

### 内存占用

- **Flash使用**: 278KB / 1024KB (27%)
- **RAM使用**: ~40KB（估算，包括FreeRTOS内核）
- **LCD缓冲**: 480字节行缓冲（240像素 × 2字节RGB565）

## ⚠️ 已知问题

1. **Windows USB描述符缓存**
   - **现象**: 修改USB设备名称后，Windows仍显示旧名称
   - **解决**: 已使用自定义PID (0x4B54)，拔插USB重新枚举即可
   
2. **Enter键显示**
   - **现象**: Enter键在屏幕上显示为字母'E'（防止不可见字符0x1F显示空白）
   - **实际功能**: 按OK键发送Enter/回车，功能正常

## 🤝 贡献

欢迎提交Issue和Pull Request！

## 📄 许可证

MIT License

## 🔗 参考资源

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/index.html)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [USB HID Specification](https://www.usb.org/hid)
- [ESP32-S3-USB-OTG Board Reference](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-usb-otg/index.html)

---

**项目创建日期**: 2026年3月7日  
**最后更新**: 2026年3月8日
