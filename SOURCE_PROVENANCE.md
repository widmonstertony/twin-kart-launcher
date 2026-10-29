# Source provenance and public package boundary

This project is intended to become an **independent public repository, not a fork**. The code listed below was written for this launcher from scratch using documented Windows/.NET/Python APIs and facts observed from the installed clients and behavioral study of the reference project. It does not contain the original games, proprietary art, a copied launcher, or a reconstructed game implementation. The original launcher now starts the original profile backend and an externally supplied Drift client; real login/inventory/equipment traffic is accepted. Complete offline races, AI and final DLSS rendering remain unfinished.

## First-party source modules

| Module | Original work in this project | Standard dependencies |
|---|---|---|
| `src/MainForm.cs`, `Profiles.cs`, `ClientDiscovery.cs` | WinForms workflow, separate Classic/Drift profiles, calibration persistence, local Steam discovery and PE/SHA-256 inspection. | .NET Framework 4.8, Windows Registry/file APIs. |
| `src/DeviceInput.cs`, `SharedPad.cs` | Managed DirectInput wrapper and session-scoped 64-byte input channel. | .NET interop and memory-mapped files. |
| `src/CockpitForm.cs`, `native/CameraControl.h` | Original cockpit settings UI, persistent/live head/seat/FOV control contract, strict bounds and atomic file replacement handling. | .NET WinForms and Windows SDK. |
| `gateway/TrackSupportAudit.cs`, `tools/AuditTracks.cs` | Original build-matched native inventory validation and catalog reconciliation, with explicit mirror/reverse candidates, ambiguity handling and no inferred driving acceptance. Real resource captures remain private and external to published source. | .NET Framework JSON/file APIs. |
| `native/DriftAssetInventory.cpp`, `ClientBuildGuard.h` | Original exact-build guard and optional native IAssetRegistry world/AI-path metadata inventory. Native ABI facts were derived independently from the user's client. No Pak keys, resource contents or engine implementation are included. A real isolated inventory capture completed with configuration restoration. One bounded native array is retained until the owned process exits. | Windows SDK, BCrypt and the user's supported client. |
| `src/GraphicsProfile.cs` | Evidence-gated SR/FG/NR request schema. It writes no game settings or graphics hooks and does not claim any feature works yet. | .NET Framework. |
| `native/InputDevice.cpp`, `VirtualXInput.cpp`, `VirtualXInput.def` | DirectInput device enumeration/read and process-local XInput session adapter. No physical force-feedback command is sent. | Windows SDK DirectInput/XInput headers and system DLLs; MSVC. |
| `native/DriftCamera.cpp` | Independently authored, exact-build camera callback adapter, seat transform, bounded live calibration, process-local toggle and driver-head visibility control. Offsets, entry-point signatures and bone names are observations from the installed client. No vendor function bodies, skeletons, meshes, animation clips or third-party camera implementation are included. Calls to original client interfaces run on the camera callback thread. One Drift track has visual evidence of the original hands and steering wheel; lifecycle and cross-vehicle acceptance remain incomplete. | Windows SDK, BCrypt and the externally supplied supported Drift client. |
| `gateway/PackedFieldLayout.cs` | Independently authored packed-field mapping by name and width between observed version schemas; no third-party server implementation is included. | .NET Framework. |
| `src/OfflineLaunchSession.cs`, `OwnedGameModule.cs`, `OfflineGarageForm.cs` | Independent process/session management, INI backup/restore, hash-bounded native module loading, and offline tuning-plan UI. The terminal runner uses the same session implementation. | Windows APIs and .NET Framework. |
| `gateway/InventoryUnlocks.cs`, `OfflineTuning.cs`, `OfflineGarage.cs`, `TrackCatalog.cs` | Independent external-catalog grants, idempotent ownership migration, unrestricted local tuning policy and track selection. IDs, effect values, assets and captures are supplied externally. | .NET JSON serializer. |
| `gateway/OfflineProfileStore.cs`, `NativeKartParameters.cs`, `LocalPingServer.cs` | Independent atomic profile store, request dispatch, equipment/tuning/upgrade operations, time-attack state, packed native-effect rebasing, client-compatible encrypted parameter envelope and local latency service. Some peripheral read-only replies still use externally supplied response data; that is not claimed as an original implementation of those services. | .NET file, socket, JSON and cryptography APIs. |
| `native/DriftRenderProbe.cpp`, `RenderPassTrace.cpp`, `RenderPassTrace.h`, `DlssD3D11.cpp`, `DlssD3D11.h` | Original D3D11 resource/pass observation, shader hashing and NGX SR API consumer. External NVIDIA headers/libraries are required to build the consumer. No SDK implementation or NVIDIA runtime is copied into this source package. Device initialization/texture observations are distinct from actual reconstruction and final composition. | Windows SDK; separately supplied NVIDIA NGX SDK and runtime. |
| `native/DriftObjectIdentity.h` | Independently authored slot/serial/destruction checks used before retained cockpit-mesh writes. Uses supported-client object layout observations and Unreal object-lifetime principles; does not contain engine implementation code. Synthetic tests do not establish live-client lifecycle acceptance. | Windows SDK and externally supplied supported Drift client. |
| `tools/PrepareSdkCopy.cs`, `PrepareDriftClient.ps1`, `RunSession.cs` | Independent PE export inspection and exact-build SDK-copy preparation, resource junction setup and diagnostic entry point. Published code contains compatibility signatures and original replacement stubs only. The resulting SDK is a modified proprietary vendor binary, not our original library, and is never published in this package. | Windows/.NET and user's supported original installation. |
| `gateway/PacketContracts.cs`, `RoomState.cs`, `OfflineRoomService.cs` | Original typed request models, strict room transitions and native room-notification generation connected to the profile backend. Supports owner/AI slots, setting changes and exit. Field shapes were studied from observed native metadata and reference behavior; no reference function bodies are copied. Room notifications do not establish running AI racers or a ready host session. | .NET Framework JSON serializer and cryptography. |
| `gateway/WireCodec.cs` | First-party bounded JSON/hash/AES-CBC codec, now exercised with the actual Steam client. Uses random IVs and no embedded keys. | .NET Framework JSON serializer and standard cryptography. |
| `gateway/PreAuthSession.cs`, `LocalAccountSession.cs`, `DynamicDataCatalog.cs`, `LoopbackWebSocket.cs`, `GatewayHost.cs` | Independently authored RFC6455 transport, strict local identity sequence, random session token, external document catalog and diagnostic host. Field shapes were observed at the client boundary and compared with the public reference implementation. No third-party function bodies or reply templates are embedded. | .NET Framework sockets, JSON and cryptography. |
| `gateway/protocol_probe.py` | Loopback-only, transparent WebSocket/HTTP shape recorder with secret-value redaction. | Python standard library. |
| `tools/InspectClient.cs` | Read-only executable and package inventory command. | .NET Framework. |
| `tests/` | Protocol/object-lifetime fixtures and standalone D3D11 WARP checks authored for this project; dummy strings labeled `SECRET_*` are test markers, not real credentials. WARP checks exercise eight installed rendering hooks and read back actual pixels without starting a game. | .NET Framework compiler, Python standard library, MSVC and Windows SDK D3D11/D3DCompiler. |

