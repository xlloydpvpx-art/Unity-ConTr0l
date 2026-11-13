#include "unify/capability_engine.hpp"
#include <set>
namespace unify {
std::vector<CapabilityResolution> CapabilityEngine::resolve(const std::vector<CapabilityRecord>& records) {
 static const std::set<std::wstring> allowed{
  L"DPI",L"DPI_XY",L"POLLING_RATE",L"LIFT_OFF_DISTANCE",L"ANGLE_SNAPPING",L"MOTION_SYNC",L"RIPPLE_CONTROL",L"ASYMMETRIC_CUTOFF",L"SENSOR_CALIBRATION",
  L"BUTTON_REMAP",L"MACROS",L"DEBOUNCE_TIME",L"SCROLL_SETTINGS",L"MOUSE_PROFILES",L"BATTERY_LEVEL",L"POWER_MANAGEMENT",
  L"KEY_REMAP",L"KEYBOARD_MACROS",L"LAYOUT_SETTINGS",L"TAP_HOLD",L"SOCD",L"RAPID_TRIGGER",L"ADJUSTABLE_ACTUATION",L"DKS",L"MT",L"SNAP_TAP",L"NKRO_MODE",L"KEYBOARD_PROFILES",
  L"RGB",L"PER_KEY_RGB",L"RGB_EFFECTS",L"RGB_BRIGHTNESS",L"BACKLIGHT",
  L"PROFILE_SWITCH",L"ONBOARD_MEMORY",L"FIRMWARE_VERSION",L"FIRMWARE_UPDATE",L"DEVICE_RESET",L"WIRELESS_MODE",L"BATTERY_HEALTH"
 };
 std::vector<CapabilityResolution> out; std::set<std::pair<std::wstring,std::wstring>> seen;
 for(const auto& r:records) {
  if(!allowed.contains(r.name) || (r.operation!=L"READ" && r.operation!=L"WRITE") || r.status!=L"VERIFIED" || r.evidence.empty()) continue;
  if(!seen.emplace(r.name,r.operation).second) continue;
  CapabilityResolution c; c.name=r.name; c.operation=r.operation; c.evidence=r.evidence;
  c.state=r.implementation.empty()?L"VERIFIED_NOT_IMPLEMENTED":L"IMPLEMENTATION_NOT_REGISTERED"; c.executable=false; out.push_back(std::move(c));
 } return out;
}
}
