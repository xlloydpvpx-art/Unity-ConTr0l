#include "unify/universal_discovery.hpp"
#include <algorithm>

namespace unify::universal {
namespace {
void add(Profile& p, Observation value) {
    if (std::find(p.observations.begin(), p.observations.end(), value) == p.observations.end())
        p.observations.push_back(value);
}
}
Profile Classifier::classify(const Input& in) {
    Profile p;
    p.transport = in.transport;
    p.hasVendorProductId = in.vid != 0 && in.pid != 0;
    p.hasRevision = in.revisionKnown && in.revision != 0;
    if (in.catalogExactMatch && p.hasVendorProductId) p.identity = IdentityLevel::CatalogExactMatch;
    else if (p.hasVendorProductId && in.hasManufacturerString && in.hasProductString) p.identity = IdentityLevel::HardwareIdAndStrings;
    else if (p.hasVendorProductId) p.identity = IdentityLevel::HardwareId;

    // HID Usage Tables provide generic input-function classification, not vendor feature semantics.
    if (in.usagePage == 0x01 && in.usage == 0x06) p.function = Function::Keyboard;
    else if (in.usagePage == 0x01 && in.usage == 0x02) p.function = Function::Mouse;
    else if (in.usagePage == 0x0C) p.function = Function::ConsumerControl;
    else if (in.usagePage == 0x01 && (in.usage == 0x04 || in.usage == 0x05)) p.function = Function::GameController;

    if (in.inputReportBytes) add(p, Observation::InputReports);
    if (in.outputReportBytes) add(p, Observation::OutputReportChannel);
    if (in.featureReportBytes) add(p, Observation::FeatureReportChannel);
    if (in.usagePage == 0x01 && in.usage == 0x06) add(p, Observation::StandardKeyboardUsage);
    if (in.usagePage == 0x01 && in.usage == 0x02) add(p, Observation::StandardMouseUsage);
    if (in.usagePage == 0x0C) add(p, Observation::ConsumerUsage);
    if (in.usagePage == 0x01 && (in.usage == 0x04 || in.usage == 0x05)) add(p, Observation::GameControllerUsage);

    // A report channel is not a vendor protocol. Unknown devices stay observation-only.
    p.configurationProtocolKnown = false;
    p.hardwareWritesAllowed = false;
    p.status = p.function == Function::Unknown
        ? L"Detected through an exposed Windows interface; function not classified. Read-only observation."
        : L"Standard HID function classified. Report channels are observations only; no configuration protocol is inferred.";
    return p;
}
const wchar_t* Classifier::transportName(Transport v) {
    switch(v) { case Transport::UsbHid:return L"USB_HID"; case Transport::BluetoothHid:return L"BLUETOOTH_HID"; case Transport::BlePnP:return L"BLE_PNP"; case Transport::UsbPnP:return L"USB_PNP"; case Transport::UsbVendorSpecific:return L"USB_VENDOR_SPECIFIC"; case Transport::OtherPnP:return L"OTHER_PNP"; default:return L"UNKNOWN"; }
}
const wchar_t* Classifier::functionName(Function v) {
    switch(v) { case Function::Keyboard:return L"KEYBOARD"; case Function::Mouse:return L"MOUSE"; case Function::ConsumerControl:return L"CONSUMER_CONTROL"; case Function::GameController:return L"GAME_CONTROLLER"; case Function::Composite:return L"COMPOSITE"; default:return L"UNKNOWN"; }
}
const wchar_t* Classifier::identityName(IdentityLevel v) {
    switch(v) { case IdentityLevel::HardwareId:return L"HARDWARE_ID"; case IdentityLevel::HardwareIdAndStrings:return L"HARDWARE_ID_AND_STRINGS"; case IdentityLevel::CatalogExactMatch:return L"CATALOG_EXACT_MATCH"; default:return L"UNKNOWN"; }
}
} // namespace unify::universal
