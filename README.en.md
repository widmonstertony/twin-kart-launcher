# Twin Kart Launcher

[简体中文](README.md) | English

## v0.2.0-alpha.1 release checkpoint (2026-10-30)

This is the current public-source checkpoint of the independently authored launcher, offline compatibility backend, native rooms, AI, cockpit, and graphics adapters. It remains a **development prerelease**. Users must provide the original client, private compatibility data, and NVIDIA environment. The Release contains no game, first-party assets, protocol keys, SDK, model, or NVIDIA runtime.

- The native lobby path covers create/join/leave, map and team selection, seven AI members, and strict start boundaries for 13 local modes. Seven AI finished one Spitem solo mixed race and reached the original result page; six AI finished one Infinite Booster duo race with original scoring. Flag AI objectives/navigation, every mode rule, and every track are not fully accepted.
- v88 fixes blank lobby AI previews. Random selection is restricted to the exact PC client's 51 registered character/costume pairs and 211 registered kart bodies while retaining seven distinct AI members. Private registration evidence is used only for local validation and is not published.
- An actual Rudolph run showed the original hands, wheel, steering animation, and F6 round-trip. New phase-aware code restores the complete third-person character for finish/result phases. This is not visual acceptance of every kart/character combination.
- One real-game graphics interval recorded 120 native frames, 240 provider presentations, a qualified SR path, and two successful Movie NR results. The current low-resolution double-NR → SR → FG performance reorder passes software/GPU boundary checks; 120 FPS, dynamic quality, and the latest in-game result still require user acceptance.
- Of 234 catalog track IDs, the latest client scan produced 100 native catalog definitions, 101 successful queried definitions, and 138 empty queries. Missing definitions are never silently redirected to another map; unlocking an ID does not create missing game assets.

The public repository contains only independently authored project source and rebuildable development tools. It does not copy another launcher's source or distribute first-party game content.

Local integration check (2026-10-30 UTC, unpublished): the HDR SR consumer now serializes GPU draining, old-owner teardown and reconstruction on the new render thread. An actual NVIDIA offscreen check completed four evaluations and two owner transitions. Native infinite reversed-Z uses positive infinity and rejects `FLT_MAX`, which aliases NVIDIA's uninitialized sentinel; real Streamline accepted the camera constants, without presenting generated frames in that check. Moving synthetic inputs completed seven evaluations through the original SR → DLSS 5 NR adapter, including size/owner rebuilds; GPU readback checked motion direction, scaling and changing output. No game was started. Sustained in-game generation and dynamic visual quality remain unaccepted.

A subsequent complete synthetic chain rendered **120 frames and presented 238**, with 120 SR and 120 DLSS 5 evaluations and a reconstruction-consumer teardown/rebuild halfway through. Original code identifies the shared native COM device and retires only its own features, preserving the presentation owner's NVIDIA instance; final teardown drains reconstruction before stopping presentation. That complete run and cleanup logged no Streamline errors or invalid infinite-far sentinel warning. Full in-game gameplay and dynamic quality remain unaccepted.


Latest integration checkpoint (2026-10-29): a focused, visible **synthetic GPU fixture rendered 120 frames and actually presented 233**, with 113 frames reporting an extra generated presentation. It used the external NVIDIA runtime, closed its window and restored focus. This establishes frame generation in the standalone presentation module; it does not establish in-game generation or visual quality. The old unfocused hidden test generated zero extra frames and remains a failed historical check.

The native game adapter now observes the client's real six simulation/render/present markers and 64-bit frame IDs, obtains provider tokens at simulation start and paces on the simulation thread. Presentation requires the current native frame to match its pre-HUD guides. Native slots are installed only when the unpacked game creates its swapchain; external DXGI hooks still become ready before primary entry. Source tests cover thread separation, frame-zero/large IDs, duplicate/stale rejection, provider failure and teardown. No game was started for these changes.

AI/result overrides now remain scoped to the exact local-race world and its original object serial, including address reuse and map transitions. Exact native track definitions are allowed without rejecting their names solely for mirror/reverse suffixes; missing definitions remain unresolved and are never silently mapped to another route. Lower rendering percentage is requested in both native user settings and the engine cvar configuration, with the existing session restoration. These paragraphs retain historical integration evidence; the release checkpoint above is authoritative for current acceptance.

Independent launchers and an offline compatibility backend for the original **KartRider (2004 lineage)** and **KartRider: Drift** clients. This project does not recreate the racing game. It is an independently created repository, not a fork of another launcher.

**v0.2.0-alpha.1 is a development prerelease, not a complete offline racing distribution.** The release contains original launcher/development binaries and source. You must supply the original client, game assets and compatible private data. No vendor client, modified vendor SDK, player save, protocol key, NVIDIA runtime or reference-server source is distributed.

## Current state

2026-10-29 graphics update: the opt-in exact-build adapter now converts actual native depth/velocity guides, checks the camera layout and connects D3D11 → D3D12 NR → D3D11 before the HUD. Three real-game GPU readbacks each changed 1,024/1,024 sampled pixels; the written output had the same COM identity as the swapchain backbuffer and the original Present succeeded. Lobby and Village_I01 observations retained normal color and native HUD. An earlier HDR-stage attempt produced bad color; processing now runs after the original tone mapping in the 10-bit color target. **This verifies an experimental DLSS 5 NR output path, not full visual quality or moving-race acceptance. In-game SR output and generated-frame presentation remain unfinished.** Vendor runtimes/models remain external.

