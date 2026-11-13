# UNIFY CONTROL Adapter SDK — v1.3.0

## Purpose

The SDK is a typed C++20 contract for adding model-specific read/write adapters without changing the device discovery core. It is not a plugin loader and it does not provide a production USB, HID vendor, Bluetooth, or BLE transport implementation.

## Adapter identity

An adapter must match all of the following:

- Exact 16-bit VID and PID.
- Exact manufacturer and product/model strings as supplied by the identity pipeline.
- Exact hardware revision when `revisionSpecified` is true; an unknown revision does not match.
- Firmware within the declared numeric dotted-version range, when a range is declared.

No fuzzy name matching is used. Numeric dotted firmware versions such as `1.2.0` and `1.10.0` are compared by numeric components, not lexicographically. Vendor-specific strings such as `FW_ABC` are intentionally rejected until an adapter-specific comparator is designed and reviewed.

## Declaring operations

Each operation declares a capability from the SDK allowlist, a `Read` or `Write` kind, minimum and maximum values, a unit, whether readback is required, and whether rollback is supported. Duplicate capability/kind pairs, unknown capabilities, invalid bounds, and writes without readback are rejected.

The declarative shape is documented by `plugins/adapter-manifest.schema.json`. This schema is data validation only; the application does not load code from manifests. `plugins/manifest.schema.json` remains the separate general plugin schema.

## Runtime contract

`IScalarTransport` must:

1. Report whether the device is still connected.
2. Report its concrete `Transport` type. `Unknown` or a mismatch with the manifest is rejected.
3. Implement only the documented scalar operations for the exact adapter.

`AdapterSdk::executeRead` requires a declared read operation, checks connection and transport, records a `READ_REQUESTED` audit event before I/O, then validates the returned value against declared bounds and records the outcome.

`AdapterSdk::execute` requires a declared write operation, checks bounds and explicit user confirmation, enforces host trust gates and runtime policy, records `WRITE_REQUESTED` before I/O, reads the previous value, performs the write, and reads back the result. Reversible operations attempt to restore and verify the previous value after a failed readback. Disconnects and failed stages are returned as explicit states and passed to the audit sink. If the audit sink cannot record the request, device I/O is not started; if it cannot record the outcome, success is not reported.

## Trust and safety boundary

- `RuntimePolicy` defaults to `enableHardwareWrites=false`.
- The desktop application does not register a production hardware-write transport.
- `signatureVerified`, `safetyReviewed`, and `hardwareTested` are trust-gate inputs supplied by trusted host code. They are **not** cryptographic proof and must never be copied from untrusted manifest fields.
- A future host trust service must verify signatures against a maintained trust store, review adapter provenance and safety, and record physical test evidence before setting these flags.
- No generic HID report writes, firmware updates, arbitrary commands, or dynamic native plugin loading are introduced by this SDK.
- Mock transport tests validate control flow only. They do not establish compatibility with a real peripheral.

## Adding an adapter safely

1. Obtain vendor documentation, an official SDK, or a legally usable, reproducible protocol source.
2. Record exact model, VID/PID, hardware revision and firmware constraints.
3. Declare only capabilities and bounded operations proven by that evidence.
4. Implement an isolated transport that rejects any other identity and reports its actual transport type.
5. Add mock tests for normal operation, invalid values, read failure, disconnect, readback mismatch and rollback.
6. Perform physical tests on each declared hardware/firmware revision; document results and limitations.
7. Have the host trust service verify the package and review it. Until that service and a production transport are available, application hardware writes remain disabled.

## Capability inspection (v1.3.0)

`AdapterSdk::inspect` is a metadata-only operation. It validates the manifest and exact device identity, checks the connected state and transport type, and reports only capabilities explicitly declared by that adapter. Write operations are reported as locked unless the host policy is enabled and all three trust inputs are present. Inspection does not send reports, read settings, infer Hall Effect support, or probe unknown protocols.

Before a write, the SDK now also checks that the current value read from the device is inside the operation's declared safe range. If not, it refuses to write.

## Current support state

The SDK contract, validation, read/write orchestration, transport matching and mock tests are implemented. No production model adapter, signed adapter package verification, sandbox, or hardware transport is shipped in v1.3.0.

## v1.6.0 transport identifiers
`UsbWinUsb` is now accepted by the SDK and declarative adapter schema alongside `BleGatt`. These are transport identifiers only. The v1.6.0 Windows scanner discovers WinUSB endpoint metadata and BLE GATT service/characteristic metadata, but it does not ship a generic `IScalarTransport` implementation for either transport. Discovery metadata, endpoint presence, GATT UUIDs and characteristic property flags do not establish a device-specific command protocol. Any future implementation must be scoped to exact identity and verified protocol evidence; hardware writes remain disabled in the desktop host.
