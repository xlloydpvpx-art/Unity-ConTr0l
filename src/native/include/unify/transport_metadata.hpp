#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace unify::transport {
struct UsbEndpoint {
    std::uint8_t interfaceNumber{};
    std::uint8_t alternateSetting{};
    std::uint8_t address{};
    std::uint8_t attributes{};
    std::uint16_t maxPacketSize{};
    std::uint8_t interval{};
    std::uint8_t pipeId{};
};
struct GattCharacteristic {
    std::wstring uuid;
    std::uint32_t properties{};
};
struct GattService {
    std::wstring uuid;
    std::vector<GattCharacteristic> characteristics;
};
bool validateUsbEndpoint(const UsbEndpoint& endpoint, std::wstring& reason);
bool validateGattMetadata(const std::vector<GattService>& services, std::wstring& reason);
const wchar_t* usbTransferType(std::uint8_t attributes);
}
