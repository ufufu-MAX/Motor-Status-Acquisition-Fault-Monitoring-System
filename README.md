# STM32F407 电机数据监测项目

基于 STM32F407ZGT6、STM32 HAL 和 FreeRTOS 的电机数据采集与监测工程。代码包含 ADC 采样与数据处理、故障判断、串口命令、SD 卡记录、W25Q64 存储以及 RS485 通信等功能。具体行为和参数以源码为准。

## 开发环境

- 芯片：STM32F407ZGT6（LQFP144，Cortex-M4F）
- 工程配置：STM32CubeMX，配置文件为 `F407.ioc`；工程记录的固件包版本为 STM32Cube FW_F4 V1.28.3
- 编译工具：CMake 3.22 或更高版本、Ninja、Arm GNU Toolchain（`arm-none-eabi-gcc` 等工具需在 PATH 中）
- 开发工具：VS Code 或其他支持 CMake 的编辑器
- 软件组件：STM32 HAL、FreeRTOS、FatFs
- 时钟配置：HSE 8 MHz、LSE 32.768 kHz，系统时钟 168 MHz；实际板卡需符合该配置

## 编译

在项目根目录执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

生成的 ELF 文件位于 `build/Debug/F407.elf`。Release 版本可将上述两处 `Debug` 改为 `Release`。使用支持 STM32F407 的下载器和调试软件烧录 ELF；编译成功不代表外设接线或运行功能已经通过验证。

## 硬件接线

下表根据 `F407.ioc` 和当前源码整理，具体开发板的板载连接、供电和电平请以板卡原理图及模块手册为准。外接模块应与开发板共地，连接前确认电源与信号电平兼容。

### 调试接口

| STM32 引脚 | 信号 | 连接 |
| --- | --- | --- |
| PA13 | SWDIO | 下载器 SWDIO |
| PA14 | SWCLK | 下载器 SWCLK |
| GND | 地 | 下载器 GND |

下载器的目标电压参考和供电方式按下载器及开发板说明连接。

### ADC 输入

| STM32 引脚 | 配置 |
| --- | --- |
| PA0 | ADC1_IN0 |
| PA1 | ADC1_IN1 |
| PA2 | ADC1_IN2 |
| PA3 | ADC1_IN3 |

四路信号的具体传感器、调理电路和量程需根据实际硬件确认；输入不得超出芯片允许的模拟输入范围。

### W25Q64 SPI Flash

| STM32 引脚 | W25Q64 信号 |
| --- | --- |
| PA4 | CS / NSS |
| PA5 | CLK / SCK |
| PA6 | DO / MISO |
| PA7 | DI / MOSI |

供电、WP 和 HOLD 引脚按所用芯片或模块手册连接。

### SD 卡（SDIO 4 位）

| STM32 引脚 | SD 卡信号 |
| --- | --- |
| PC8 | D0 |
| PC9 | D1 |
| PC10 | D2 |
| PC11 | D3 |
| PC12 | CLK |
| PD2 | CMD |

供电、上拉电阻及卡座连接应按实际硬件设计确认。

### 串口与 RS485

| 用途 | STM32 引脚 | 连接 |
| --- | --- | --- |
| USART2 TX | PD5 | USB 转串口模块 RX |
| USART2 RX | PD6 | USB 转串口模块 TX |
| USART1 TX | PA9 | RS485 收发器 DI |
| USART1 RX | PA10 | RS485 收发器 RO |
| RS485 方向控制 | PA8 | 根据收发器和方向控制电路连接 |

USART2 用于串口命令，参数为 **115200、8 位数据、无校验、1 位停止位（8N1）**；可通过串口发送 `CLI_CMD_HELP` 并附带回车或换行查看命令。USART1 用于 RS485，参数为 **9600、8N1**，代码中的从站地址为 `1`。

注意：当前 `RS485_SetTx()` 将 PA8 拉低，`RS485_SetReceive()` 将 PA8 拉高。若模块的 DE 和 /RE 直接并联且高电平发送、低电平接收，其方向与本程序相反；需要依据实际电路处理电平反相或调整程序。RS485 总线的 A/B、终端电阻和偏置按具体收发器及总线设计连接。

### 指示灯

程序使用 PC13 控制 LED；LED 是否板载及亮灭极性取决于开发板电路。

## 目录说明

- `Core/`：CubeMX 生成的主程序、外设初始化和中断代码
- `User/`：采集、串口、SD 卡、Flash、RS485 等应用代码
- `Drivers/`：HAL 和 CMSIS 驱动
- `Middlewares/`、`FATFS/`：FreeRTOS、FatFs 等中间件和配置
- `cmake/`：工具链和 CubeMX 的 CMake 配置
- `F407.ioc`：STM32CubeMX 工程配置

## 使用说明

实际运行前请确认开发板型号、晶振、各模块电源、引脚和方向控制电路。传感器类型与 ADC 通道的实际对应关系、量程、SD 卡模块及 RS485 硬件接法需要结合实物补充。外设功能应分别进行板级验证。

## 许可证

本仓库尚未指定项目整体许可证；仓库中第三方组件的许可条款以各组件附带的许可证文件为准。
