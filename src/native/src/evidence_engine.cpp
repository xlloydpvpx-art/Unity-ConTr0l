#include "unify/evidence_engine.hpp"
#include <map>
#include <set>
namespace unify::evidence {
const wchar_t* typeName(Type t) { switch(t) {case Type::HardwareId:return L"HARDWARE_ID";case Type::Revision:return L"REVISION";case Type::ManufacturerString:return L"MANUFACTURER_STRING";case Type::ProductString:return L"PRODUCT_STRING";case Type::SerialString:return L"SERIAL_STRING";case Type::ContainerId:return L"CONTAINER_ID";case Type::ParentId:return L"PARENT_ID";case Type::HidDescriptor:return L"HID_DESCRIPTOR";case Type::ParsedUsage:return L"PARSED_USAGE";case Type::TrustedCatalog:return L"TRUSTED_CATALOG";case Type::VendorDocumentation:return L"VENDOR_DOCUMENTATION";case Type::PhysicalTest:return L"PHYSICAL_TEST";case Type::IdentityConflict:return L"IDENTITY_CONFLICT";} return L"UNKNOWN"; }
Assessment Engine::assess(const std::vector<Item>& items) {
    Assessment a; std::map<Type,std::set<std::wstring>> values; std::set<Type> verified;
    for(const auto& e:items) {
        if(e.value.empty()) continue;
        values[e.type].insert(e.value);
        if(e.independentlyVerified && !e.source.empty()) verified.insert(e.type);
        if(e.type==Type::IdentityConflict) a.conflicts.push_back(e.note.empty()?L"Identity database reported an ambiguous match.":e.note);
    }
    for(const auto& [type, vals]:values) if(type!=Type::IdentityConflict && vals.size()>1) a.conflicts.push_back(std::wstring(typeName(type))+L" has conflicting values");
    const auto has=[&](Type t){return values.contains(t)&&!values[t].empty();};
    // Scores are evidence quality indicators, not compatibility ratings.
    if(has(Type::HardwareId)) a.score+=25;
    if(has(Type::Revision)) a.score+=5;
    if(has(Type::ManufacturerString)) a.score+=5;
    if(has(Type::ProductString)) a.score+=5;
    if(has(Type::SerialString)) a.score+=2;
    if(has(Type::ContainerId)) a.score+=3;
    if(has(Type::ParentId)) a.score+=3;
    if(has(Type::HidDescriptor)) a.score+=15;
    if(has(Type::ParsedUsage)) a.score+=7;
    if(verified.contains(Type::TrustedCatalog)) a.score+=15;
    if(verified.contains(Type::VendorDocumentation)) a.score+=10;
    if(verified.contains(Type::PhysicalTest)) a.score+=5;
    if(!a.conflicts.empty()) { a.identityLevel=L"CONFLICTED"; a.confidence=L"CONFLICTED"; a.state=L"MANUAL_REVIEW_REQUIRED"; }
    else if(verified.contains(Type::TrustedCatalog) && has(Type::HardwareId)) { a.identityLevel=L"CATALOG_CORROBORATED"; a.confidence=L"HIGH"; a.state=L"IDENTIFIED_WITH_EVIDENCE"; }
    else if(has(Type::HardwareId) && has(Type::ManufacturerString) && has(Type::ProductString)) { a.identityLevel=L"HARDWARE_ID_AND_STRINGS"; a.confidence=L"MODERATE"; a.state=L"IDENTIFIED_PARTIALLY"; }
    else if(has(Type::HardwareId)) { a.identityLevel=L"HARDWARE_ID"; a.confidence=L"LIMITED"; a.state=L"IDENTIFIED_PARTIALLY"; }
    else { a.identityLevel=L"UNKNOWN"; a.confidence=L"INSUFFICIENT"; a.state=L"DETECTED_ONLY"; }
    return a;
}
} // namespace unify::evidence