2026-10-29 race update: the adapted native solo item button entered `Village_I01`, with logs confirming seven actual AI pawns and their AI flags. Player driving, original animated hands/wheel and an item slot were observed in earlier trials. **The native AI registration candidate has now been loaded but AI still stayed on the grid, with no FollowTG driving component observed; complete AI races remain unfinished.** Native solo speed/item routing, an original solo result-styler fallback and readiness responses pass software checks. The native lobby defaults to `Village_I01` and then reuses the last launcher track/AI/lap request. PC interfaces were independently mapped using the owner's working iOS project as a behavioral reference; no reference launcher implementation is included.

| Area | Evidence and remaining work |
| --- | --- |
| Original launcher/backend | The real Drift client accepted login, inventory, login completion, equipment changes and repeated keepalive requests. |
| Isolated sessions | Exact-build checks, owned-process module loading, local SDK-copy preparation and INI restoration are implemented. Restoration passed the recorded test sessions. |
| Inventory and tuning | Catalog grants, idempotent ownership migration, custom/maximum tuning plans and an editor are implemented. Unsupported target-client fields can be excluded by explicit compatibility metadata. An identifier or saved setting is not proof of a usable asset or gameplay effect. |
| Time attack | Track selection, native parameter encoding, result persistence and idempotent result retries across restart have software checks. On-track acceptance through the new backend remains pending. |
| Speed/item races and AI | Native personal/team rooms and 13 local mode boundaries are connected. Seven AI finished an observed Spitem solo race; six finished an Infinite Booster duo race with original scoring. Flag AI, every item effect, every mode rule, and complete per-mode settlement remain unaccepted. |
| Cockpit view | An actual Rudolph run showed the original hands, wheel, steering animation, and F6 restoration. Phase-aware code now limits head hiding to the native driving phase. Every kart/character, hit/finish/respawn/map-transition combination still needs visual acceptance. |
| DLSS | One older-order real-game interval recorded 120 native frames, 240 provider presentations, a qualified SR path, and successful double NR. The current performance reorder passes software/GPU boundaries, but latest dynamic quality, latency, 120 FPS, and sustained stability remain unaccepted. |
| Resources and tracks | The latest 234-ID scan produced 100 native catalog definitions, 101 successful queried definitions, and 138 empty queries. Missing definitions stay unavailable instead of being silently aliased; these counts do not establish all-track driving or latest global content. |
| Classic | A complete supported original client has not yet been obtained. |
| Fanatec | Input/calibration code is retained. Hardware acceptance is deferred until gameplay and graphics integration is ready. |

Some private resource definitions and peripheral read-only response datasets still derive from reference research. Those datasets are external, are not published and are not claimed as original content. See [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md) for the exact source/dependency boundary.

## Build

Current priority: finish real-game DLSS SR, frame generation and DLSS 5 integration before a combined user trial with the preserved cockpit, native AI host candidate and track work. Fanatec remains last. The optional inventory module is built with `native/build-assets.ps1`; add it explicitly to an isolated session manifest to produce a private `assets.jsonl`. It retains one bounded native metadata snapshot until process exit and exports no asset contents. `TwinKartTrackAudit.exe` reconciles a complete, build-matched capture against an external track catalog without claiming driving acceptance; `gateway/test-tracks.ps1` checks its failure and ambiguity handling.

Windows x64, .NET Framework 4.8, MSVC C++ Build Tools and a Windows 10/11 SDK are required for a full build. NVIDIA NGX headers/libraries are supplied separately when building the SR consumer.

The presentation candidate is built with `native/build-render.ps1 -Test -StreamlineSdkRoot <external-sdk-path>`. A private manifest's optional `Graphics` object requests `SuperResolution`, `FrameGeneration`, `NeuralRendering` and a lower `RenderPercentage`; external `TWIN_KART_NEURAL_MODULE`, `TWIN_KART_NEURAL_PROVIDER` and `TWIN_KART_FG_PROVIDER` paths must be provided separately. These options request behavior and never certify it. FG sessions wait for original graphics-hook readiness before resuming their owned primary thread; an initialization failure aborts that owned process and restores configuration. Unsupported buffer layouts retain the native path. Candidate initialization is not proof of generated frames.

Temporal SR additionally requires `TWIN_KART_DLSS_MODULE`, its lowercase `TWIN_KART_DLSS_MODULE_SHA256` and an external `TWIN_KART_DLSS_PROVIDER` directory. Build that original consumer with `native/build-dlss.ps1`. The session supplies a private cache directory. These files stay separate from source publication; a configured adapter is not an accepted temporal reconstruction path.

NR also requires `TWIN_KART_NEURAL_MODULE_SHA256`: the lowercase SHA-256 of your independently built consumer from `native/build-neural.ps1 -NgxSdkRoot <external-sdk-path> -OutputName TwinKartNeuralSR2_nvngx`. Record the hash of that exact output in the private manifest, including when running the offscreen reconstruction probe. Both managed preflight and the native renderer verify it. This permits clean rebuilds without accepting an unpinned DLL; external vendor-model interface checks remain separate.

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
