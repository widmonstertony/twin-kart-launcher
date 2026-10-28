# Changelog / 更新记录

## Unreleased / 后续开发

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
