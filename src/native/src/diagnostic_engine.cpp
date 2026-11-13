#include "unify/diagnostic_engine.hpp"
namespace unify::diagnostics {
const wchar_t* severityName(Severity s) { switch(s) {case Severity::Info:return L"INFO";case Severity::Warning:return L"WARNING";case Severity::Error:return L"ERROR";} return L"UNKNOWN"; }
std::vector<Finding> Engine::analyze(const Facts& f) {
    std::vector<Finding> out;
    auto add=[&](Severity s,const wchar_t* c,const wchar_t* t,const wchar_t* d,const wchar_t* r){out.push_back({s,c,t,d,r});};
    if(!f.present) { add(Severity::Info,L"DEVICE_NOT_PRESENT",L"Device disconnected",L"The device is no longer present in the current scan.",L"Reconnect the device and scan again."); return out; }
    if(!f.hidInterface) add(Severity::Info,L"NO_HID_INTERFACE",L"No HID interface exposed",L"Windows exposes this device through PnP, but no HID collection was observed.",L"Only metadata is available; vendor interface transport is not opened automatically.");
    else if(!f.descriptorReadable) add(Severity::Warning,L"HID_DESCRIPTOR_UNREADABLE",L"HID descriptor metadata unavailable",L"The HID interface exists, but Windows did not provide readable preparsed descriptor data.",L"Check permissions, driver state, and whether another application owns the device.");
    else if(!f.descriptorValid) add(Severity::Error,L"HID_DESCRIPTOR_INVALID",L"HID descriptor validation failed",L"The supplied descriptor did not pass structural validation.",L"Do not send reports; capture the descriptor again and inspect its bounds.");
    else if(!f.usageRecognized) add(Severity::Warning,L"HID_USAGE_UNCLASSIFIED",L"HID usage not classified",L"The descriptor is readable but does not match a known generic usage classification.",L"Keep the device detected-only and add evidence before classification.");
    if(f.descriptorReadable && f.descriptorValid && !f.reportInput && !f.reportOutput && !f.reportFeature) add(Severity::Warning,L"NO_REPORT_FIELDS",L"No report fields observed",L"The descriptor did not expose Input, Output, or Feature fields.",L"Check descriptor capture and device interface selection.");
    if(f.identityConflict) add(Severity::Error,L"IDENTITY_CONFLICT",L"Identity evidence conflicts",L"Two or more evidence sources disagree.",L"Require manual review; do not bind a model adapter.");
    if(f.adapterMatched && !f.protocolVerified) add(Severity::Warning,L"ADAPTER_PROTOCOL_UNVERIFIED",L"Adapter protocol is not verified",L"An adapter candidate exists but its protocol evidence is insufficient.",L"Keep all hardware operations disabled.");
    if(!f.adapterMatched) add(Severity::Info,L"NO_ADAPTER",L"No compatible adapter",L"Detection does not imply configuration support.",L"Use safe diagnostics and submit evidence through the adapter contribution process.");
    if(f.protocolVerified && f.adapterMatched) add(Severity::Info,L"ADAPTER_EVIDENCE_PRESENT",L"Verified adapter evidence exists",L"This finding is not a statement that write operations are enabled in this build.",L"Review adapter trust, firmware scope, and runtime policy separately.");
    if(out.empty()) add(Severity::Info,L"NO_ISSUES",L"No diagnostic findings",L"No issues were detected by the current read-only checks.",L"This does not establish hardware configuration compatibility.");
    return out;
}
} // namespace unify::diagnostics
