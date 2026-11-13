#include "unify/transport_metadata.hpp"
#include <algorithm>
namespace unify::transport {
bool validateUsbEndpoint(const UsbEndpoint& e, std::wstring& reason) {
    if ((e.address & 0x70u) != 0 || (e.address & 0x0fu) == 0) { reason=L"Invalid USB endpoint address or reserved address bits."; return false; }
    if ((e.attributes & 0xfcu) != 0 || (e.attributes & 0x03u) > 3u) { reason=L"Invalid USB endpoint transfer attributes."; return false; }
    if (e.maxPacketSize == 0 || e.maxPacketSize > 1024) { reason=L"USB endpoint maximum packet size is outside supported descriptor bounds."; return false; }
    if (e.pipeId != e.address) { reason=L"WinUSB pipe ID does not match the endpoint address."; return false; }
    reason.clear(); return true;
}
bool validateGattMetadata(const std::vector<GattService>& services, std::wstring& reason) {
    if (services.size() > 512) { reason=L"GATT service count exceeds the discovery limit."; return false; }
    std::size_t total=0;
    for (const auto& service:services) {
        if (service.uuid.empty() || service.uuid.size()>64) { reason=L"GATT service UUID is missing or oversized."; return false; }
        if (service.characteristics.size()>2048 || total+service.characteristics.size()>4096) { reason=L"GATT characteristic count exceeds the discovery limit."; return false; }
        total+=service.characteristics.size();
        for (const auto& c:service.characteristics) if(c.uuid.empty() || c.uuid.size()>64 || (c.properties & ~0x3ffu)!=0) { reason=L"GATT characteristic metadata is invalid."; return false; }
    }
    reason.clear(); return true;
}
const wchar_t* usbTransferType(std::uint8_t attributes) { switch(attributes&0x03u) {case 0:return L"CONTROL";case 1:return L"ISOCHRONOUS";case 2:return L"BULK";case 3:return L"INTERRUPT";default:return L"UNKNOWN";} }
}