`build.ps1`, `native/build-*.ps1`, `test-*.ps1`, `gateway/test-room.ps1`, `gateway/test-wire.ps1`, `.gitignore`, `README.md`, `gateway/PROBE.md`, `LICENSE` and this file are first-party build, test and explanatory materials. `src/ClientDiscovery.cs` and its test contain the SHA-256 of one installed Drift build as a compatibility identifier; that hash is not an account identifier or credential. The `D:\Games\...` path in `README.md` is a generic command example, not the user's machine path.

## External components and ownership boundary

`NeuralD3D12.*`, `FrameGenerationD3D12.*`, `SharedTextureBridge.*` and the three standalone graphics probes are original integration code. Public NVIDIA/Windows SDK declarations are supplied externally at build time; their implementations and runtimes are not copied into this tree. The hash-limited, opt-in NR path uses private exports of the externally supplied neural model. Its `Init_Ext` ABI differs from the core SDK API. The adapter's distinct `TwinKart..._nvngx.dll` filename satisfies that snippet's component-name convention; no system NVIDIA DLL, driver, signature policy or model file is replaced or patched. The model remains external, is not distributed, and an unknown model hash is rejected by this private path. A successful synthetic GPU fixture does not establish game guide correctness, visible composition or quality.

Original D3D11 shader reflection records resource names/slots and selected camera-field offsets. It does not save shader bytecode or vendor instruction bodies. Shared-texture fixtures exercise actual D3D11/D3D12 GPU fences and D3D12 writes returning to D3D11; CPU readback is used only to check the result. Frame generation reports native presented-frame counts, and a hidden-window fixture with interpolation disabled by NVIDIA's focus check remains explicitly unverified.

The native local-host candidate (`DriftRaceBridge.cpp`, `LocalRaceRuntime.h`, `OwnedCodePatch.h`, `GameThreadQueue.*`, `RaceControl.h`) and launcher race UI are independently written interface adapters. They call the supplied client's original loading transition, AI component registration and solo result-styler factories after an exact-build check. The owner's working iOS offline project was studied as a behavioral reference for solo mode routing, AI classification, driver preservation, table initialization and result ownership. PC function addresses, layouts and calling conventions were independently mapped from the owner's PC/Android binaries. Constants, layouts, enum values and short compatibility signatures are observations; no decompiled function bodies or reference launcher source are included. Native host travel, driving AI, finish order and settlement are separate acceptance stages. A PC trial entered one track and spawned stationary AI; the subsequent driver/solo-lobby/result changes are software-checked but await a new real trial. `TrackSupportAudit.AttachDefinitions` validates an external capture of native definitions, which is not distributed with the source.

