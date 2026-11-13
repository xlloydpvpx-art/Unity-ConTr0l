# UNIFY CONTROL architecture (v1.6.0)

## Process boundaries
- **Frontend:** static HTML/CSS/JS in WebView2. It receives a read-only inventory and can request only `scan`.
- **Native host:** Win32/C++20. Owns SetupAPI, HIDClass and PnP topology enumeration. The browser never opens HID handles.
- **Device core:** portable C++ grouping/serialization logic, unit-tested without Windows hardware.
- **Observation store:** writes a local UTF-8 snapshot to `%LOCALAPPDATA%\\UNIFY_CONTROL\\unknown_observations.json`. No inventory is uploaded.
- **Database:** manufacturer catalog is a discovery hint only. Device entries remain empty until VID/PID/revision and protocol evidence are verified.
- **Plugins:** A strict declarative manifest schema exists; nothing more. Native plugin loading is intentionally disabled; signature verification, ABI isolation and capability allowlists are prerequisites.

## Device identity and grouping
The scanner collects HID interface path, PnP instance ID, VID/PID/revision (from PnP IDs as a fallback when opening the HID handle fails), manufacturer/product/serial strings, Usage Page/Usage, interface number, Container ID, parent instance ID and location information when Windows exposes them.

Interfaces sharing a Windows Container ID are grouped as one physical device. If Container ID is unavailable, an interface remains a separate group rather than guessing. A receiver hint is explicitly unverified and is never treated as a device identity or capability.

## Compatibility states
Every discovered interface and group remains `DETECTED ONLY`. The manufacturer catalog does not confer compatibility. The resolver may identify a model only after exact VID/PID/revision matching against a record marked VERIFIED. This identity result still does not promote any configurable capability; model-specific hardware tests and protocol evidence are separately required.

## Unknown-device observations
The observation store writes local identity observations atomically using a temporary file and rename. Serial numbers and raw device paths are excluded from the observation file. Current observations are snapshots, not an upload or telemetry channel.

## Plugin security policy
1. Manifests are data, not executable code.
2. No dynamic library is loaded in any release to date.
3. A future loader must validate publisher signatures, API version, exact device identity/revision, requested capability allowlists and protocol test evidence.
4. Plugins must not receive arbitrary process handles or unrestricted filesystem/network access.
5. Device writes remain disabled until model-specific protocol tests and recovery procedures exist.

## Known limitations
- Windows discovery includes HID collections, USB PnP devnodes, standard USB device-interface paths and Bluetooth PnP devnodes. USB endpoint/WinUSB/GATT access is not implemented in this release.
- Container IDs and parent relations depend on Windows PnP metadata and may be absent.
- Receiver identification is heuristic only.
- The current database has manufacturer names/aliases and an intentionally empty verified-model registry; exact model resolution is ready for evidence-backed entries.
- DPI, polling rate, RGB, remapping, macros and firmware writes are not enabled by generic discovery.


## Device database resolution (v0.5.0)

`DeviceDatabase` loads the local UTF-8 `database/devices.json`. It accepts manufacturer catalog names and aliases as identification hints. Exact model identification requires a record with numeric decimal VID/PID values, optional numeric revision, non-empty manufacturer/model/evidence fields, and `status: "VERIFIED"`. Conflicting records for the same identity are marked ambiguous rather than choosing one. When revision is specified it must match the device revision. A manufacturer-name match never upgrades compatibility: the interface remains `DETECTED ONLY`. The database intentionally contains no fabricated device-ID records.

## Protocol Research Center (v0.5.0)
`database/protocol_research.json` is a research queue, not a compatibility database. Each item records a manufacturer/model family, transport confidence, status, evidence level, notes and source URLs. `VERIFIED` items are rejected unless they include both an evidence level and at least one source. Research targets do not cause HID traffic or enable configuration. The UI exposes status and evidence provenance only.

## Plugin readiness boundary
The plugin manifest remains declarative. `publisherSignature` is currently a required metadata field, not a cryptographic verification result. No executable module is loaded, no publisher is trusted solely from that string, and no plugin can issue device commands. A future milestone must implement real signature verification against a trust store, manifest schema validation, ABI version negotiation, process isolation and capability-scoped IPC before enabling plugins.


## Database integrity (v0.5.0)
The loader accepts only device database schema version 3. A record marked `VERIFIED` is admitted only when VID/PID are nonzero 16-bit values, optional revision is within 16 bits, and manufacturer, model and evidence are present. Rejected records and conflicting exact identity keys are counted and surfaced in the UI. Manufacturer name/alias matching collects all candidates; collisions produce an ambiguous result rather than depending on catalog order. This remains a constrained in-project JSON reader, not a general-purpose JSON implementation; keep the data file within the documented schema and validate it as JSON during build/test.

