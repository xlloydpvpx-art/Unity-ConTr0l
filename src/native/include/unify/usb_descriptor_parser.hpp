#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace unify::usb {
constexpr std::uint8_t DescriptorDevice=0x01, DescriptorConfiguration=0x02, DescriptorInterface=0x04, DescriptorEndpoint=0x05;
struct DeviceDescriptor { std::uint16_t usbVersion{}, vendorId{}, productId{}, deviceVersion{}; std::uint8_t deviceClass{}, deviceSubClass{}, deviceProtocol{}, maxPacketSize0{}, configurations{}; bool valid{false}; std::wstring error; };
struct EndpointDescriptor { std::uint8_t address{}, attributes{}, interval{}; std::uint16_t maxPacketSize{}; };
struct InterfaceDescriptor { std::uint8_t number{}, alternateSetting{}, interfaceClass{}, interfaceSubClass{}, interfaceProtocol{}, declaredEndpoints{}; std::vector<EndpointDescriptor> endpoints; };
struct ConfigurationResult { bool valid{false}; std::wstring error; std::uint16_t totalLength{}; std::uint8_t configurationValue{}, declaredInterfaces{}; std::vector<InterfaceDescriptor> interfaces; std::size_t descriptorCount{}; };
class DescriptorParser {
public:
    static constexpr std::size_t MaxDescriptorBytes=65535;
    static DeviceDescriptor parseDevice(const std::vector<std::uint8_t>& bytes);
    static ConfigurationResult parseConfiguration(const std::vector<std::uint8_t>& bytes);
};
} // namespace unify::usb
