# Adapter target matrix — v1.3.0

This document separates the next adapter targets from actual compatibility claims.

| Target family | Desired functions | Current state | Why no live adapter is shipped |
|---|---|---|---|
| Hall Effect keyboards | Actuation, Rapid Trigger, SOCD, DKS, key remap, RGB | Research required | These features are vendor firmware/protocol-specific. The switch technology does not define a host protocol. |
| Gaming mice | DPI stages, polling rate, LOD, debounce, button remap, RGB | Research required | Sensor names and retail brands do not prove a configuration protocol. |
| OEM/ODM and white-label | Depends on exact board and firmware | Identity required | Rebrands may differ in VID/PID, MCU, firmware, PCB revision and receiver protocol. |

## Evidence required to promote a target

1. Exact retail model and hardware revision physically available for testing.
2. VID/PID and interface identity captured from Windows PnP/HID enumeration.
3. Firmware version or a documented reason it is unavailable.
4. Official vendor SDK/manual or a protocol source with clear provenance and legal reuse basis.
5. Transport and report layout understood; no guessing from report lengths or vendor usage pages.
6. Mock tests for every declared operation and failure path.
7. Physical read tests, followed by carefully scoped write tests only where authorized and reversible.
8. Review, signed package provenance and release notes before the host can trust the adapter.

## What v1.3.0 actually adds

- A metadata-only adapter inspection API. It exposes only operations declared by an exact, valid adapter manifest.
- Explicit read-available, write-locked and write-eligible states.
- A pre-write guard that refuses to modify a device if its current value is outside the adapter's declared bounds.
- A machine-readable research queue in `database/adapter_research_queue.json`.

## What is not claimed

No real Hall Effect keyboard or gaming mouse vendor protocol is implemented in this release. No hardware has been physically tested here. The queue intentionally contains no guessed VID/PID values and cannot be loaded as an executable adapter. Hardware writes remain disabled in the desktop host.