## v0.6.0 profile boundary

The Profiles screen stores user-authored profile metadata in the persistent WebView2 origin storage. A Windows Container ID is preferred as the profile key; when absent, VID/PID/interface is used and visibly treated as potentially non-unique. Profiles contain notes and a preferred workspace only. The `settings` object is empty and `hardwareWritesEnabled` is always false. No profile is applied to HID hardware. Future settings require model-specific verified capability records and a reviewed transport implementation.

## Capability resolution (v0.7.0)

Capability declarations are loaded only from exact VERIFIED model records. The resolver allowlists DPI, polling rate, RGB, key remap, macros, battery level, profile switch and firmware update labels; requires a READ/WRITE operation and non-empty evidence; and removes duplicates. No transport adapter is registered in this release. Capabilities remain informational and are never executable.

## Adapter registry (v0.8.0)

`AdapterRegistry` is a native in-process descriptor registry. It rejects duplicate IDs, missing evidence, zero VID/PID, unknown transports, and capabilities outside a strict allowlist. Matching requires exact VID/PID and model, plus exact revision when specified. Every match remains disabled. No dynamic libraries are loaded and no HID output is sent. Enabling hardware operations requires package-signature verification, reproducible protocol evidence, physical-device test coverage, explicit user authorization, and a bounded transport implementation.

## Read-only HID descriptor inspection (v0.9.0)

After opening a HID collection with read-only access where possible, the scanner obtains `HIDP_CAPS` from OS preparsed data. It publishes report byte lengths and input/output/feature button/value capability counts. No report payloads are sent. Descriptor lengths and counts do not identify vendor commands or prove DPI/RGB support. If access fails, the scanner reports an explicit unreadable state.


## v1.0.0 application boundary
The desktop UI now presents explicit status pages for compatibility, keyboard, mouse, RGB, macros, plugins and local settings. These pages distinguish detected hardware from verified support and keep all device controls read-only. The portable release is considered stable for discovery, identification, diagnostics and local metadata profiles; universal hardware configuration is not claimed.


## v1.2.1 adapter SDK and Bluetooth discovery
The portable core now compiles `adapter_sdk.cpp`. Adapter manifests declare exact VID/PID, optional required revision, exact manufacturer/model, evidence HTTPS URI, transport, numeric firmware bounds and a bounded list of read/write operations. Read calls require a declared read capability and an online transport whose runtime type equals the manifest transport. Write calls additionally require explicit user confirmation, a declared write capability, range validation, trust gates, a pre-write read and readback verification. Reversible operations attempt a verified rollback after a failed readback. The application default policy remains `enableHardwareWrites=false`; no production transport is registered. Every SDK read/write entry point also requires an `IAuditSink`: intent must be recorded before device I/O, outcomes are recorded afterward, and failure to record an intent blocks the operation.

The Windows scanner supplements HID interface enumeration with a PnP pass for `BTHENUM`, `BTHLEDEVICE` and `BTHLE` devnodes. Those entries are marked `Bluetooth (PnP metadata only)` and `DETECTED ONLY`. This is not Bluetooth GATT discovery, pairing, or control.


## v1.3.0 adapter inspection
The SDK can now inspect a candidate adapter without communicating with the peripheral. It validates the exact identity and manifest, then returns only declared capabilities and their access state. Writes remain locked unless the host's trust service and runtime policy explicitly permit them. The desktop host still does not register a production write transport.


## v1.6.0 universal descriptor and transport pipeline

The portable core now includes bounded HID and USB descriptor parsers. They accept raw descriptor bytes supplied by a trusted capture/backend and perform no I/O. The HID parser resolves top-level usages, collections, Report IDs and Input/Output/Feature report sizes. The USB parser validates device/configuration/interface/endpoint descriptors, lengths and cross-counts.

Windows HID scanning continues to use the OS preparsed-data API and now queries button/value capability records to expose usage ranges, report IDs and link-collection identifiers. This is interpreted capability metadata; it is not represented as a raw report-descriptor capture.

The transport registry accepts trusted discovery backends with metadata or descriptor-read access only. It intentionally has no generic send/write API. The evidence engine computes a triage score and identity state from provenance-bearing observations; conflicts force manual review. The diagnostic engine generates read-only findings and is serialized into the native device inventory for the WebView2 Diagnostics page.

USB SetupAPI discovery associates standard USB device-interface paths with PnP devnodes. It does not fetch endpoint descriptors or open WinUSB pipes. The USB parser is ready for a future explicitly authorized descriptor-capture backend.
