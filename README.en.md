# Twin Kart Launcher

Offscreen NVIDIA check: the original D3D11 NGX consumer now passes two real HDR SR evaluations at 640×360 → 1280×720. GPU completion/readback finds all 921,600 output pixels changed from the cleared target, no nonfinite RGB channels and preserved pre-existing pipeline state. `native/build-dlss.ps1 -BuildProbe` builds the original hash-checked `ProbeTemporalSR` tool. Inputs are synthetic; no window, swapchain, Present or game is created, and this does not verify the native game's temporal source or image quality.

The latest source adds a separate pre-AA HDR temporal SR adapter. It reads independently mapped native input/output rectangles, jitter and reciprocal pre-exposure, converts cropped raw guides, calls the original D3D11 NGX consumer and replaces only a fully qualified native dispatch. Unsupported shaders/layouts or vendor failures retain the original dispatch. The known captured 1:1 TAA layout is deliberately rejected as SR; an actual smaller in-game source and supported upscale permutation remain unverified. WARP tests execute the conversion, request/copy boundary, native padding preservation and failure/state restoration with an explicitly synthetic consumer. They do not establish NVIDIA evaluation or gameplay quality.

Cockpit controls now publish an atomic history revision for camera changes; reconstruction also resets across rejected/missing frames, teleports, orientation cuts and FOV changes. FG receives zero jitter for the already-resolved color/guide grid. DXGI resize defers bridge retirement and Streamline reconfiguration to the established render thread. Every requested graphics session now waits for hook readiness before primary entry, and checks original module hashes/dependency files before changing user configuration. **No game is being launched for partial tests; combined game acceptance remains gated on completing the requested integration.**

Latest source integration candidate wires a qualified lower-resolution native SDR input through DLSS SR and NR, preserves native HUD rendering, and routes a D3D11 shadow-buffer interface to an original D3D12/Streamline presentation chain on the actual game HWND. It includes pre-primary-thread hook readiness, native camera decoding, conservative UI masking, asynchronous input fences and reconstruction resize handling. Builds, original startup fixtures, WARP interface/state checks and a 2,048-pixel shared-texture test using the real Streamline device pass. **The new paths have not been run in the game; complete DLSS, AI races and all-track support are not claimed.**

Graphics source checkpoint: an opt-in exact-build adapter now executes DLSS 5 NR on actual game color/depth/motion inputs and writes the result into the native pre-HUD output. GPU readbacks and same-backbuffer successful Present records verify the experimental output path. Dynamic visual quality, in-game DLSS SR and generated-frame presentation remain unfinished. Separate 640×360 → 1280×720 SR GPU fixtures use synthetic inputs and do not establish game integration. NR's private ABI path is restricted to one external model SHA-256; vendor models and SDKs remain external.

The original Streamline FG/Reflex/PCL adapter accepts a 2× configuration on the local GPU, which reports one supported generated frame. An earlier hidden-window fixture reported one presentation per rendered frame because the window was unfocused. Generated frames remain unverified. The new original game-HWND presentation candidate replaces that hidden presentation approach but still needs combined game verification. The shared-texture bridge passes a D3D12-write-to-D3D11 readback test for all 2,048 pixels, including on the real Streamline-created device. Shader reflection retains no shader instructions.

Latest source checkpoint: an independently authored, opt-in native local-host candidate now connects the launcher to the client's own character/kart preparation and level-travel interfaces. Its UI requests speed or item mode, 1–7 AI opponents, laps and a track. Managed/native request interoperability, same-process game-thread dispatch and builds pass. **Same-grid starts, driving AI, item interactions and settlement remain unverified and are not claimed complete.** The existing room-protocol start boundary still returns an error instead of claiming a host exists.

All 234 external catalog IDs have now been queried through the actual PC client's native track-definition interface: 96 return definitions and 138 are empty. Empty results comprise 117 mirror variants, 2 non-mirror reverse variants and 19 other base aliases. Mirror/reverse support needs further adaptation; unlocking IDs does not add missing native definitions or assets. The new report validates build, thread, complete coverage and counts, and keeps definition availability separate from map/AI resources and driving acceptance. The launcher retains all entries and labels unresolved ones.

The native host candidate requires `EnableExperimentalRaceHost`, a matching `TrackReportPath`, and an explicitly listed `TwinKartDriftRaceBridge.dll` in the private launch manifest. Start the game, log in and stay in the lobby, then use the launcher's AI candidate window. `TwinKartLauncher.exe --start-manifest <private.local.json>` opens the launcher and starts its owned game session while keeping the live cockpit and race controls available. Run `native/build-race-bridge.ps1 -Test` and `gateway/test-tracks.ps1` for the corresponding software checks. This checkpoint does not complete DLSS SR, FG or NR quality acceptance.

[简体中文](README.md) | English

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
