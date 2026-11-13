#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace unify {
enum class AdapterTransport { HidFeatureReport, HidOutputReport, VendorBulk, VendorControl, UsbWinUsb, BleGatt, Unknown };
struct AdapterDescriptor {
 std::wstring id, manufacturer, model; std::uint16_t vid{},pid{},revision{}; bool revisionSpecified{};
 AdapterTransport transport{AdapterTransport::Unknown}; std::vector<std::wstring> capabilities; std::wstring evidence;
 bool signatureVerified{false}, hardwareTested{false}, safetyReviewed{false};
};
struct AdapterMatch { std::wstring adapterId,state,reason; bool commandsEnabled{false}; };
class AdapterRegistry {
public: bool add(const AdapterDescriptor&); AdapterMatch match(std::uint16_t,std::uint16_t,std::uint16_t,const std::wstring&,const std::wstring&) const; std::size_t size() const{return adapters_.size();}
private: std::vector<AdapterDescriptor> adapters_;
};
}
