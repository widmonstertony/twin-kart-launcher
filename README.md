# Twin Kart Launcher

简体中文 | [English](README.en.md)

当前版本：**v0.1.0-alpha.1，开发预发布**。Release 提供原创启动器、开发工具及源码，不包含原厂客户端、游戏资源、私有存档、协议密钥或 NVIDIA 运行库。当前版本还不能作为完整离线比赛发行版使用。

为两款原厂游戏独立编写的本地启动器项目：2004 年起的《跑跑卡丁车》经典版，以及《跑跑卡丁车：漂移》。本项目不重新制作赛车游戏，不复制第三方启动器实现，不包含厂商客户端或美术资源。

**原创启动器已直接启动原创后端和真实 Drift 客户端。客户端已接受新后端的登录、背包、任务、登录完成、换车和持续保活响应。启动器已接入解锁、改装方案、相机模块加载、图形探针及配置恢复；这不表示 AI、完整比赛或 DLSS 画面已完成。**

原厂程序、SDK、美术及资源定义由用户外部提供。新 `PrepareDriftClient.ps1` 和 `PrepareSdkCopy.cs` 从本机原厂安装生成隔离运行目录与本地身份 SDK 副本，不复制第三方启动器实现。当前私有数据集仍包含参考项目提供的资源定义及部分只读响应数据，均不进入公开源码包。

当前开发顺序：先完成原厂离线玩法与 DLSS，再做 Fanatec。已有输入模块暂存，硬件验收延后。Android Drift 的本机样本已用于私有静态协议研究；移动端资源不会直接冒充最新 PC 资源。

## 已实现并验证

- 双版本独立配置、客户端路径和 SHA-256 核查；自动读取 Steam 库定位 Drift。
- DirectInput 8 设备与轴枚举，分别选择方向盘、油门、刹车，支持踏板接底座或独立 USB。
- 左／中／右转向校准、松开／踩到底踏板校准、反向轴、死区、按钮绑定、实时输入预览。
- 原创会话共享内存输入桥，以及 x86 / x64 XInput 适配 DLL。它只为指定会话提供输入，不安装虚拟驱动，不发送物理力反馈。
- 心跳超过 250 ms、配置未完成或设备断开时，停止提供虚拟控制器输入。
- C# 5 / .NET Framework 编译；两个架构的实际 XInput DLL 软件测试通过。
- 原创离线房间状态核心：将已核对的创建、加入、添加 AI 和发车请求解析为强类型对象，校验竞速／道具模式、赛道 ID 与已知字段类型，再记录本地状态。`PcCustomGame` 的已知房间结构也可只读校验；尚未接上原厂客户端网络协议。
- 原创字节封装编解码：保留原始 JSON 字节拼写，支持可选的 SHA-256 八位后缀与 AES-256-CBC/PKCS#7 信封；默认用安全随机数生成 IV。标准向量、畸形输入及真实 PC 登录握手已验证。
- 原创回环 WebSocket 网关和四步本地账号流程：预认证、离线身份、关联检查、账号完成。只模拟本地身份，不连接或验证在线账号。
- 独立动态数据目录：从外部 UTF-8 文档生成类型明确的配置响应和 SHA-256；源码不附带厂商配置或第三方响应模板。
- 原创持久化后端：JSON 存档原子提交与失败回滚，背包迁移、装备选择、改装、升级、赛道列表、计时赛状态及本地成绩写入。核心已通过软件测试；其中登录、背包、换车及保活已有实际客户端请求证据，比赛与成绩处理尚待新端实机验收。
- 原创改装方案窗口：单车自定义或全部已有改装记录的车辆最高档方案。解除点数预算、车型档位及固定属性约束，但仅编码外部表明确支持的数值。缺少性能捕获的车型和旧客户端不存在的功能不会被伪装为可用。
- 原创会话启动与模块加载：只管理本轮启动的进程、校验文件指纹、加载本项目原生模块，退出后恢复全部 INI。窗口入口和终端诊断入口共享同一会话代码；当前窗口新增入口仅完成编译验证。
- 原创构建限定相机模块 `native/DriftCamera.cpp`：已在 Drift 海盗计时赛中看到原厂双手、方向盘及行驶中的转向姿态变化。使用原厂骨骼可见性接口隐藏遮挡视线的头部分支，保留手臂；不包含角色或车辆资产。视角切换恢复、换车、重生和换图仍待验收，目前通过私有实验加载器使用。
- 原创赛车性能字段布局转换：按字段名称及宽度将参考版本的 66 字段映射到当前 Steam 客户端的 55 字段；已实测进入赛道，用户确认操控。转换模块原创，私有实验服务端整体仍是参考派生代码。

Fanatec ClubSport DD+ 15 Nm（PlayStation）与 ClubSport V3 Inverted 是目标硬件。用户目前没有连接实体设备，实机输入、游戏内操控、按钮含义与力反馈尚未验收。

## 尚未完成

