# AGENTS.md

本文件是在 TwinPlant-Embedded 仓库里工作的规范，人和 AI 编码助手都要遵守。项目背景和架构见 [README.md](README.md)。

## 1. 基本原则

- 一期只做 README 中列出的范围。新增传感器、外设、第三方库或协议字段之前，先说明它服务于哪一项验收标准。
- 优先保证链路稳定、日志可追踪、故障可恢复，再考虑性能优化和新功能。
- 改动保持最小、聚焦。不顺手重构无关代码，不改与当前任务无关的文件。
- 每次改动都必须让三个工程能编译通过：只改了某一个工程，就至少编译那一个；改了 `shared/`，三个都要编译。

## 2. 仓库与目录

| 目录 | 内容 | 谁来修改 |
|---|---|---|
| `stm32/*/Core/`、`Drivers/`、`Middlewares/` | CubeMX 生成 | 只在 `USER CODE BEGIN/END` 之间手写代码 |
| `stm32/*/App/Inc`、`App/Src` | 业务代码 | 手写，CubeMX 不会修改 |
| `stm32/*/*.ioc` | 外设、引脚、时钟、FreeRTOS 配置 | 只用 CubeMX 修改 |
| `stm32/*/platformio.ini` | 编译配置 | 手写 |
| `esp32s3/` | Arduino 固件 | 手写 |
| `shared/` | 跨固件共用的协议定义（C 头文件） | 手写，改动影响三套固件 |
| `docs/` | 协议、接线、烧录、测试文档 | 手写 |

## 3. STM32CubeMX 规范

### 3.1 改配置只能通过 CubeMX
外设、引脚、时钟、中断优先级、FreeRTOS 参数都在 `.ioc` 里改，然后点 Generate Code。不要直接改生成的 `main.c`、`*_hal_conf.h`、`FreeRTOSConfig.h` 里的配置部分，下次重新生成会被覆盖。

### 3.2 手写代码的位置
- 在 CubeMX 生成的文件里，只能写在 `/* USER CODE BEGIN xxx */` 和 `/* USER CODE END xxx */` 之间。
- 业务逻辑放在 `App/` 里。生成文件中只保留调用入口，例如在 `main.c` 的 USER CODE 区调用 `app_init()`，在 `freertos.c` 的任务函数里调用 `App/` 中的实现。

### 3.3 Project Manager 设置
- Toolchain / IDE：Makefile
- Copy only the necessary library files
- Generate peripheral initialization as a pair of `.c/.h` files per peripheral：开
- Backup previously generated files：**关**。版本管理用 git，不要提交 `Backup/`
- Keep User Code when re-generating：**开**
- Delete previously generated files when not re-generated：开
- Enable Full Assert：开（开发阶段）
- Use latest available version：建议关闭，避免 CubeMX 自动升级固件包、悄悄替换 `Drivers/` 中的 HAL（目前两个 `.ioc` 里仍是开启状态，下次打开 CubeMX 时取消勾选）。升级固件包必须单独提交
- F407 Enable multi-threaded support：开，策略 Default；FreeRTOS 的 `USE_NEWLIB_REENTRANT`：开

### 3.4 重新生成之后
1. 用 `git diff` 检查 CubeMX 改了哪些文件，确认没有意外改动。
2. 如果新增了源文件目录，或新生成的文件不在 `Core/`、`Drivers/`、`Middlewares/`、工程根目录下，同步更新 `platformio.ini` 的 `build_src_filter` 和 `-I`。
3. 编译通过后，把 `.ioc` 和生成的代码放在同一个提交里。

## 4. PlatformIO 规范

- STM32 工程不设置 `framework`，不引入 PlatformIO 自带的 HAL/CMSIS。
- 工具链固定为 `toolchain-gccarmnoneeabi@~1.120301.0`（GCC 12.3.1），不要删掉这一行。
- F407 的浮点参数（`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`）由 `fpu_flags.py` 同时加到编译、汇编、链接三个阶段，不要改成只写在 `build_flags` 里。
- 仓库路径里有空格，`build_flags` 里不要使用 `${BUILD_DIR}` 这类会展开成绝对路径的变量。
- 编译命令在各工程目录下执行：`pio run`；烧录：`pio run -t upload`。

## 5. 嵌入式编码规范

