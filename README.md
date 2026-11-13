# UNIFY CONTROL 1.8.0

**Windows 10/11 x64 · C++20 · SetupAPI/HID · WinUSB · Bluetooth LE GATT · WebView2**

UNIFY CONTROL is a model-independent keyboard, mouse and peripheral discovery and diagnostics platform. Its universal pipeline records what Windows can actually expose, separates observed metadata from verified protocol evidence, and keeps unknown devices in a detected-only state.

> **Compatibility truth:** universal discovery does not mean universal configuration. A manufacturer, VID/PID, USB endpoint, HID report or GATT UUID is not proof of a configurable protocol.

## v1.7.0 additions
- Fixed diagnostics aggregation so the Diagnostics screen counts per-device findings without referencing an undefined variable.
- Added a reproducible Windows x64 GitHub Actions build, automated portable validation, artifact packaging and an Inno Setup installer definition.
- Added WebView2 SDK acquisition to the Windows build script and explicit Evergreen Runtime installation handling in the installer package.
- Updated release/version documentation and introduced a branded application icon.

## v1.6.0 additions
- **WinUSB endpoint metadata:** opens only Windows USB device-interface paths that can be initialized by WinUSB; queries interface/alternate-setting and pipe metadata. It performs no endpoint transfers.
- **Bluetooth LE GATT metadata:** uses Windows C++/WinRT APIs to enumerate visible BLE devices, GATT service UUIDs and characteristic UUID/property flags. It performs no characteristic value reads, writes or notification subscriptions.
- **SDK transport identifier:** adds `UsbWinUsb` alongside `BleGatt`; these identifiers do not implement generic vendor protocols.
- **Portable validators and fixtures:** bounded endpoint and GATT metadata validation, malformed fixture rejection, JSON serialization checks and exception-isolated discovery design.

## Preserved capabilities
- HID interface discovery and Windows preparsed capability analysis, including report lengths, usages, button/value capabilities and report IDs.
- USB PnP devnode and standard USB device-interface path discovery.
- Bluetooth Classic/BLE PnP metadata discovery.
- Bounded portable HID report-descriptor and USB descriptor parsers for trusted captured bytes.
- Extensible discovery registry, evidence provenance, identity-conflict handling and read-only diagnostics.
- Exact-evidence device database, capability vocabulary, protocol research center and Adapter SDK.
- WebView2 frontend, portable C++20 tests and validation scripts.

## Security boundaries
- No generic HID Output/Feature report writes.
- No WinUSB bulk, interrupt, control or isochronous transfers.
- No GATT characteristic value reads/writes or notification subscriptions.
- No undocumented vendor commands or firmware flashing.
- The desktop hardware-write policy remains disabled. Any future adapter operation requires exact identity, a valid trusted manifest, protocol evidence, safety review, physical testing, audit and the SDK's existing verification gates.

## Build and validation
- CI: `.github/workflows/windows-release.yml` builds the native Windows x64 app and installer on `windows-latest`; release artifacts are uploaded by the workflow.
- Portable core: CMake 3.24+ and C++20; `bash scripts/validate.sh` on Linux/macOS environments with CMake, compiler, Node.js and Python.
- Windows desktop: Visual Studio/MSVC, Windows 10/11 SDK and Microsoft WebView2 SDK; `build-windows.ps1` can acquire the pinned SDK with NuGet when no SDK path is supplied.
- Portable test fixtures do not replace Windows compilation or physical device testing. See `docs/BUILD_VALIDATION.md` and `docs/KNOWN_LIMITATIONS.md`.