| 项目 | 当前证据 / 缺口 |
|---|---|
| 经典版客户端 | 本机未找到；官方 TC GAME 安装器下载目前返回 403，完整客户端未取得 |
| Drift 客户端 | 已核对 Steam build 15490952，28 个 Pak，共 27,934,556,284 字节；不代表全球最终版本或每条赛道可玩 |
| 原创离线登录、比赛 | 新启动器已对接新后端，真实客户端接受登录、背包、登录完成及换车；计时赛/成绩逻辑软件测试通过，新端赛道实测及完整结算仍待完成。外部客户端数据不属于原创源码 |
| 竞速 / 道具 / AI | 实机自定义房间已显示 AI1；同场起跑、AI 行驶、冲线和结算仍未接通 |
| 第一人称 | 之前的海盗赛道实测原厂双手、方向盘及 F6 切换恢复通过；原创启动器已加载同一模块。新端赛道及换车／重生／换图生命周期待验收；经典版缺客户端 |
| DLSS 超分、帧生成、DLSS 5 | 分别研究实际渲染输入和呈现链；尚未通过本项目游戏内验收 |
| 全解锁、全部赛道 | 需要与实际客户端资源逐项核对并试跑；ID 列表不是完成证明 |

## 构建与运行

需要 Windows x64、.NET Framework 4.8、MSVC C++ Build Tools（x64/x86）、Windows 10/11 SDK。构建脚本通过 `vswhere` 发现工具；本地 DLL 静态链接 C++ 运行库。

```powershell
.\build.ps1
.\test-input.ps1 -SkipNative
.\test-input.ps1 -SkipNative -Architecture x86
.\gateway\test-room.ps1
.\gateway\test-wire.ps1
.\gateway\build-gateway.ps1
.\gateway\test-gateway.ps1
.\gateway\test-body-layout.ps1
.\gateway\test-unlocks.ps1
.\gateway\test-offline.ps1
.\native\build-camera.ps1
.\build\TwinKartLauncher.exe
```

在启动器中选择版本和客户端。“离线配置”选择本机的私有会话清单，“改装方案”保存下次启动要应用的方案，“启动原厂游戏”使用本项目后端。会话清单和外部数据不随源码分发。设备配置保留在独立区域，输入桥仍处于软件验证阶段。

准备隔离客户端（支持的 Steam 构建）：

```powershell
.\tools\PrepareDriftClient.ps1 -InstalledRoot 'D:\Games\KartDrift' -DestinationRoot 'D:\OfflineGames\TwinKartDrift'
```

此步骤只准备原厂运行文件和 SDK 兼容副本，不自动生成当前仍在研究的全部外部协议、物理和资源数据。不能把一份可编译的源码检查点当作完整离线发行版。

只读客户端核查：

```powershell
.\build\TwinKartAudit.exe
# 或指定客户端 EXE
.\build\TwinKartAudit.exe 'D:\Games\KartDrift\Binaries\Win64\KartDrift-Win64-Shipping.exe'
```

不要把开发用 `KartWheelXInput.dll` 直接覆盖到 Steam 安装。启动器的隔离会话已有配置恢复验证；方向盘输入通道还未通过原厂游戏实机验收。

完整开发网关入口：`build/TwinKartGateway.exe --offline <private.local.json>`；仅账号诊断使用 `--local-account`。配置提供 `TransportKey`、`HashSecret`、`PreAuthKey`、`ExpectedClientVersion`、`AccountPlatform`、`Port`、`RunSeconds`、`LocalAccountId`、`ProtocolHash`、`DynamicDataPath`、`ProfileDataDirectory` 和 `ProfilePath`。外部文档目录格式为 `{ "Name": "example.json", "Content": "{}" }` 的 JSON 数组。实际客户端数据和私有配置不包含在公开包中；缺少这些数据时，Release 可以打开启动器和核查客户端，但不能直接开始离线比赛。

## 发布与后续开发

这是独立仓库，不是第三方项目的 fork。研究参考项目的行为和协议，独立编写实现；源码及依赖边界见 [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md)。

当前发布记录见 [CHANGELOG.md](CHANGELOG.md)。后续按“原创后端基础 → AI 竞速/道具比赛 → 图形与座舱整合 → 全资源/全赛道验收 → Fanatec”的顺序推进。优先整合代码并做软件检查，整合后再交给用户进游戏验收。已有参考链路或单项实验结果不自动算作新客户端整体验收完成。

## 源码与证据

- `src/`：原创 WinForms 启动器、配置、客户端核查和输入通道。
- `native/`：原创 DirectInput 读取、XInput 会话适配、Drift 构建限定座舱相机与构建脚本。相机 DLL 只接受明确指定且 SHA-256 匹配的客户端；没有通用进程扫描或自动部署功能。
- `gateway/`：原创房间状态核心、字节封装、WebSocket、本地账号状态机与外部动态数据目录。握手成功不代表比赛成功。
- `tests/`：软件回归验证；不能代替硬件或比赛验收。
- `research/`：本机独立观察与未核实的推测；含机器专属证据，首版公开源码不收录。

首版公开源码的明确文件清单和外部依赖边界见 `SOURCE_PROVENANCE.md`。

源码发布时只包含本项目源文件，不包含游戏资产、厂商 DLL、第三方启动器、个人配置、原始抓包或用户日志。游戏名称和商标属于相应权利人。本项目没有官方关联。