The public BlueYeeeee/Kartrider-Drift_Offline v0.2.11 implementation has been read as a behavioral reference, as the user requested. Independent authorship does not mean no reference material was consulted. Earlier private A/B runs compiled its server implementation and used external data; those servers and sources remain excluded. The new original launcher/backend run does not load that server. Proprietary client/SDK binaries, private reference-derived data documents, account data, keys and diagnostic configurations remain external. This is not a claim that all gameplay requirements have passed.

- The user supplies authentic Classic and Drift client binaries and assets from their own installations. Those files remain outside this repository. The current Classic client has not been acquired; the Drift Steam client is installed locally.
- Windows, DirectInput, XInput, .NET Framework, MSVC and Windows SDK are build/runtime dependencies supplied by Microsoft; no copies are vendored here. Fanatec drivers and physical hardware are external.
- DLSS SR, DLSS-G/MFG and DLSS 5 neural rendering require NVIDIA's proprietary runtime/model DLLs and compatible render inputs. This repository currently includes **no NVIDIA binaries, models, Streamline SDK implementation, ReShade, RenoDX add-on or third-party graphics proxy**. Future first-party integration code may call documented APIs while the user supplies licensed runtimes separately. The Forza Motorsport mod's `transfusion-overrides/` and `third_party/mfgunlock/` contain third-party-derived material and must not be copied here as “our original code.”
- Existing `work/kartdrift` and `outputs/KartDriftOffline` reference-server, SDK shim, patcher, resource and experiment trees are private research inputs only. They are **not** part of this launcher source. The Android device audit and any phone-transfer credentials are also excluded.
- Protocol and camera research documents record observations and hypotheses; they are not implementation source. No generated decompilation, full client binary, packet body, raw user log, save or private token belongs in the public repository.

## Safe source allowlist for the first public package

Stage an empty new repository and copy **only** these paths from `work/twin-kart-launcher`:

```text
.gitignore
LICENSE
README.md
README.en.md
CHANGELOG.md
SOURCE_PROVENANCE.md
build.ps1
test-graphics.ps1
test-input.ps1
src/*.cs
native/*.cpp
native/*.h
native/*.def
native/build-*.ps1
gateway/RoomState.cs
gateway/OfflineRoomService.cs
gateway/PacketContracts.cs
gateway/WireCodec.cs
gateway/PreAuthSession.cs
gateway/LocalAccountSession.cs
gateway/DynamicDataCatalog.cs
gateway/PackedFieldLayout.cs
gateway/InventoryUnlocks.cs
gateway/OfflineTuning.cs
gateway/OfflineGarage.cs
gateway/TrackCatalog.cs
gateway/TrackSupportAudit.cs
gateway/test-tracks.ps1
gateway/OfflineProfileStore.cs
gateway/NativeKartParameters.cs
gateway/LocalPingServer.cs
gateway/test-unlocks.ps1
gateway/test-offline.ps1
gateway/test-body-layout.ps1
gateway/LoopbackWebSocket.cs
gateway/GatewayHost.cs
gateway/build-gateway.ps1
gateway/test-gateway.ps1
gateway/protocol_probe.py
gateway/test-room.ps1
gateway/test-wire.ps1
gateway/PROBE.md
tools/InspectClient.cs
tools/AuditTracks.cs
tools/PrepareSdkCopy.cs
tools/PrepareDriftClient.ps1
tools/RunSession.cs
tools/ProbeNeuralRuntime.cpp
tools/ProbeFrameGeneration.cpp
tools/ProbeSharedTexture.cpp
tests/*.cs
tests/*.cpp
tests/test_protocol_probe.py
```

Do not use a recursive copy of the workspace or `git add .` against the unfiltered working directory. The excluded paths are `build/`, `dist/`, `profiles/`, `logs/`, all `research/`, `SCOPE.md`, `__pycache__/`, generated binaries/objects, `.jsonl` captures, client/game assets, proprietary DLLs, reference-server sources or packages, SDK shim files, real configuration, save data and phone artifacts. `.gitignore` adds a second guard; the explicit allowlist remains the release boundary.

Before any public push, review the staged file list and scan **that staged tree** for absolute user paths, account names, email addresses, access keys and unexpected binaries. Build and run the software tests from the staged copy. Publish only after the list is reviewed; this document itself does not authorize or perform a GitHub action.

## Audit result on 2026-10-28

The first-party source allowlist contains no observed personal absolute path, email address, real token, API key, password or embedded vendor binary. Test-only `SECRET_*` values are synthetic redaction assertions. `build/` already contains multiple compiled `.exe`, `.dll`, `.obj`, `.lib` and `.exp` files; they are excluded. `research/` contains generated client/phone metadata and machine-specific user-profile and installed-game paths, so it is excluded as a whole. This is a source-tree review, not a provenance warranty for future additions or proof of offline gameplay.
