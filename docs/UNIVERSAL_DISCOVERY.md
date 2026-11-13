# Universal discovery pipeline — v1.6.0

## Purpose

The universal layer classifies any device interface exposed by Windows without requiring a manufacturer/model adapter. It is deliberately separate from the adapter SDK: discovery and classification are generic; configuration remains protocol-specific and evidence-gated.

## Discovery sources

- HID class interfaces: SetupAPI + HID API; HID preparsed data provides top-level collection usage and report lengths/capability counts.
- USB PnP devnodes: SetupAPI metadata fallback for USB VID/PID nodes that do not expose a HID collection. This includes devices whose interface is not handled by the HID class. No WinUSB handle, endpoint, control pipe, or vendor command is opened.
- Bluetooth Classic and BLE PnP devnodes: metadata-only fallback where Windows exposes the devnode. BLE GATT service/characteristic discovery and writes are not implemented in this release.
- Composite grouping: Windows Container ID remains the strongest grouping key; interfaces without a Container ID remain separate rather than being guessed together.

## Universal profile

`universal::Classifier` is a portable, model-independent layer. It emits:

- transport category (`USB_HID`, `BLUETOOTH_HID`, `BLE_PNP`, `USB_PNP`, `USB_VENDOR_SPECIFIC`, `OTHER_PNP`, `UNKNOWN`)
- standard function class (keyboard, mouse, consumer control, game controller, unknown)
- identity evidence level (unknown, hardware ID, hardware ID + strings, exact catalog match)
- observations for input reports and the existence of output/feature report channels
- explicit `configurationProtocolKnown=false` and `hardwareWritesAllowed=false`

A report channel is not proof that a vendor configuration protocol exists. Output/feature report sizes, usage pages, VID/PID, strings, and product names never independently enable configuration.

## Adapter discovery and operation model

The SDK remains independent of model families. An adapter may be introduced for any transport only when its manifest declares exact identity/revision/firmware scope, evidence, bounded operations and transport. The SDK checks exact model identity, firmware range, operation allowlists, read-before-write, user confirmation, audit intent/outcome, readback and rollback where reversible. Production write policy remains disabled in the application.

The project intentionally does not dynamically load arbitrary third-party executable plugins. Declarative manifests are data, not executable commands.

## Current coverage and limitations

- Universal metadata enumeration is broad but limited to devices Windows exposes through the selected SetupAPI classes and PnP IDs. It cannot see every proprietary receiver protocol or device hidden behind a vendor driver.
- Windows HID discovery uses preparsed capability metadata. A bounded independent HID report-descriptor parser and USB device/configuration/interface/endpoint descriptor parser are included for raw descriptor captures supplied by trusted backends; Windows does not yet capture raw USB descriptors or enumerate WinUSB pipes.
- Bluetooth PnP detection is not BLE GATT support.
- USB vendor-specific PnP detection is not WinUSB transport support.
- No generic writes, firmware flashing, undocumented commands, or inferred feature controls are enabled.
- Physical Windows hardware matrix and MSVC/WebView2 build remain external validation gates.

## Evidence basis

Microsoft documents HID discovery, report movement, and HID parser APIs as distinct areas. Applications intended for unknown HID report layouts must parse descriptors/usages rather than assuming vendor report structures. USB vendor-specific communication through WinUSB additionally requires a device using the WinUSB function driver and explicit interface/endpoint discovery. References:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hid-api
- https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/using-winusb-api-to-communicate-with-a-usb-device
- https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/standard-usb-descriptors
- https://www.usb.org/hid


## v1.6.0 discovery layers
1. **OS discovery:** SetupAPI/Configuration Manager enumerates HID collections, USB PnP devnodes, standard USB device-interface paths and Bluetooth PnP metadata.
2. **Descriptor interpretation:** Windows HID preparsed capabilities are collected; raw HID/USB descriptor parsers are available for bounded captures supplied by trusted backends.
3. **Transport provider registry:** extensible metadata/descriptor-only discovery providers are registered under a trust boundary. Active protocol I/O is rejected from this registry.
4. **Evidence assessment:** observed hardware IDs, revision, strings, parent/container relations, descriptor evidence and independently sourced catalog records are assessed; conflicts require manual review.
5. **Diagnostics:** per-device read-only findings are emitted to the UI. No finding enables a hardware operation.

The Windows scanner does not currently capture raw USB configuration descriptors, enumerate WinUSB pipes, enumerate BLE GATT characteristics, or infer vendor protocol commands.
