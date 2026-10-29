# Changelog / 更新记录

## Unreleased / 后续开发

- Wire real native simulation/render/present markers, 64-bit frame correlation, simulation-thread Reflex pacing and provider tokens; defer packed-client slot installation until swapchain creation. / 接入真实原生模拟／渲染／呈现时序、64 位帧关联、模拟线程节奏及 NVIDIA token；原生接口在解包后创建显示链时安装。
- A focused synthetic GPU fixture rendered 120 frames and presented 233 (113 extra-frame reports). No game was started, and game FG/quality acceptance remains pending. / 聚焦合成 GPU 测试渲染 120 帧、显示 233 帧（113 次报告额外生成帧）；没有启动游戏，游戏内效果待验收。
- Scope local AI/result hooks to the original world's object identity; allow exact native variant definitions without suffix filtering; request lower native screen percentage in engine configuration as well as user settings. / AI／结算钩子限定原生世界身份；精确赛道定义不因后缀被过滤；引擎配置与用户设置同时请求低分辨率。

- Fix the native FG race-state gate: CurrentPhase is a UPhase containing transition rules, not a UTransition_BeginRace instance. Compare the independently verified native BeginRace/RetireCountdown FNames and retain pause/stale-state rejection. Source regression only; no game or generated-frame acceptance. / 修复将比赛阶段误当转换规则的判断错误；使用已核对的原生阶段名称，保留暂停与过期快照保护，未启动游戏验收。
- Add a hash-checked offscreen D3D11 SR probe. Two actual NVIDIA HDR evaluations and GPU readback pass at 640×360 → 1280×720, with 921,600 finite nonzero output pixels and preserved native pipeline state. Synthetic input only; no window/Present/game and no native temporal-source or quality acceptance. / 新增 NVIDIA D3D11 超分实际计算与离屏读回验证，合成输入检查通过，不冒充游戏验收。
- Add an independently authored pre-AA HDR temporal SR candidate with exact shader selection, reflected compute-group validation, padded input/output rectangles, native jitter/pre-exposure and original D3D11 NGX evaluation. Actual WARP boundary tests cover cropped conversion, exact output writes, state restoration and failure fallback with a synthetic consumer; actual in-game upscaling remains unverified. / 新增抗锯齿前 HDR 时域超分候选与 GPU 接口测试，未将合成测试当作游戏验收。
- Reset reconstruction history on cockpit controls/head-anchor changes, camera cuts, missed frames and respawns; correct FG jitter for resolved inputs. Defer resize reconfiguration to the render owner, check graphics files/hashes before config writes and gate all requested graphics startup on hook readiness. / 补齐视角与相机变化的时域重置，修正插帧抖动、分辨率切换和启动依赖检查。
- Check the requested native solo speed/item behavior even if an AI path already exists; reuse existing exact-class components and keep race completion unverified. / 即使 AI 已有路线组件，也核对所需竞速／道具行为组件，复用已有组件，完整比赛仍待完成。
- Wire an opt-in native SR → NR candidate, original D3D11 shadow/D3D12 presentation adapter, game-HWND Streamline tags, native camera decoding, conservative UI mask, completion fences and resize reset. Add a deterministic owned-process startup sequence that waits for graphics hooks before primary entry, plus original fixture tests. No new game acceptance. / 新增超分到 NR 与实际窗口呈现的调用候选、相机解码、UI 掩码及同步，启动前等待图形拦截就绪；原创测试程序通过，未启动游戏验收。
- Add private-manifest graphics requests and temporary lower internal resolution with existing INI restoration. Preserve unsupported-path fallback and separate runtime events from visual acceptance. / 增加清单统一请求图形功能及临时内部渲染比例，沿用配置恢复；不把运行事件当作画质验收。
- Complete AI component preparation after native client setup, select original speed/item AI components and bind FollowTG through the exact-build native movement setter. Software checks only. / 原生客户端准备完成后再补 AI 组件，选择竞速／道具实现并连接寻路和移动组件，尚无新比赛验收。
- Connect an opt-in exact-build native NR pass after tone mapping and before HUD rendering, with actual depth/velocity conversion, camera-layout checks, D3D11/D3D12 shared GPU fences, original target writeback, changed-pixel GPU readback and backbuffer/Present identity evidence. Earlier HDR placement failed visual checks and was replaced. NR quality, in-game SR and generated-frame presentation remain unverified. / 接通实验性原生 NR 画面链路，包含真实引导转换、相机布局校验、跨 API GPU 同步、原目标回写、像素读回和实际呈现身份检查；修正早期 HDR 接点颜色异常，画质、游戏超分与插帧仍未验收。
- Add bounded opt-in temporal resource/camera capture with exact shader selection, deferred arming and a strict byte/frame budget; actual WARP texture and constant-buffer readbacks preserve bindings and references. Captures remain private. / 新增受限的时域资源与相机采集，限定着色器、触发时机、字节和帧数预算，真实 WARP 读回检查通过，采集数据不公开。
- Replace time-attack preparation with the original local loading transition; a PC trial entered Village_I01 and spawned three real AI pawns. They remained stationary. / 改用原厂本地加载流程；PC 实机已进入 Village_I01 并生成三个 AI，但 AI 仍静止。
- Add exact-build AI component registration, FollowTG preservation, native solo speed/item lobby routing, solo result-styler fallback and readiness responses. Software checks pass; real-game acceptance is pending. Native solo buttons reuse the last launcher track/AI/lap choice. / 新增构建限定的原厂 AI 组件、寻路保留、大厅个人竞速／道具转接、结算样式兜底和准备状态响应；软件检查通过，实机未验收，原生按钮沿用启动器上次赛道／AI／圈数选择。
- Reuse existing exact-class AI components during the original registration routine; the old trial already contains movement components, so only missing components should be constructed. Real-game verification is pending. / 原厂注册流程复用 AI 已有同类型组件；旧试运行已有移动组件，仅补缺项，实机待验证。
- Add original D3D12 SR and hash-limited private-ABI NR consumers; standalone synthetic GPU evaluation/readback passes. / 新增原创 D3D12 超分及模型指纹限定的 NR 模块，独立合成资源 GPU 计算与读回通过，尚非游戏验收。
- Add original Streamline FG/Reflex/PCL integration, synchronized D3D11/D3D12 shared textures and shader resource/camera reflection. A hidden-window FG test is correctly not accepted as generated frames. / 新增原创帧生成接口、跨 API 共享纹理及着色器资源／相机布局读取；隐藏窗口未实际插帧，不伪报成功。

