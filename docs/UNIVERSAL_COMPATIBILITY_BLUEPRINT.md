# UNIFY CONTROL — Universal Compatibility Architecture

## Objective
Build one application that can discover and configure peripherals across manufacturers, OEMs, ODMs, white-label vendors and regional brands. Universal means a common framework and a growing adapter ecosystem; it does **not** mean one undocumented command works on every device.

## 1. Compatibility must be layered

| Layer | What it proves | User-facing state |
|---|---|---|
| A. Bus discovery | Windows sees a USB/Bluetooth/PnP device | Detected |
| B. Interface discovery | HID collections, USB interfaces, usage pages and endpoints are enumerated | Interfaces discovered |
| C. Identity | Exact VID/PID/revision/model evidence | Identified / Unknown / Ambiguous |
| D. Read support | A tested adapter can safely read a specific setting | Read supported |
| E. Write support | A tested adapter can safely change and verify a specific setting | Configure supported |
| F. Advanced support | Onboard memory, firmware, recovery and vendor-specific features are independently tested | Advanced supported |

Never infer a higher layer from a lower one. A HID output report length is not proof of an RGB command. A brand string is not proof of model identity. A shared OEM enclosure is not proof that two firmware builds use the same protocol.

## 2. Discovery backends

The Windows discovery roadmap must cover more than HID collections:
- SetupAPI / Configuration Manager for PnP devices, parent-child relations, Container IDs and hot-plug notifications.
- HIDClass for keyboard, mouse, consumer-control and vendor-defined HID collections; collect preparsed descriptor metadata read-only.
- USB device/interface enumeration for vendor-specific interfaces that do not expose HID collections. Keep endpoint descriptors and interface alternate settings as observations only.
- Bluetooth LE / GATT discovery as a separate transport, with service/characteristic enumeration before any write support.
- Composite devices and 2.4 GHz receivers must be represented as a physical parent plus multiple interfaces/children when Windows exposes that topology.

No scan should send vendor commands. Discovery should be cancellable, bounded, and resilient to devices disappearing mid-scan.

## 3. Protocol adapter model

An adapter is scoped to an exact identity range and firmware/revision constraints. It declares:
- transport: HID feature, HID output, USB control, USB bulk, BLE GATT, or documented vendor SDK;
- report/interface/endpoint selectors and report sizes;
- each capability independently, including READ versus WRITE;
- protocol evidence, tested firmware, tested hardware revisions and test fixtures;
- safe defaults, timeouts, retry policy, read-back verification and recovery behavior.

A device may use multiple adapters for different interfaces or transports. A capability can be read-only while another is writable. Never unlock all settings merely because one command was reverse engineered.

## 4. Capability catalogue

`database/capability_catalog.json` defines the shared vocabulary. It covers mouse sensor tuning, polling rate, DPI stages, button mapping, macros, keyboard layers, actuation, Rapid Trigger, SOCD, DKS, Mod-Tap, per-key RGB, profiles, onboard memory, battery, firmware information and recovery-related operations.

The catalogue is not a compatibility list. Each device record must opt into only the operations that have model-specific evidence and a working implementation.

## 5. Generic support versus vendor support

- Standard HID keyboard/mouse input works through Windows without vendor configuration and should not be advertised as programmable by UNIFY CONTROL.
- Standard HID feature/output reports may be inspected read-only, but report IDs and payload formats are device-specific.
- Vendor-defined HID, USB control/bulk and BLE characteristics require protocol-specific adapters.
- Public SDKs and documented protocols should be preferred over reverse engineering.
- If multiple products share a chipset, create a chipset-family adapter only after confirming firmware/protocol equivalence; keep explicit allowlisted model variants.

## 6. Safety and trust

All write operations remain disabled until all gates pass:
1. Exact device and firmware scope match.
2. Adapter package integrity and publisher signature are cryptographically verified.
3. Requested capability is explicitly allowlisted.
4. Transport operation is bounded by report size, timeout, rate limit and device identity.
5. The user confirms the change.
6. The adapter performs read-back verification where possible and records a local result.
7. Firmware operations additionally require a verified image, power-loss recovery path and device-specific recovery procedure.

Do not allow arbitrary plugin code to access raw device handles. The future plugin host should use a narrow RPC protocol, least privilege, per-capability permissions, resource limits and crash isolation. Declarative manifests alone do not make code safe.

## 7. Community compatibility growth

Create a contribution workflow that accepts anonymized descriptor captures, exact model/revision labels, firmware version, protocol traces, source links and repeatable test results. Serial numbers, user paths and unrelated device identifiers must be stripped. Submissions enter `RESEARCH_REQUIRED`; maintainers review them before `VERIFIED` status. Add regression fixtures so new adapters cannot break existing device families.

## 8. Priority implementation roadmap

1. Complete Windows discovery: HID + non-HID USB interfaces + PnP topology + Bluetooth LE metadata.
2. Replace the constrained hand-written JSON parser with a maintained JSON library and versioned schemas.
3. Implement the adapter SDK and transport abstraction with no dynamic loading initially; test adapters in-process against mock transports.
4. Add signed package verification and isolated plugin host before third-party native plugins.
5. Add protocol adapters using public/documented protocols first, then carefully reviewed researched protocols.
6. Implement settings transaction flow: read current value, validate range, preview, user approval, write, read back, rollback if supported.
7. Add hardware-in-the-loop CI fixtures and a public compatibility contribution process.

## 9. Definition of done for a setting

A setting is not complete because it appears in the UI. It is complete only when the exact model/revision is identified, the correct transport/interface is selected, the command is documented or reproducibly researched, invalid values are rejected, the operation is tested on physical hardware, read-back matches the requested value, failure behavior is safe, and regression tests pass.

## Current release boundary
This blueprint defines the universal architecture and the complete shared capability vocabulary. The current codebase remains a discovery and evidence framework. It does not yet enumerate every non-HID USB or BLE transport, load third-party plugins, or configure arbitrary devices. Those are implementation milestones, not claims of present support.
