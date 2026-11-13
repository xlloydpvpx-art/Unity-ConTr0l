#include "unify/transport_registry.hpp"
#include <algorithm>
namespace unify::transport {
namespace {
bool validKind(Kind kind) {
    switch(kind) { case Kind::UsbHid: case Kind::UsbPnP: case Kind::UsbVendorInterface: case Kind::BluetoothHid: case Kind::BleGatt: case Kind::OtherPnP: return true; default: return false; }
}
}
bool Registry::add(std::shared_ptr<IDiscoveryBackend> backend, std::wstring& reason) {
    if(!backend) { reason=L"Null backend rejected."; return false; }
    BackendDescriptor d;
    try { d=backend->descriptor(); }
    catch(...) { reason=L"Backend descriptor threw an exception; registration rejected."; return false; }
    if(d.id.empty() || d.displayName.empty() || !validKind(d.kind)) { reason=L"Backend identity and transport kind are required."; return false; }
    if(d.maximumAccess!=Access::MetadataOnly && d.maximumAccess!=Access::DescriptorRead) {
        reason=L"Discovery registry accepts metadata/descriptor-read providers only; protocol I/O belongs to reviewed adapters."; return false;
    }
    if(!d.trusted) { reason=L"Backend is not trusted."; return false; }
    if(std::any_of(backends_.begin(),backends_.end(),[&](const auto& x){return x.descriptor.id==d.id;})) { reason=L"Duplicate backend ID."; return false; }
    backends_.push_back(Entry{d,std::move(backend)}); reason=L"Registered for discovery only."; return true;
}
std::vector<Observation> Registry::enumerateAll(std::vector<BackendStatus>& statuses) const {
    std::vector<Observation> all; statuses.clear();
    for(const auto& entry:backends_) {
        const auto& d=entry.descriptor;
        try {
            auto found=entry.backend->enumerate();
            bool mismatchedKind=false;
            for(auto& o:found) {
                if(o.kind==Kind::Unknown) o.kind=d.kind;
                if(o.kind!=d.kind) { mismatchedKind=true; continue; }
                all.push_back(std::move(o));
            }
            statuses.push_back({d.id,mismatchedKind?L"ENUMERATED_WITH_WARNINGS":L"ENUMERATED",mismatchedKind?L"Observations with a transport kind different from the registered backend were discarded.":L"Metadata discovery completed; no device configuration I/O was performed."});
        } catch(...) { statuses.push_back({d.id,L"BACKEND_ERROR",L"Discovery backend raised an exception; it was isolated."}); }
    }
    return all;
}
const wchar_t* kindName(Kind k) { switch(k) {case Kind::UsbHid:return L"USB_HID";case Kind::UsbPnP:return L"USB_PNP";case Kind::UsbVendorInterface:return L"USB_VENDOR_INTERFACE";case Kind::BluetoothHid:return L"BLUETOOTH_HID";case Kind::BleGatt:return L"BLE_GATT";case Kind::OtherPnP:return L"OTHER_PNP";default:return L"UNKNOWN";} }
const wchar_t* accessName(Access a) { switch(a) {case Access::MetadataOnly:return L"METADATA_ONLY";case Access::DescriptorRead:return L"DESCRIPTOR_READ";case Access::ReadOnlyProtocol:return L"READ_ONLY_PROTOCOL";case Access::ConfigurationWrite:return L"CONFIGURATION_WRITE";default:return L"UNKNOWN";} }
} // namespace unify::transport