- Add original session-local race requests, an opt-in launcher window, speed/item mode, 1–7 AI opponents and laps; preserve original kart/driver preparation and invoke native level travel on the owned game thread.
- Add explicit game-thread routing without changing keyboard/mouse messages, bounded request syntax, fresh command identities and managed/native interoperability checks.
- Reconcile every catalog ID with actual native track definitions and retain unresolved mirror/reverse/base entries in the UI; validate client build, thread, counts and complete coverage before committing the report.
- Keep race driving, settlement, all-track acceptance and DLSS status explicitly unverified. The alpha Release is unchanged.

- Add cockpit settings with persistent/live seat, head and FOV controls; identical reapplication after F6 now works. / 新增第一人称设置窗口和持久化、实时座位／头部／视野控制，修复 F6 后重发相同设置不生效的问题。
- Add optional head-bone anchoring and native weak-reference serial acquisition for new drivers. Control-file checks pass; updated game lifecycle acceptance is pending. / 新增可选头部骨骼定位和原厂弱引用序号获取，控制文件检查通过，新版实机生命周期仍待验收。
- Add an optional native asset-registry inventory module and a strict track-evidence reconciler. One real isolated capture found 1,585 world/sublevel records and 101 AI-path assets; 19 catalog base aliases remain unmatched after separating two reverse candidates. Configuration restoration passed. / 新增原生资源目录模块和严格赛道证据核对工具；实机隔离采集得到 1,585 个地图／子关卡和 101 份 AI 路线，区分两个反向候选后仍有 19 个基础别名未匹配，配置恢复通过。
- Prioritize cockpit, complete AI races and all tracks; defer further DLSS/Fanatec work. / 优先完成第一人称、完整 AI 比赛和全赛道，DLSS 与 Fanatec 后排。
- Add native room readiness notifications and persist a bounded, idempotent result acknowledgement journal. Retries survive restart without consuming another active race or changing rankings twice. / 补齐房间准备状态通知和有限长度的幂等结算回执；重试可跨重启恢复，不会误结束下一场比赛或重复刷新排行榜。
- Restore a previous cockpit mesh before replacing its cache; validate object slots, destruction flags and allocation serials before retained-object writes. Synthetic lifetime checks pass; new in-game lifecycle acceptance is pending. / 切换座舱缓存前恢复旧模型；写回前核对对象槽、销毁状态和分配序号。生命周期软件检查通过，新版实机生命周期仍待验收。
- Add sampled D3D11 draw/dispatch input-output traces and SHA-256 shader identifiers. WARP checks cover resource references, pipeline preservation and pixel/compute binding correlation. This is temporal-pass discovery, not working DLSS output. / 新增 D3D11 绘制／计算输入输出采样与着色器 SHA-256；WARP 检查覆盖引用、渲染状态保持和输入输出关联。用于定位时域渲染接点，尚未输出 DLSS 画面。
- Preserve observer hooks when the D3D11 runtime refreshes its shared dispatch table by cloning the supported context interface per instance. All eight draw/dispatch wrappers produce samples and preserve rendered pixels in WARP tests. / 用每个上下文独立的接口表保留图形 hook，修复 D3D11 运行时刷新共享表后采样丢失的问题；WARP 实测八种绘制／计算入口均持续记录且保持输出像素。
- Connect the original native room adapter to the profile backend: create/join, seven AI slots, kick, setting changes and exit. Same-track race hosting is still pending. / 原创房间通知已接入后端，覆盖创建/加入、7 个 AI 槽位、移除、设置变化和退出；同场比赛主持协议仍待完成。
- Complete native time-attack result fields, preserve the starting vehicle/tuning snapshot, refresh local ranking versions and exclude abandoned runs from personal bests. / 补齐计时赛原生结算字段，保存发车时车辆与改装快照，更新本地排行榜版本，退出比赛不覆盖最佳成绩。
- Integration is software-checked first; user gameplay acceptance follows the combined implementation. / 先整合并做软件检查，整合后由用户进行游戏验收。

