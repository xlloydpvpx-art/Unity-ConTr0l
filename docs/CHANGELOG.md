# Changelog

## 1.6.0 — WinUSB endpoints and Bluetooth LE GATT discovery
- Added Windows WinUSB initialization for standard USB device-interface paths and metadata-only enumeration of interfaces, alternate settings and endpoint pipes.
- Added Windows C++/WinRT Bluetooth LE enumeration of Windows-visible devices, GATT service UUIDs and characteristic UUID/property flags, with a bounded wait and isolated failures.
- Added portable validation and simulated fixtures for USB endpoint metadata and GATT service/characteristic metadata.
- Added `UsbWinUsb` to the Adapter SDK transport enum; this is a transport identity, not a generic vendor protocol implementation.
- Added transport statuses and endpoint/GATT metadata to device JSON and Diagnostics UI.
- Preserved fail-closed discovery and hardware-write protections: no WinUSB transfers, no GATT value operations and no undocumented protocol commands.
- Windows/MSVC/WebView2 compilation and physical USB/Bluetooth testing remain outstanding release gates.

## 1.5.0 — Universal discovery and diagnostics engine
- Added bounded, portable HID and USB descriptor parsers for captured descriptor bytes. HID parsing covers top-level usages, collections, report IDs and Input/Output/Feature report lengths; USB parsing covers device/configuration/interface/endpoint structures and cross-count validation.
- Added strict parser limits and malformed descriptor rejection; parsing never opens a device or sends reports.
- Expanded Windows HID inspection with preparsed usage/button/value capability enumeration and report ID summaries.
- Added SetupAPI enumeration of the standard USB device-interface GUID and association of interface paths with PnP devnodes; endpoints and pipes remain unopened.
- Added a transport discovery registry with trusted metadata/descriptor-only backends, exception isolation, and explicit rejection of active I/O access levels.
- Added evidence assessment with source provenance, corroboration and conflict detection.
- Added read-only diagnostic findings for unknown devices, descriptor problems, identity conflicts and missing adapters; serialized the evidence and findings into device inventory JSON and added a Diagnostics page to WebView2.
- Added automated fixtures for HID/USB parser boundaries, report kinds/IDs, endpoint/interface consistency, transport registration policy, backend exception isolation, evidence conflicts, diagnostics and JSON fields.
- Corrected malformed inherited Bluetooth PnP string literals in the Windows scanner.
- No brand/model compatibility claims or new hardware-write paths were introduced.


## 1.4.0 — Model-independent universal discovery
- Added a portable universal classifier for transport, standard HID function, identity evidence level and report-channel observations.
- Added SetupAPI USB PnP metadata fallback for USB VID/PID devnodes that do not expose a HID collection, including non-HID/vendor-specific interfaces. No WinUSB transport is opened.
- Added universal profile metadata to the device JSON payload and surfaced it in the device inventory UI.
- Explicitly separated observed report channels from supported configuration protocols; universal classification never authorizes writes.
- Added tests for generic keyboard, mouse, unknown USB device, identity evidence and locked write state.
- No model-specific compatibility claims or hardware-write support added.

## 1.3.0 — Adapter inspection and safety hardening
- Added a no-I/O adapter inspection API that exposes only explicitly declared capabilities after exact identity, manifest, connection and transport validation.
- Capability inspection distinguishes read availability, locked writes and write eligibility; it never infers features from product category or HID report sizes.
- Added a guard that refuses to write when the current device value is outside the adapter-declared safe range.
- Added an OEM/ODM, Hall Effect keyboard and gaming mouse research queue with deliberately empty model/VID/PID fields until evidence is collected.
- Extended portable tests for capability inspection, exact identity rejection, locked-write reporting and unsafe previous-value rejection.
- No physical vendor adapter or device compatibility claim is added: this environment contains no identified target hardware, exact IDs, protocol evidence or physical test results.


