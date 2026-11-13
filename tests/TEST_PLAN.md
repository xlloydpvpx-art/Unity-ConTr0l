# UNIFY CONTROL 1.0.0 validation plan

## Automated portable tests
- [x] Group interfaces sharing one Container ID.
- [x] Keep interfaces without a Container ID separate rather than guessing.
- [x] Serialize group counts and `DETECTED ONLY` state.
- [x] Escape quotes in JSON string values.
- [x] Load UTF-8 manufacturer catalog and aliases.
- [x] Resolve exact VID/PID/revision only for records marked VERIFIED.
- [x] Match manufacturer aliases without claiming exact model identity.
- [x] Reject a revision-mismatched exact model record.
- [x] Reject ambiguous duplicate VID/PID/revision records.
- [x] Load the shipped UTF-8 catalog and resolve Chinese localized aliases.
- [x] Keep unknown VID/PID unmatched.

## Windows and physical hardware (pending Windows validation)
- [ ] Verify local UTF-8 database loading and Chinese-character manufacturer aliases on Windows.
- [ ] Compile the desktop target with Visual Studio 2022 and the WebView2 SDK.
- [ ] Launch on clean Windows 10 and Windows 11 with WebView2 Runtime.
- [ ] Enumerate standard USB keyboard and mouse; verify VID/PID and HID Usage Page/Usage.
- [ ] Verify VID/PID fallback when HID handle opening is denied.
- [ ] Attach/detach repeatedly; confirm debounced refresh.
- [ ] Test composite keyboard/mouse receiver; verify Container ID grouping.
- [ ] Test devices with missing Container ID; confirm separate groups.
- [ ] Verify parent instance ID and location metadata where exposed.
- [ ] Test blank manufacturer/product/serial strings and access-denied interfaces.
- [ ] Verify observation snapshot is valid UTF-8 JSON under LocalAppData and contains no serial number or raw device path.
- [ ] Verify WebView2 messages from non-local origins are ignored.
- [ ] Verify receiver hint is displayed only as unverified and never grants support.
- [ ] Verify unknown devices remain `DETECTED ONLY` and no output report is sent.

No device is considered protocol-supported based on enumeration or manufacturer catalog membership alone.


## v0.5.0 database integrity regression cases
- Reject schema versions other than 3.
- Reject verified VID/PID/revision values outside the USB 16-bit range.
- Reject verified model records with missing evidence.
- Count conflicting exact model keys and return an ambiguous identity.
- Return ambiguous manufacturer identification when multiple catalog entries share an alias.

## v0.6.0 profile checks
- Create a profile for a detected interface and verify it remains after application restart.
- Save the same profile name for the same device twice and verify it updates rather than duplicates.
- Delete a profile and verify it is removed after restart.
- Verify profiles without Container ID show the non-unique interface-key warning.
- Inspect stored JSON: `settings` must remain empty and `hardwareWritesEnabled` must remain `false`.
- Verify creating, editing or deleting a profile sends no WebView native message and causes no HID output.

- Capability declarations are accepted only from exact VERIFIED model records.
- Missing evidence and unknown capability names are ignored.
- No capability is executable without a registered model-specific adapter.

- Adapter registration rejects missing evidence, invalid transport/capability, incomplete identity, and duplicate IDs.
- Adapter matching requires exact VID/PID/model and optional exact revision.
- A successful adapter match remains disabled; no hardware command is executed.

- HID descriptor metadata serializes input/output/feature report lengths.
- Descriptor unreadable state does not imply unsupported device capability.
- Scanner performs descriptor inspection only; no HID writes occur.


## v1.0.0 UI and release checks
- [x] Compatibility Center distinguishes exact model matches from detected-only devices.
- [x] Keyboard, mouse, RGB and macro screens clearly state unavailable controls and do not send hardware commands.
- [x] Plugin Manager discloses that manifests are declarative and runtime loading is disabled.
- [x] Settings page reports application version and write-protection status.
- [ ] Run Windows/WebView2 startup and persistence tests on Windows 10/11.


