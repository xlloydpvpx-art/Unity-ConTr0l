# UNIFY CONTROL v1.7.0 release summary

## Delivered
- Windows WinUSB discovery for device-interface paths supported by the WinUSB driver: interface descriptors, alternate settings and endpoint/pipe metadata.
- Windows Bluetooth LE GATT discovery through C++/WinRT: visible devices, service UUIDs, characteristic UUIDs and property flags.
- Strict metadata bounds and validation; per-device errors are isolated and system-level failures do not create synthetic device records.
- Adapter SDK transport identifier `UsbWinUsb`; existing `BleGatt` identifier retained.
- Device JSON and Diagnostics UI expose WinUSB/GATT discovery statuses and metadata counts.
- Portable simulated fixtures for USB endpoints and GATT metadata.
- Existing universal identity, evidence, diagnostics, transport registry and fail-closed hardware-write gates preserved.

## Validation performed
- GCC 14.2 / CMake 3.31 portable C++20 build: passed.
- CTest device-manager suite: 1/1 target passed, including new synthetic endpoint/GATT tests.
- Frontend JavaScript syntax and JSON catalog/schema parsing: passed.
- Release ZIP integrity and SHA-256: to be recorded in the generated release manifest/checksum sidecar.

## Not validated
- MSVC/Windows SDK/C++/WinRT/WebView2 build and startup in this Linux environment.
- Physical WinUSB driver binding, endpoint enumeration, device hot-plug and composite-device interface behavior.
- Actual Windows BluetoothLE selector behavior and physical GATT service discovery.
- Vendor protocol semantics, production adapters or hardware configuration writes.

No manufacturer/model compatibility claim is made. Discovery metadata is not protocol evidence. Unknown devices remain detected-only/read-only.


## v1.7.0 release delta
The v1.6.0 discovery architecture is retained. v1.7.0 fixes Diagnostics finding aggregation and adds the Windows x64 CI/installer packaging pipeline. See `AUDIT_REPORT_v1.7.0.md` for test boundaries and unperformed Windows/physical tests.
