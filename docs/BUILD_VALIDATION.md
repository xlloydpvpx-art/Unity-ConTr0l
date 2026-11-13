# Build and validation — UNIFY CONTROL 1.7.0

## Portable validation performed
Environment: Linux, GCC 14.2, CMake 3.31, C++20, Node.js and Python.

Command:

```bash
BUILD_DIR=/tmp/unify-control-v160-build bash scripts/validate.sh
```

Results:
- CMake configure: PASS
- Portable C++20 core build: PASS
- CTest `device_manager_core`: PASS (1/1)
- Simulated USB endpoint validation: PASS
- Simulated GATT metadata validation: PASS
- JSON serialization checks for WinUSB/GATT metadata: PASS
- Frontend `node --check`: PASS
- Database/plugin JSON parsing: PASS

## Not performed
- Windows/MSVC/Windows SDK/C++/WinRT/WebView2 build or application startup.
- Physical WinUSB driver binding, interface/alternate-setting/endpoint enumeration, USB hot-plug or composite-device testing.
- Windows BluetoothLE selector behavior, actual BLE GATT enumeration, permissions or physical BLE testing.
- Physical vendor protocol validation or hardware configuration writes.

The synthetic tests validate metadata bounds and serialization only. They are not evidence that a physical device or Windows driver exposes a particular endpoint or GATT service. A Windows release build and hardware matrix remain required before distribution.

## v1.7.0 Windows CI release pipeline

The repository includes `.github/workflows/windows-release.yml`. Push a `v*` tag or use the GitHub Actions **workflow_dispatch** trigger. The runner installs/acquires the pinned Microsoft.Web.WebView2 SDK, compiles the x64 Windows GUI target with MSVC, runs CTest, stages `UNIFY_CONTROL_RELEASE`, downloads the WebView2 Evergreen bootstrapper, compiles the Inno Setup installer and uploads the ZIP/installer/build report as workflow artifacts. Physical peripheral tests are not performed by CI.

For a local Windows build, run `build-windows.ps1` from PowerShell in a Visual Studio Developer environment. It can download the pinned WebView2 SDK using NuGet if `-WebView2SdkDir` is omitted. The script stages the application in `dist/UNIFY_CONTROL_RELEASE`.
