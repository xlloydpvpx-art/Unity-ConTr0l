# HID descriptor analysis — v1.6.0

UNIFY CONTROL now contains a bounded, portable HID report-descriptor parser. It processes a raw descriptor supplied by a trusted capture/backend and reports:

- top-level collection usage page and usage;
- collection nesting and collection types;
- Input, Output and Feature report bit/byte lengths per Report ID (byte length includes the Report ID prefix when present);
- short and long item bounds;
- global PUSH/POP balance, collection balance and report-ID validity.

## Safety limits

- Maximum raw descriptor: 4096 bytes.
- Maximum global PUSH depth: 16.
- Maximum collection nesting: 32.
- Maximum report size/count: 4096 each, with checked multiplication.
- Maximum aggregate report size per report ID and kind: 8 Mi bits.
- Malformed or oversized descriptors are rejected; they are never used to send reports.

## Windows acquisition boundary

The Windows scanner uses HID preparsed data from `HidD_GetPreparsedData` and `HidP_GetCaps`, `HidP_GetButtonCaps`, and `HidP_GetValueCaps` to expose report lengths, report IDs and usage ranges. The Windows HID API does not expose the original raw report-descriptor byte stream through those calls, so the portable raw-byte parser is available to trusted capture/import backends; the application does not claim to have captured raw bytes when it has only preparsed capabilities.

## Interpretation

A report field or Output/Feature report channel is structural metadata only. It is not proof of a vendor protocol, device setting, safe command, or write permission. The scanner remains read-only.
