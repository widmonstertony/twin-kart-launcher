# Twin Kart Launcher

Latest source checkpoint: an independently authored, opt-in native local-host candidate now connects the launcher to the client's own character/kart preparation and level-travel interfaces. Its UI requests speed or item mode, 1–7 AI opponents, laps and a track. Managed/native request interoperability, same-process game-thread dispatch and builds pass. **Same-grid starts, driving AI, item interactions and settlement remain unverified and are not claimed complete.** The existing room-protocol start boundary still returns an error instead of claiming a host exists.

All 234 external catalog IDs have now been queried through the actual PC client's native track-definition interface: 96 return definitions and 138 are empty. Empty results comprise 117 mirror variants, 2 non-mirror reverse variants and 19 other base aliases. Mirror/reverse support needs further adaptation; unlocking IDs does not add missing native definitions or assets. The new report validates build, thread, complete coverage and counts, and keeps definition availability separate from map/AI resources and driving acceptance. The launcher retains all entries and labels unresolved ones.

The native host candidate requires `EnableExperimentalRaceHost`, a matching `TrackReportPath`, and an explicitly listed `TwinKartDriftRaceBridge.dll` in the private launch manifest. Start the game, log in and stay in the lobby, then use the launcher's AI candidate window. `TwinKartLauncher.exe --start-manifest <private.local.json>` opens the launcher and starts its owned game session while keeping the live cockpit and race controls available. Run `native/build-race-bridge.ps1 -Test` and `gateway/test-tracks.ps1` for the corresponding software checks. This checkpoint does not complete DLSS SR, FG or DLSS 5 final composition.

[简体中文](README.md) | English

Independent launchers and an offline compatibility backend for the original **KartRider (2004 lineage)** and **KartRider: Drift** clients. This project does not recreate the racing game. It is an independently created repository, not a fork of another launcher.

**v0.1.0-alpha.1 is a development prerelease, not a complete offline racing distribution.** The release contains original launcher/development binaries and source. You must supply the original client, game assets and compatible private data. No vendor client, modified vendor SDK, player save, protocol key, NVIDIA runtime or reference-server source is distributed.

## Current state

| Area | Evidence and remaining work |
| --- | --- |
| Original launcher/backend | The real Drift client accepted login, inventory, login completion, equipment changes and repeated keepalive requests. |
| Isolated sessions | Exact-build checks, owned-process module loading, local SDK-copy preparation and INI restoration are implemented. Restoration passed the recorded test sessions. |
| Inventory and tuning | Catalog grants, idempotent ownership migration, custom/maximum tuning plans and an editor are implemented. Unsupported target-client fields can be excluded by explicit compatibility metadata. An identifier or saved setting is not proof of a usable asset or gameplay effect. |
| Time attack | Track selection, native parameter encoding, result persistence and idempotent result retries across restart have software checks. On-track acceptance through the new backend remains pending. |
| Speed/item races and AI | The original room adapter is wired to the profile backend for native room notifications, up to seven AI slots and setting changes. Software contracts pass. Same-track race hosting, AI driving, item interactions and full race settlement are not complete. |
| Cockpit view | A previous isolated track test showed the original driver's hands, wheel and steering animation, including F6 restoration. The current candidate adds head-bone anchoring, native weak-reference serial acquisition, a settings window and persistent/live seat/head/FOV controls. Control-file interoperability, migration and F6 reapplication checks pass; the updated in-game vehicle/character/map/respawn lifecycle remains pending. |
| DLSS | Original D3D11 observation and an NGX SR consumer exist. SR availability was reported on the game's real device. The current observer correlates sampled draw/dispatch bindings and shader hashes; its WARP checks preserve pipeline state and references. Actual SR output, generated frames and DLSS 5 neural-rendering composition are not verified. |
| Resources and tracks | The original registry module captured 102,917 native asset records, including 1,585 worlds/sublevels and 101 AI-path assets, in one isolated client run. The external 234-variant catalog has 117 base aliases: 96 match exact main-world names, 2 have reverse-base candidates and 19 remain unmatched. Aliases/version differences require investigation; these counts do not establish all-track driving or latest global content. |
| Classic | A complete supported original client has not yet been obtained. |
| Fanatec | Input/calibration code is retained. Hardware acceptance is deferred until gameplay and graphics integration is ready. |

Some private resource definitions and peripheral read-only response datasets still derive from reference research. Those datasets are external, are not published and are not claimed as original content. See [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md) for the exact source/dependency boundary.

## Build

Current priority: cockpit view, complete AI races and all tracks. Further DLSS and Fanatec work is deferred. The optional inventory module is built with `native/build-assets.ps1`; add it explicitly to an isolated session manifest to produce a private `assets.jsonl`. It retains one bounded native metadata snapshot until process exit and exports no asset contents. `TwinKartTrackAudit.exe` reconciles a complete, build-matched capture against an external track catalog without claiming driving acceptance; `gateway/test-tracks.ps1` checks its failure and ambiguity handling.

Windows x64, .NET Framework 4.8, MSVC C++ Build Tools and a Windows 10/11 SDK are required for a full build. NVIDIA NGX headers/libraries are supplied separately when building the SR consumer.

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