## 1.2.1 — Adapter SDK and expanded discovery
- Promoted the typed C++ adapter SDK into the portable core build and documented its adapter contract.
- Added declared, bounded read operations and runtime transport identity checks; unknown or mismatched transports fail closed.
- Added mandatory audit-sink callbacks for read/write intents and outcomes; a failed intent log prevents device I/O, and a failed outcome log never reports success.
- Replaced lexical firmware-string comparisons with strict numeric dotted-version comparisons and validation of firmware ranges.
- Tightened HTTPS evidence URI validation and revision-scoped manifest validation.
- Added Windows Bluetooth Classic/BLE PnP devnode enumeration as metadata-only detection when no HID collection is exposed. No BLE GATT commands or generic Bluetooth writes are issued.
- Fixed grouping of non-HID devices without Container IDs so unrelated Bluetooth devnodes remain separate.
- Added portable tests for Bluetooth grouping, read operations, transport mismatch and firmware version ordering.
- Hardware writes remain disabled by default; mock transport tests do not constitute physical hardware validation.

## 1.1.0 — Universal compatibility foundation
- Added a broad, normalized capability catalogue for mouse, keyboard, lighting, wireless, profile and firmware features.
- Expanded native capability validation and adapter descriptors to recognize the shared vocabulary.
- Added the universal compatibility blueprint: layered support states, HID/USB/BLE discovery roadmap, adapter contract, safety model and contribution workflow.
- Expanded declarative plugin manifest capability vocabulary; executable plugin loading remains disabled.
- No unsupported device capabilities or model compatibility claims were added.

## 1.0.1
Fixes found in code review. No new device capabilities; hardware writes remain disabled.

- **Hot-plug:** the window now calls `RegisterDeviceNotification` for the HID interface class. Before, `DBT_DEVICEARRIVAL` was never delivered for HID interfaces, so the automatic refresh described in Settings did not fire. `DBT_DEVNODES_CHANGED` also triggers a debounced rescan.
- **Responsiveness:** HID enumeration runs on a worker thread. A device that hangs inside a HID call no longer freezes the window; scan requests arriving during a scan are coalesced into one follow-up scan; shutdown does not wait on a hung worker.
- **First scan:** the page requests the first scan after its message listener exists. The native host no longer scans right after `Navigate()`, which could be lost.
- **Revision identity:** the hardware revision is now read from the hardware ID (`HID\VID_xxxx&PID_xxxx&REV_xxxx`). It is not present in the HID instance ID, so the PnP fallback used to report revision 0 and could not match revision-scoped records.
- **Persistence:** WebView2 user data now lives in `%LOCALAPPDATA%\UNIFY_CONTROL\WebView2`. The default (next to the exe) is not writable under Program Files, which would have broken local profiles.
- **Frontend hardening:** Content-Security-Policy added; protocol-research source links are only made clickable for `https:` URLs.
- **Tests:** `NDEBUG` is undefined in the test file. Release builds (the default of `validate.ps1`) compiled every `assert` away, so the tests passed without checking anything. Added malformed-database, unknown-revision and hostile-string tests; verified under AddressSanitizer/UBSan.
- Housekeeping: version strings aligned to 1.0.1; two misleading-indentation warnings fixed.

## 1.0.0
Initial detection, identification, diagnostics and local-profile release.

## 1.6.0 — WinUSB endpoints and Bluetooth LE GATT discovery
- Added Windows WinUSB initialization for enumerated USB device-interface paths and metadata-only enumeration of interface/alternate-setting endpoint pipes.
- Added Windows C++/WinRT Bluetooth LE discovery for GATT services and characteristic UUID/property metadata.
- Added portable bounded validators and simulated tests for endpoint and GATT metadata.
- Added `UsbWinUsb` to the adapter SDK transport enum; this is an identifier only, not a generic protocol implementation.
- Added JSON and diagnostics UI fields for WinUSB/GATT discovery status and results.
- Kept all endpoint transfers, GATT value reads/writes and unverified configuration protocols disabled.
- Windows/MSVC/WebView2 compilation and physical hardware validation remain outstanding.


## 1.7.0 — Windows release pipeline and diagnostics hardening
- Fixed a runtime ReferenceError in Diagnostics: finding counts are now aggregated from the current device inventory.
- Added a Windows x64 GitHub Actions workflow that acquires the pinned WebView2 SDK, compiles with MSVC, runs CTest and packages the desktop application.
- Added Inno Setup installer configuration with Start Menu/Desktop shortcuts and an Evergreen WebView2 Runtime bootstrapper.
- Added branded application icon and updated version metadata.
- No device-control protocols or hardware writes were added; all unverified devices remain read-only.
- Linux portable build and unit tests are validated locally; Windows CI and physical hardware tests remain pending until the workflow runs on GitHub and devices are available.
