# UNIFY CONTROL — Audit and development report 1.7.0

## Scope
Audited the supplied `UNIFY_CONTROL_v1_6_0.zip` and continued its existing architecture. The archive contained 78 ZIP entries. No wholesale rewrite was performed.

## Architecture found
- C++20 portable device core under `src/native`.
- Windows desktop host in `src/native/src/main.cpp`, using Win32, SetupAPI/HID-related APIs, WebView2 and Windows Bluetooth/WinUSB discovery modules.
- Frontend in `src/frontend` (HTML/CSS/JavaScript), served through the `https://unify.local/` WebView2 virtual host mapping.
- Local JSON catalogs in `database/`.
- Adapter SDK, evidence engine, capability engine, transport registry, diagnostics and bounded descriptor parsers.
- Portable C++ unit tests in `tests/unit`.

## Implemented in the base v1.6.0
- HID interface discovery and Windows preparsed capability metadata.
- USB PnP/device-interface discovery.
- WinUSB interface/endpoint metadata where the standard path and driver permit it; no endpoint transfers.
- Bluetooth PnP and BLE GATT service/characteristic metadata; no GATT value reads/writes.
- Exact-evidence identity matching, adapter manifests and capability vocabulary.
- Read-only diagnostics and unknown-device observations.
- Local profiles stored in the WebView2 user-data profile.

## v1.7.0 changes
- Fixed the Diagnostics page's undefined `all` reference by aggregating findings from each device's `diagnostics` array; added Node regression tests for mixed findings and empty inventories.
- Updated project version and release notes.
- Added an application icon resource.
- Added a Windows 2022 x64 GitHub Actions pipeline that acquires a pinned WebView2 SDK, builds with MSVC, runs CTest, stages the app, compiles an Inno Setup installer and uploads artifacts.
- Added WebView2 Evergreen Runtime bootstrapper packaging and installation.
- Added portable validation invariants for the Diagnostics fix, icon and version.

## Validation performed in this environment
- CMake configure: PASS (CMake 3.31.6, GNU C++ 14.2.0).
- Portable C++ core build: PASS.
- CTest: PASS, 1/1 test target.
- Node JavaScript syntax check: PASS (Node 22.16.0).
- JSON parsing for device/protocol/capability/research catalogs and plugin schemas: PASS.
- Frontend release invariants: PASS.
- Frontend diagnostics regression tests: PASS.

## Not performed / not claimed
- Windows/MSVC/WebView2 executable compilation: NOT RUN here (environment is Linux and does not provide the Windows SDK/MSVC toolchain).
- Inno Setup compilation: NOT RUN here.
- Windows app startup and UI interaction: NOT RUN.
- Physical USB/HID/BLE peripheral matrix: NOT PERFORMED.
- Keyboard/mouse configuration writes: NOT IMPLEMENTED; intentionally locked.

## Functional inventory
| Area | Status | Notes |
|---|---|---|
| Portable C++ core | Implemented and tested | Existing v1.6.0 architecture retained |
| Windows HID/USB discovery | Implemented in source | Windows runtime test pending |
| WinUSB endpoint metadata | Implemented in source | No endpoint data transfers |
| BLE GATT metadata | Implemented in source | No characteristic value operations |
| Exact identity/evidence | Implemented | Unknown/conflicting devices remain detected-only |
| Adapter SDK | Framework implemented | No broadly validated commercial device adapters included |
| Rapid Trigger / actuation | Not supported | No tested model-specific protocol adapter |
| Mouse DPI / polling / debounce | Not supported | No tested model-specific protocol adapter |
| RGB / macros / remapping | Not supported | UI explains controls are locked |
| Profiles | Implemented in frontend | Local WebView2 storage; Windows persistence test pending |
| Diagnostics | Implemented | v1.7.0 runtime reference bug fixed; Windows UI test pending |
| Windows installer | Definition added | Must be compiled by Windows workflow |
| Windows EXE | Pending | Requires successful Windows runner build |

## Known technical limitations
- Discovery is bounded by what Windows and the active driver expose.
- Standard USB device-interface paths do not cover every vendor-specific interface GUID.
- BLE visibility depends on Windows Bluetooth discovery/pairing state and permissions.
- Metadata and report lengths do not reveal vendor protocol semantics.
- A device being recognized by brand or VID/PID does not authorize configuration.
- The GitHub workflow must be run in a GitHub repository to create Windows artifacts; it has not been triggered from this environment.
