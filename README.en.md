# Twin Kart Launcher

[简体中文](README.md) | English


Latest integration checkpoint (2026-10-29): a focused, visible **synthetic GPU fixture rendered 120 frames and actually presented 233**, with 113 frames reporting an extra generated presentation. It used the external NVIDIA runtime, closed its window and restored focus. This establishes frame generation in the standalone presentation module; it does not establish in-game generation or visual quality. The old unfocused hidden test generated zero extra frames and remains a failed historical check.

The native game adapter now observes the client's real six simulation/render/present markers and 64-bit frame IDs, obtains provider tokens at simulation start and paces on the simulation thread. Presentation requires the current native frame to match its pre-HUD guides. Native slots are installed only when the unpacked game creates its swapchain; external DXGI hooks still become ready before primary entry. Source tests cover thread separation, frame-zero/large IDs, duplicate/stale rejection, provider failure and teardown. No game was started for these changes.

AI/result overrides now remain scoped to the exact local-race world and its original object serial, including address reuse and map transitions. Exact native track definitions are allowed without rejecting their names solely for mirror/reverse suffixes; missing definitions remain unresolved and are never silently mapped to another route. Lower rendering percentage is requested in both native user settings and the engine cvar configuration, with the existing session restoration. Actual lower-resolution scene inputs, complete AI speed/item races, all-track driving and DLSS image quality still require completion and combined acceptance. **Do not start the game for partial testing.**

Independent launchers and an offline compatibility backend for the original **KartRider (2004 lineage)** and **KartRider: Drift** clients. This project does not recreate the racing game. It is an independently created repository, not a fork of another launcher.

**v0.1.0-alpha.1 is a development prerelease, not a complete offline racing distribution.** The release contains original launcher/development binaries and source. You must supply the original client, game assets and compatible private data. No vendor client, modified vendor SDK, player save, protocol key, NVIDIA runtime or reference-server source is distributed.

## Current state

2026-10-29 graphics update: the opt-in exact-build adapter now converts actual native depth/velocity guides, checks the camera layout and connects D3D11 → D3D12 NR → D3D11 before the HUD. Three real-game GPU readbacks each changed 1,024/1,024 sampled pixels; the written output had the same COM identity as the swapchain backbuffer and the original Present succeeded. Lobby and Village_I01 observations retained normal color and native HUD. An earlier HDR-stage attempt produced bad color; processing now runs after the original tone mapping in the 10-bit color target. **This verifies an experimental DLSS 5 NR output path, not full visual quality or moving-race acceptance. In-game SR output and generated-frame presentation remain unfinished.** Vendor runtimes/models remain external.

2026-10-29 race update: the adapted native solo item button entered `Village_I01`, with logs confirming seven actual AI pawns and their AI flags. Player driving, original animated hands/wheel and an item slot were observed in earlier trials. **The native AI registration candidate has now been loaded but AI still stayed on the grid, with no FollowTG driving component observed; complete AI races remain unfinished.** Native solo speed/item routing, an original solo result-styler fallback and readiness responses pass software checks. The native lobby defaults to `Village_I01` and then reuses the last launcher track/AI/lap request. PC interfaces were independently mapped using the owner's working iOS project as a behavioral reference; no reference launcher implementation is included.

| Area | Evidence and remaining work |
| --- | --- |
| Original launcher/backend | The real Drift client accepted login, inventory, login completion, equipment changes and repeated keepalive requests. |
| Isolated sessions | Exact-build checks, owned-process module loading, local SDK-copy preparation and INI restoration are implemented. Restoration passed the recorded test sessions. |
| Inventory and tuning | Catalog grants, idempotent ownership migration, custom/maximum tuning plans and an editor are implemented. Unsupported target-client fields can be excluded by explicit compatibility metadata. An identifier or saved setting is not proof of a usable asset or gameplay effect. |
| Time attack | Track selection, native parameter encoding, result persistence and idempotent result retries across restart have software checks. On-track acceptance through the new backend remains pending. |
| Speed/item races and AI | The original room adapter is wired to the profile backend for native room notifications, up to seven AI slots and setting changes. Software contracts pass. Same-track race hosting, AI driving, item interactions and full race settlement are not complete. |
| Cockpit view | A previous isolated track test showed the original driver's hands, wheel and steering animation, including F6 restoration. The current candidate adds head-bone anchoring, native weak-reference serial acquisition, a settings window and persistent/live seat/head/FOV controls. Control-file interoperability, migration and F6 reapplication checks pass; the updated in-game vehicle/character/map/respawn lifecycle remains pending. |
| DLSS | The experimental NR adapter executes with real native guides and writes changed GPU output to the exact backbuffer successfully presented by the game. Normal lobby and one-track colors/HUD were observed. Dynamic quality remains unverified. In-game SR and generated-frame presentation are unfinished; SR capability and synthetic evaluations alone do not establish them. |
| Resources and tracks | The original registry module captured 102,917 native asset records, including 1,585 worlds/sublevels and 101 AI-path assets, in one isolated client run. The external 234-variant catalog has 117 base aliases: 96 match exact main-world names, 2 have reverse-base candidates and 19 remain unmatched. Aliases/version differences require investigation; these counts do not establish all-track driving or latest global content. |
| Classic | A complete supported original client has not yet been obtained. |
| Fanatec | Input/calibration code is retained. Hardware acceptance is deferred until gameplay and graphics integration is ready. |

