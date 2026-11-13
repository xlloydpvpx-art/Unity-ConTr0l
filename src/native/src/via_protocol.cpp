#include "unify/via_protocol.hpp"

namespace unify::via {

bool isAllowedReadCommand(std::uint8_t command) {
    return command == kGetProtocolVersion || command == kGetKeycode || command == kGetLayerCount;
}

Frame buildGetProtocolVersion() { Frame f{}; f[0] = kGetProtocolVersion; return f; }
Frame buildGetLayerCount() { Frame f{}; f[0] = kGetLayerCount; return f; }
Frame buildGetKeycode(std::uint8_t layer, std::uint8_t row, std::uint8_t col) {
    Frame f{}; f[0] = kGetKeycode; f[1] = layer; f[2] = row; f[3] = col; return f;
}

bool parseProtocolVersion(const Frame& reply, std::uint16_t& version) {
    if (reply[0] != kGetProtocolVersion) return false; // 0xFF = "unhandled": not a VIA keyboard
    version = static_cast<std::uint16_t>((reply[1] << 8) | reply[2]);
    return version != 0 && version != 0xFFFF;          // reject empty / garbage replies
}

bool parseLayerCount(const Frame& reply, std::uint8_t& layers) {
    if (reply[0] != kGetLayerCount) return false;
    layers = reply[1];
    return layers >= 1 && layers <= 32;
}

bool parseKeycode(const Frame& reply, std::uint8_t layer, std::uint8_t row, std::uint8_t col, std::uint16_t& keycode) {
    if (reply[0] != kGetKeycode || reply[1] != layer || reply[2] != row || reply[3] != col) return false;
    keycode = static_cast<std::uint16_t>((reply[4] << 8) | reply[5]);
    return true;
}

} // namespace unify::via