## v0.1.0-alpha.1 — 2026-10-28

Initial independent development prerelease. / 首个独立开发预发布。

- Original launcher, local backend, isolated session handling and SDK-copy preparation. / 原创启动器、离线后端、隔离会话和 SDK 副本准备工具。
- Inventory migration, tuning plans, track catalog and native parameter encoding. / 背包迁移、改装方案、赛道目录和原生参数编码。
- Build-specific cockpit camera and D3D11 resource observer. / 指定构建的座舱相机与 D3D11 资源观察器。
- English/Chinese documentation and explicit source provenance. / 中英说明和源码来源边界。
- Software checks and recorded real-client login/equipment acceptance. / 软件检查以及真实客户端登录、换车记录。

Not complete: AI speed/item races, final DLSS SR/FG/NR output, all-resource/all-track playability, complete Classic support and Fanatec hardware acceptance. / 尚未完成：AI 竞速与道具比赛、DLSS 超分/帧生成/NR 最终画面、全资源和全赛道可玩性、经典版完整支持及 Fanatec 硬件验收。

The Windows archive is a development toolkit. It contains no game, vendor SDK, private client data or NVIDIA runtime. / Windows 压缩包是开发工具包，不含游戏、厂商 SDK、私有客户端数据或 NVIDIA 运行库。
