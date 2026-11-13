#pragma once
// Windows raw-HID transport for the VIA protocol (read-only probe). Windows only.
#include <string>
#include "unify/via_protocol.hpp"

namespace unify::via {

struct ProbeResult {
    bool ok{false};
    std::uint16_t protocolVersion{};
    std::uint8_t layerCount{};
    std::wstring error; // short ASCII code, safe to embed in JSON
};

// Sends only allow-listed READ requests (protocol version, layer count) to the raw HID interface
// at `devicePath`. Never writes configuration. Blocking: call it off the UI thread.
ProbeResult probe(const std::wstring& devicePath);

} // namespace unify::via
