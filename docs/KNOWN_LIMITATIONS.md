# Known limitations — UNIFY CONTROL 1.7.0

## Universal discovery boundaries
- Universal means that Windows-exposed devices can enter a common discovery/diagnostic pipeline; it does not mean every device can be configured.
- Windows scanner enumerates HID interfaces, generic USB PnP devnodes, standard USB device-interface paths, and Bluetooth PnP devnodes. Some devices/drivers do not expose all metadata.
- WinUSB endpoint metadata and BLE GATT service/characteristic metadata are enumerated where Windows exposes supported devices; no data transfers or characteristic value operations are performed.
- The portable HID and USB descriptor parsers accept raw descriptors from a trusted capture/backend. Windows HID preparsed-data APIs used by the scanner expose interpreted caps, not the original raw descriptor byte stream.
- HID Output/Feature report availability does not identify a vendor protocol and never authorizes writes.
- The transport registry accepts metadata/descriptor-only backends. Active protocol transports must be implemented as separately reviewed adapters.
- Evidence scores are triage indicators, not statistical confidence or compatibility claims. Conflicting evidence forces manual review.
- Diagnostics are deterministic read-only findings and do not prove hardware health.

## Build/runtime
- Portable C++20 core is testable on non-Windows platforms.
- Windows/MSVC/WebView2 executable build, runtime startup, hot-plug behavior and physical-device matrix remain external validation gates.
- Windows runtime and physical transport behavior still require external validation. Generic protocol control remains unimplemented.
- No physical keyboard or mouse configuration writes are enabled by this release.

## v1.6.0 transport discovery boundaries
- WinUSB endpoint metadata is available only when a present standard USB device-interface path is bound to a compatible WinUSB driver and Windows grants the required handle access. This build does not enumerate arbitrary vendor-specific interface GUIDs; devices exposed only through a custom GUID may remain PnP-only. The application queries interface descriptors and pipe metadata; it does not issue bulk, interrupt, control or isochronous transfers.
- WinUSB enumeration is attempted through the standard USB device-interface path. Composite devices with vendor interfaces may require a device-specific interface GUID or INF configuration before Windows exposes a WinUSB-compatible path.
- Bluetooth LE discovery is limited to devices returned by the Windows BluetoothLE device selector at scan time; the GATT worker has a 15-second scan-wide wait budget and late results are discarded. Visibility depends on Windows pairing/discovery state, radio state, permissions and device availability. The application enumerates GATT services and characteristic properties only; it does not read characteristic values, subscribe to notifications or write values.
- GATT UUID/property metadata does not establish device identity, protocol semantics or configuration compatibility.
- The portable tests use synthetic metadata fixtures. They do not exercise WinUSB, Windows Bluetooth APIs, real endpoints, physical peripherals or driver installation.
- Windows SDK/C++/WinRT/MSVC and WebView2 compilation has not been performed in the Linux validation environment. Validate the Windows application and hardware behavior on a Windows 10/11 x64 machine before distribution.


## v1.7.0 release pipeline status
- The Windows build workflow is prepared but has not been executed from this environment; no Windows `.exe` or installer is claimed as locally compiled.
- The installer bundles the WebView2 Evergreen bootstrapper. Runtime installation still requires network access and Windows installer permissions.
- The current diagnostics UI counts only findings returned by the native scanner; it does not perform independent hardware stress tests.