### 5.1 通用（C，适用于两块 STM32 和 `shared/`）
- C11，缩进 4 个空格，不用 Tab。
- 文件名、函数名、变量名用 `snake_case`；宏和枚举值用 `UPPER_SNAKE_CASE`；类型名以 `_t` 结尾。
- 对外函数加模块前缀，例如 `sensor_read()`、`proto_encode()`；只在本文件使用的函数和变量加 `static`。
- 使用定宽整数（`uint8_t`、`int32_t`），不用 `int` 表示协议字段或寄存器值。
- 不在运行期使用 `malloc`/`free`。F407 的内存从 FreeRTOS 堆中分配，并且只在初始化阶段分配。
- HAL 调用的返回值必须检查，失败时返回统一错误码（见 5.4），不要静默忽略。
- 注释说明"为什么这样做"，不复述代码在做什么。

### 5.2 F103（裸机）
- 主循环不阻塞：用 `HAL_GetTick()` 做非阻塞定时，不用长时间的 `HAL_Delay()`。
- UART 收发用 DMA 加空闲中断。中断里只搬运数据、设置标志，解析放在主循环里做。

### 5.3 F407（FreeRTOS）
- 使用 FreeRTOS 原生 API 或 CMSIS-RTOS v2 API，同一个模块里只用其中一种。
- 一个任务只负责一件事。任务之间通过队列、事件组、任务通知传递数据，不共享全局变量；必须共享时用互斥锁保护。
- 中断里只能调用 `FromISR` 版本的 API，ISR 只做最少的工作，然后通知任务处理。
- 会调用 FreeRTOS API 的中断，优先级数值不得小于 `configMAX_SYSCALL_INTERRUPT_PRIORITY`（当前为 5）。
- 新建任务时写明栈大小的依据。联调阶段用 `uxTaskGetStackHighWaterMark()` 检查栈余量，余量不低于 25%。
- HAL 时基使用 TIM6，SysTick 留给 FreeRTOS，不要改回来。
- 安全要求：继电器在上电、复位、看门狗复位、故障状态下都默认**关闭**；看门狗只能由监控任务在确认各任务都正常运行后喂狗。

### 5.4 错误码与日志
- 错误码统一定义在 `shared/` 中，三套固件共用同一套数值。
- 日志分为 ERROR、WARN、INFO、DEBUG 四级，每条日志都带上 `deviceId` 和 `sequence`；与某次控制相关的日志还要带上 `requestId`。
- 中断里不要打印日志。

### 5.5 ESP32-S3（Arduino）
- C++17，命名规则和 C 代码一致。
- `loop()` 中不要阻塞。网络重连用状态机实现，不要写在 `while` 循环里等待。
- Wi-Fi 密码、MQTT 账号密码等凭证不能写进源码，也不能提交到 git，放在 NVS 中或 git 忽略的本地配置文件里。

## 6. 协议规范

- 协议有明确的版本号。改动字段或帧格式时，先更新 `docs/` 中的协议文档，再改代码，并同步修改 `shared/` 中的定义。
- 每条消息都包含：`deviceId`、`timestamp`、`type`、`value`、`quality`、`sequence`、协议版本。
- 消息类型：心跳、数据上报、控制请求、控制应答、错误。控制应答必须带回请求中的 `requestId`。
- UART 帧必须有帧头、长度和 CRC，接收端校验失败直接丢弃并计数，不能执行。
- 控制指令在 F407 上执行之前，要检查数值范围和设备状态；不合法的请求返回错误码，不执行。
- 新增 MQTT topic 或修改 payload 时，同时检查前端仓库是否需要配合修改。

## 7. Git 规范

- 在功能分支上开发，不直接提交到 `main`。
- 提交信息格式为 `type: 简短描述`，`type` 取 `feat`、`fix`、`build`、`docs`、`refactor`、`test`、`chore`。需要时在正文里说明原因。
- 一个提交只做一件事。CubeMX 重新生成的代码和对应的 `.ioc` 放在同一个提交里。
- 不要提交：`.pio/`、`Backup/`、`EWARM/`、`compile_commands.json`、IDE 的本地缓存、任何凭证。

## 8. 验证

- 每次改动后至少保证编译通过。
- 涉及硬件行为的改动，要说明是否已经在板子上验证过；没有验证的要明确写出来，不能当作已完成。
- 涉及验收标准的功能，验证记录写到 `docs/` 的测试报告中。