## v1.0.1 additions
- [x] Malformed/truncated/hostile database JSON never crashes the loader and never yields a VERIFIED model.
- [x] Unknown revision (0) does not match a record that requires a specific revision.
- [x] Control characters and quotes in device strings cannot break the JSON sent to the UI.
- [x] Test asserts stay active in Release (`#undef NDEBUG`); verified with a failing canary under `-DNDEBUG`.
- [x] Tests pass under AddressSanitizer + UBSan.
- [ ] Windows: plug/unplug a USB keyboard or mouse; the list refreshes within ~1 s without pressing Rescan.
- [ ] Windows: first launch shows the device list without pressing Rescan.
- [ ] Windows: with a device that stalls on `CreateFile`/`HidD_Get*`, the window stays responsive and closes cleanly.
- [ ] Windows: a device whose hardware ID contains `REV_xxxx` reports that revision, also when the HID handle cannot be opened.
- [ ] Windows: profiles persist after restart when installed under `Program Files`.


## v1.2.1 additions
- [x] Portable mock test for declared adapter read and out-of-range returned values.
- [x] Portable mock test that runtime transport must equal the manifest transport.
- [x] Portable mock audit sink records read/write intents and outcomes; unavailable audit sink prevents writes before transport I/O.
- [x] Numeric dotted firmware comparison and invalid/reversed firmware-bound rejection.
- [x] Bluetooth devnode grouping fallback when HID interface paths and Container IDs are absent.
- [ ] On Windows, verify PnP-only discovery for a Bluetooth Classic device and a BLE device that does not publish a HID collection. Confirm both remain `DETECTED ONLY`.
- [ ] Confirm no GATT pairing, characteristic writes or device configuration occurs.
- [ ] Verify Win32 scanner and WebView2 build with MSVC 2022.


## v1.2.1 additions
- [x] Adapter inspection is metadata-only and rejects invalid manifests and non-exact identities.
- [x] Only explicitly declared operations are exposed; undeclared capabilities are not inferred.
- [x] Write operations are visibly locked unless all trust gates and host policy are enabled.
- [x] A current value outside declared bounds blocks a write before transport write is called.
- [x] Existing transport mismatch, audit-intent failure, confirmation, readback and rollback cases remain covered.
- [ ] Physical tests for exact Hall Effect keyboard and gaming mouse models (no target hardware was supplied).


## v1.6.0 universal discovery tests
- HID raw descriptor parser: top-level collection, usage page/usage, report IDs, Input/Output/Feature bit and byte lengths, long-item truncation, short-item truncation, Report ID zero, mixed implicit/explicit IDs, unbalanced PUSH/POP and collection nesting/resource bounds.
- USB descriptor parser: device descriptor fields, configuration total length, interface/alternate-setting counts, endpoint addresses/direction, endpoint counts, malformed and truncated records.
- Transport registry: trusted metadata backend accepted; duplicate, untrusted and active-I/O backends rejected; backend exceptions isolated.
- Evidence engine: partial identity, source-corroborated catalog identity, conflicting identity values and detected-only state.
- Diagnostics: unknown usage, no HID interface, unreadable/invalid descriptor, identity conflict and missing/unverified adapter findings.
- Serialization: evidence assessment, report IDs, usage detail and diagnostic findings included in device JSON.

## v1.6.0 simulated transport metadata tests
- Accept a valid bulk-IN endpoint with a matching WinUSB pipe ID.
- Reject reserved endpoint-address bits, endpoint zero, mismatched pipe IDs and oversized packet sizes.
- Map endpoint transfer type metadata without inferring device configuration semantics.
- Accept bounded GATT service/characteristic UUID metadata and reject missing/oversized UUIDs or invalid property flags.
- Serialize WinUSB and GATT status/metadata into the device JSON payload.
- Validate the SDK's `UsbWinUsb` transport identifier while preserving exact-manifest requirements.
- On Windows hardware, manually verify that no WinUSB transfer or GATT characteristic read/write is issued during discovery.


## v1.7.0 frontend regression tests
- Diagnostics aggregation counts ERROR and WARNING findings across multiple device entries.
- Empty inventories and missing/null diagnostics arrays return zero counts without throwing.
- Node syntax validation checks the frontend script before packaging.
