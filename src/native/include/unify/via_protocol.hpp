#pragma once
// QMK/VIA raw-HID protocol codec (portable, no I/O). Command IDs and reply layouts were checked
// against qmk_firmware quantum/via.h and via.c. Only an allow-list of READ commands can be built;
// destructive commands (bootloader jump, EEPROM reset, keymap reset) have no builder on purpose.
#include <array>
#include <cstddef>
#include <cstdint>

namespace unify::via {

constexpr std::size_t kFrameBytes = 32;       // qmk RAW_EPSIZE
constexpr std::uint16_t kUsagePage = 0xFF60;  // qmk RAW_USAGE_PAGE
constexpr std::uint16_t kUsage = 0x61;        // qmk RAW_USAGE_ID
using Frame = std::array<std::uint8_t, kFrameBytes>;

constexpr std::uint8_t kGetProtocolVersion = 0x01;
constexpr std::uint8_t kGetKeycode = 0x04;
constexpr std::uint8_t kGetLayerCount = 0x11;
constexpr std::uint8_t kUnhandled = 0xFF;

bool isAllowedReadCommand(std::uint8_t command);

Frame buildGetProtocolVersion();
Frame buildGetLayerCount();
Frame buildGetKeycode(std::uint8_t layer, std::uint8_t row, std::uint8_t col);

bool parseProtocolVersion(const Frame& reply, std::uint16_t& version);
bool parseLayerCount(const Frame& reply, std::uint8_t& layers);
bool parseKeycode(const Frame& reply, std::uint8_t layer, std::uint8_t row, std::uint8_t col, std::uint16_t& keycode);

} // namespace unify::via
