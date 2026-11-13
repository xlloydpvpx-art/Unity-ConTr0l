# USB descriptor analysis — v1.6.0

The portable USB descriptor parser accepts raw descriptor bytes supplied by a trusted capture/backend. It performs no USB I/O. It supports:

- 18-byte USB device descriptor parsing (USB version, VID/PID, revision, class and configuration count);
- configuration descriptor `wTotalLength` validation;
- interface descriptors, including alternate settings;
- endpoint descriptors (address, direction/number, attributes, maximum packet size and interval);
- interface endpoint-count validation and unique interface-number validation.

The parser rejects truncated/oversized buffers, invalid descriptor lengths/types, endpoints without a preceding interface, reserved endpoint-address bits, endpoint zero, zero maximum packet size, inconsistent endpoint counts and inconsistent interface counts. The maximum configuration blob is 65535 bytes.

Windows SetupAPI enumeration in this release discovers USB PnP devnodes and the standard USB device-interface path. It does not issue control transfers to fetch raw USB descriptors or open arbitrary vendor interfaces. The parser is ready for a future explicitly authorized descriptor-capture backend; it does not imply that endpoint details are available for every device today.
