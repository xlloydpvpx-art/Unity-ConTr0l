#include "unify/adapter_registry.hpp"
#include <algorithm>
#include <set>
namespace unify { namespace {
bool validTransport(AdapterTransport t){return t==AdapterTransport::HidFeatureReport||t==AdapterTransport::HidOutputReport||t==AdapterTransport::VendorBulk||t==AdapterTransport::VendorControl||t==AdapterTransport::UsbWinUsb||t==AdapterTransport::BleGatt;}
bool validCapability(const std::wstring& c){static const std::set<std::wstring> a{
 L"DPI",L"DPI_XY",L"POLLING_RATE",L"LIFT_OFF_DISTANCE",L"ANGLE_SNAPPING",L"MOTION_SYNC",L"RIPPLE_CONTROL",L"ASYMMETRIC_CUTOFF",L"SENSOR_CALIBRATION",
 L"BUTTON_REMAP",L"MACROS",L"DEBOUNCE_TIME",L"SCROLL_SETTINGS",L"MOUSE_PROFILES",L"BATTERY_LEVEL",L"POWER_MANAGEMENT",
 L"KEY_REMAP",L"KEYBOARD_MACROS",L"LAYOUT_SETTINGS",L"TAP_HOLD",L"SOCD",L"RAPID_TRIGGER",L"ADJUSTABLE_ACTUATION",L"DKS",L"MT",L"SNAP_TAP",L"NKRO_MODE",L"KEYBOARD_PROFILES",
 L"RGB",L"PER_KEY_RGB",L"RGB_EFFECTS",L"RGB_BRIGHTNESS",L"BACKLIGHT",L"PROFILE_SWITCH",L"ONBOARD_MEMORY",L"FIRMWARE_VERSION",L"FIRMWARE_UPDATE",L"DEVICE_RESET",L"WIRELESS_MODE",L"BATTERY_HEALTH"};return a.contains(c);}
}
bool AdapterRegistry::add(const AdapterDescriptor& d){
 if(d.id.empty()||d.manufacturer.empty()||d.model.empty()||d.evidence.empty()||d.vid==0||d.pid==0||!validTransport(d.transport)||d.capabilities.empty())return false;
 if(std::any_of(adapters_.begin(),adapters_.end(),[&](const auto& x){return x.id==d.id;}))return false;
 for(const auto& c:d.capabilities) { if(!validCapability(c)) return false; }
 adapters_.push_back(d);return true;
}
AdapterMatch AdapterRegistry::match(std::uint16_t vid,std::uint16_t pid,std::uint16_t rev,const std::wstring& model,const std::wstring& cap)const{
 for(const auto& d:adapters_)if(d.vid==vid&&d.pid==pid&&d.model==model&&(!d.revisionSpecified||d.revision==rev)&&std::find(d.capabilities.begin(),d.capabilities.end(),cap)!=d.capabilities.end())return {d.id,L"ADAPTER_MATCHED_DISABLED",L"Execution disabled: signature verification, hardware validation, safety review, and runtime transport are not available in this build.",false};
 return {L"",L"NO_EXACT_ADAPTER",L"No adapter matches this exact VID/PID/model/revision and capability.",false};
}
}