Some private resource definitions and peripheral read-only response datasets still derive from reference research. Those datasets are external, are not published and are not claimed as original content. See [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md) for the exact source/dependency boundary.

## Build

Current priority: finish real-game DLSS SR, frame generation and DLSS 5 integration before a combined user trial with the preserved cockpit, native AI host candidate and track work. Fanatec remains last. The optional inventory module is built with `native/build-assets.ps1`; add it explicitly to an isolated session manifest to produce a private `assets.jsonl`. It retains one bounded native metadata snapshot until process exit and exports no asset contents. `TwinKartTrackAudit.exe` reconciles a complete, build-matched capture against an external track catalog without claiming driving acceptance; `gateway/test-tracks.ps1` checks its failure and ambiguity handling.

Windows x64, .NET Framework 4.8, MSVC C++ Build Tools and a Windows 10/11 SDK are required for a full build. NVIDIA NGX headers/libraries are supplied separately when building the SR consumer.

The presentation candidate is built with `native/build-render.ps1 -Test -StreamlineSdkRoot <external-sdk-path>`. A private manifest's optional `Graphics` object requests `SuperResolution`, `FrameGeneration`, `NeuralRendering` and a lower `RenderPercentage`; external `TWIN_KART_NEURAL_MODULE`, `TWIN_KART_NEURAL_PROVIDER` and `TWIN_KART_FG_PROVIDER` paths must be provided separately. These options request behavior and never certify it. FG sessions wait for original graphics-hook readiness before resuming their owned primary thread; an initialization failure aborts that owned process and restores configuration. Unsupported buffer layouts retain the native path. Candidate initialization is not proof of generated frames.

Temporal SR additionally requires `TWIN_KART_DLSS_MODULE`, its lowercase `TWIN_KART_DLSS_MODULE_SHA256` and an external `TWIN_KART_DLSS_PROVIDER` directory. Build that original consumer with `native/build-dlss.ps1`. The session supplies a private cache directory. These files stay separate from source publication; a configured adapter is not an accepted temporal reconstruction path.

FG also requires the original race bridge module. It publishes an expiring game-thread snapshot of the native racing phase and pause state, so the render thread never dereferences world objects and disables interpolation in menus/loading/pause. The temporary graphics policy disables the client's competing NVAPI Reflex mode while the Streamline path owns pacing; the original INI is restored on exit. This scene-state integration has software checks, not game acceptance.

```powershell
.\build.ps1
.\gateway\build-gateway.ps1
.\native\build-camera.ps1
.\native\build-render.ps1
```

For managed tools only, use `build.ps1 -SkipNative`. The SR consumer is built separately with `native/build-dlss.ps1 -NgxSdkRoot <external-sdk-path>`; the prerelease does not bundle NVIDIA implementation binaries.

Software checks do not start a game:

```powershell
.\native\build-camera.ps1 -Test
.\native\build-render.ps1 -Test
.\gateway\test-gateway.ps1
.\gateway\test-offline.ps1
.\gateway\test-unlocks.ps1
.\gateway\test-room.ps1
.\gateway\test-wire.ps1
.\gateway\test-body-layout.ps1
```

## Use

Open `build/TwinKartLauncher.exe`, select the game and inspect the client files. The offline configuration button selects a private session manifest. The tuning editor saves a plan for the next session. Launching uses this project's backend and restores configuration after the owned game exits.

Prepare an isolated copy of the supported Steam client:

```powershell
.\tools\PrepareDriftClient.ps1 -InstalledRoot 'D:\Games\KartDrift' -DestinationRoot 'D:\OfflineGames\TwinKartDrift'
```

This prepares runtime files and a local vendor-SDK copy. It does not generate all of the external profile, protocol, resource and physics data needed for gameplay. The public prerelease alone cannot start a complete offline race.

The backend entry point is `TwinKartGateway.exe --offline <private.local.json>`. A diagnostic session runner, `TwinKartSession.exe <private.local.json>`, shares the GUI launcher's session implementation. Do not overwrite a Steam installation with development DLLs.

## Development and release policy

We study observed client behavior and public principles, then implement our own code. The project does not include a copied third-party launcher implementation. Original game files and NVIDIA components remain external dependencies owned by their respective authors.

Development proceeds in the original launcher/backend: offline basics, AI speed/item racing, graphics/cockpit integration, resource/track acceptance, then Fanatec. Integrate code and run focused software checks first; in-game acceptance is performed after integration. Previous reference-chain experiments are not treated as acceptance of the new client chain.

See [CHANGELOG.md](CHANGELOG.md) for releases. Original project source is MIT licensed; game names and trademarks belong to their owners. This project is not officially affiliated with them.
