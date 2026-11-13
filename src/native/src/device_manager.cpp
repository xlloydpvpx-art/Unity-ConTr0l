#include "unify/device_scanner.hpp"
#include "unify/universal_discovery.hpp"
#include "unify/evidence_engine.hpp"
#include "unify/diagnostic_engine.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace unify {
namespace {
std::wstring jsonEscape(const std::wstring& s) {
    std::wostringstream o;
    for (wchar_t c : s) {
        switch (c) {
        case L'"': o << L"\\\""; break;
        case L'\\': o << L"\\\\"; break;
        case L'\n': o << L"\\n"; break;
        case L'\r': o << L"\\r"; break;
        case L'\t': o << L"\\t"; break;
        default: if (c < 0x20) o << L' '; else o << c;
        }
    }
    return o.str();
}
std::wstring hex4(unsigned value) {
    std::wostringstream o; o << std::uppercase << std::hex << std::setw(4) << std::setfill(L'0') << value; return o.str();
}
}
std::vector<DeviceGroup> DeviceScanner::group(const std::vector<DeviceInfo>& devices) {
    std::vector<DeviceGroup> groups;
    std::unordered_map<std::wstring, std::size_t> index;
    for (const auto& d : devices) {
        // Container ID is the strongest PnP grouping key. Without one, keep each interface distinct.
        const std::wstring fallbackId = !d.path.empty() ? d.path : (!d.usbInterfacePath.empty() ? d.usbInterfacePath : d.instanceId);
        const std::wstring key = d.containerId.empty() ? (L"interface:" + fallbackId) : (L"container:" + d.containerId);
        auto it = index.find(key);
        if (it == index.end()) {
            DeviceGroup g; g.id = key; g.containerId = d.containerId;
            g.displayName = !d.product.empty() ? d.product : (!d.manufacturer.empty() ? d.manufacturer : L"Unknown device");
            g.receiverHint = d.receiverHint;
            index.emplace(key, groups.size()); groups.push_back(std::move(g)); it = index.find(key);
        }
        auto& g = groups[it->second];
        if (g.receiverHint == L"UNKNOWN" && d.receiverHint != L"UNKNOWN") g.receiverHint = d.receiverHint;
        g.interfaces.push_back(d);
    }
    return groups;
}
std::wstring DeviceScanner::toJson(const std::vector<DeviceInfo>& devices, const std::wstring& observationsPath, std::size_t manufacturerCount, std::size_t verifiedDeviceCount, const std::wstring& databaseStatus) {
    const auto groups = group(devices);
    std::wostringstream o;
    o << L"{\"type\":\"devices\",\"deviceCount\":" << devices.size() << L",\"groupCount\":" << groups.size() << L",\"database\":{\"status\":\"" << jsonEscape(databaseStatus) << L"\",\"manufacturerCount\":" << manufacturerCount << L",\"verifiedDeviceCount\":" << verifiedDeviceCount << L"},\"devices\":[";
    for (std::size_t i=0;i<devices.size();++i) {
        const auto& d=devices[i]; if(i) o << L',';
        universal::Input universalInput;
        if (d.connection.find(L"Bluetooth") != std::wstring::npos)
            universalInput.transport = d.instanceId.find(L"BTHLE") != std::wstring::npos ? universal::Transport::BlePnP : universal::Transport::BluetoothHid;
        else if (d.connection.find(L"USB (PnP metadata only)") != std::wstring::npos || d.connection.find(L"USB device interface") != std::wstring::npos)
            universalInput.transport = universal::Transport::UsbPnP;
        else if (d.connection.find(L"USB") != std::wstring::npos)
            universalInput.transport = universal::Transport::UsbHid;
        else universalInput.transport = universal::Transport::Unknown;
        universalInput.vid=d.vid; universalInput.pid=d.pid; universalInput.revision=d.version; universalInput.revisionKnown=d.version!=0;
        universalInput.usagePage=d.usagePage; universalInput.usage=d.usage;
        universalInput.hasManufacturerString=!d.manufacturer.empty(); universalInput.hasProductString=!d.product.empty();
        universalInput.catalogExactMatch=d.identificationStatus==L"EXACT MODEL MATCH";
        universalInput.inputReportBytes=d.inputReportBytes; universalInput.outputReportBytes=d.outputReportBytes; universalInput.featureReportBytes=d.featureReportBytes;
        const auto universalProfile=universal::Classifier::classify(universalInput);
        std::vector<evidence::Item> evidenceItems;
        if(d.vid && d.pid) evidenceItems.push_back({evidence::Type::HardwareId,L"VID_"+hex4(d.vid)+L"&PID_"+hex4(d.pid),L"Windows PnP/HID",L"Observed hardware identifier",false});
        if(d.version) evidenceItems.push_back({evidence::Type::Revision,hex4(d.version),L"Windows PnP/HID",L"Observed hardware revision",false});
        if(!d.manufacturer.empty()) evidenceItems.push_back({evidence::Type::ManufacturerString,d.manufacturer,L"Windows PnP/HID string",L"Observed manufacturer string",false});
        if(!d.product.empty()) evidenceItems.push_back({evidence::Type::ProductString,d.product,L"Windows PnP/HID string",L"Observed product string",false});
        if(!d.serial.empty()) evidenceItems.push_back({evidence::Type::SerialString,d.serial,L"HID serial string",L"Observed serial string",false});
        if(!d.containerId.empty()) evidenceItems.push_back({evidence::Type::ContainerId,d.containerId,L"Windows PnP",L"Observed container grouping",false});
        if(!d.parentInstanceId.empty()) evidenceItems.push_back({evidence::Type::ParentId,d.parentInstanceId,L"Windows PnP",L"Observed parent relation",false});
        if(d.hidDescriptorReadable) evidenceItems.push_back({evidence::Type::HidDescriptor,L"WINDOWS_PREPARSED_CAPABILITIES",L"Windows HID preparsed data",L"Interpreted capabilities; not raw descriptor bytes",false});
        if(d.hidDescriptorReadable && d.usagePage) evidenceItems.push_back({evidence::Type::ParsedUsage,std::to_wstring(d.usagePage)+L":"+std::to_wstring(d.usage),L"Windows HID preparsed data",L"Observed top-level usage",false});
        if(d.identificationStatus==L"EXACT MODEL MATCH" && !d.identificationEvidence.empty()) evidenceItems.push_back({evidence::Type::TrustedCatalog,d.identifiedModel,d.identificationEvidence,L"Exact local catalog record",true});
        if(d.identificationStatus.find(L"AMBIGUOUS")!=std::wstring::npos) evidenceItems.push_back({evidence::Type::IdentityConflict,d.identificationStatus,L"Local device database",L"The database returned an ambiguous identity result.",false});
        const auto evidenceAssessment=evidence::Engine::assess(evidenceItems);
        diagnostics::Facts diagnosticFacts; diagnosticFacts.hidInterface=!d.path.empty(); diagnosticFacts.descriptorReadable=d.hidDescriptorReadable;
        diagnosticFacts.descriptorValid=d.hidDescriptorReadable; diagnosticFacts.usageRecognized=(d.usagePage==0x01 && (d.usage==0x02 || d.usage==0x06 || d.usage==0x04 || d.usage==0x05)) || d.usagePage==0x0C;
        diagnosticFacts.reportInput=d.inputReportBytes!=0; diagnosticFacts.reportOutput=d.outputReportBytes!=0; diagnosticFacts.reportFeature=d.featureReportBytes!=0;
        diagnosticFacts.identityConflict=d.identificationStatus.find(L"AMBIGUOUS")!=std::wstring::npos;
        const auto diagnosticFindings=diagnostics::Engine::analyze(diagnosticFacts);
        o << L"{\"evidenceAssessment\":{\"score\":" << evidenceAssessment.score << L",\"identityLevel\":\"" << evidenceAssessment.identityLevel
          << L"\",\"confidence\":\"" << evidenceAssessment.confidence << L"\",\"state\":\"" << evidenceAssessment.state << L"\",\"conflicts\":[";
        for(std::size_t e=0;e<evidenceAssessment.conflicts.size();++e) { if(e) o << L','; o << L"\"" << jsonEscape(evidenceAssessment.conflicts[e]) << L"\""; }
        o << L"]},\"diagnostics\":[";
        for(std::size_t f=0;f<diagnosticFindings.size();++f) { if(f) o << L','; const auto& finding=diagnosticFindings[f];
            o << L"{\"severity\":\"" << diagnostics::severityName(finding.severity) << L"\",\"code\":\"" << jsonEscape(finding.code)
              << L"\",\"title\":\"" << jsonEscape(finding.title) << L"\",\"detail\":\"" << jsonEscape(finding.detail)
              << L"\",\"recommendation\":\"" << jsonEscape(finding.recommendation) << L"\"}";
        }
        o << L"] ,\"universalProfile\":{\"transport\":\"" << universal::Classifier::transportName(universalProfile.transport)
          << L"\",\"function\":\"" << universal::Classifier::functionName(universalProfile.function)
          << L"\",\"identity\":\"" << universal::Classifier::identityName(universalProfile.identity)
          << L"\",\"configurationProtocolKnown\":false,\"hardwareWritesAllowed\":false,\"status\":\"" << jsonEscape(universalProfile.status) << L"\"},\"path\":\"" << jsonEscape(d.path) << L"\",\"usbInterfacePath\":\"" << jsonEscape(d.usbInterfacePath) << L"\",\"hidDescriptorAnalysis\":\"" << jsonEscape(d.hidDescriptorAnalysis) << L"\",\"instanceId\":\"" << jsonEscape(d.instanceId)
          << L"\",\"containerId\":\"" << jsonEscape(d.containerId) << L"\",\"parentInstanceId\":\"" << jsonEscape(d.parentInstanceId)
          << L"\",\"location\":\"" << jsonEscape(d.location) << L"\",\"manufacturer\":\"" << jsonEscape(d.manufacturer)
          << L"\",\"product\":\"" << jsonEscape(d.product) << L"\",\"serial\":\"" << jsonEscape(d.serial)
          << L"\",\"vid\":\"" << hex4(d.vid) << L"\",\"pid\":\"" << hex4(d.pid) << L"\",\"version\":\"" << hex4(d.version)
          << L"\",\"usagePage\":" << d.usagePage << L",\"usage\":" << d.usage << L",\"interfaceNumber\":" << (d.interfaceNumber==0xffff ? -1 : d.interfaceNumber)
          << L",\"deviceType\":\"" << jsonEscape(d.type) << L"\",\"hidDescriptorReadable\":" << (d.hidDescriptorReadable?L"true":L"false")
          << L",\"reportLengths\":{\"input\":" << d.inputReportBytes << L",\"output\":" << d.outputReportBytes << L",\"feature\":" << d.featureReportBytes
          << L"},\"reportCapabilities\":{\"inputButtons\":" << d.inputButtonCaps << L",\"outputButtons\":" << d.outputButtonCaps << L",\"featureButtons\":" << d.featureButtonCaps
          << L",\"inputValues\":" << d.inputValueCaps << L",\"outputValues\":" << d.outputValueCaps << L",\"featureValues\":" << d.featureValueCaps << L"},\"reportIds\":[";
        for(std::size_t r=0;r<d.hidReportIds.size();++r) { if(r) o << L','; o << L"\"" << jsonEscape(d.hidReportIds[r]) << L"\""; }
        o << L"],\"usageDetails\":[";
        for(std::size_t u=0;u<d.hidUsageDetails.size();++u) { if(u) o << L','; o << L"\"" << jsonEscape(d.hidUsageDetails[u]) << L"\""; }
        o << L"],\"winUsbStatus\":\"" << jsonEscape(d.winUsbStatus) << L"\",\"usbEndpoints\":[";
        for(std::size_t e=0;e<d.usbEndpoints.size();++e) { if(e) o << L','; const auto& ep=d.usbEndpoints[e];
            o << L"{\"interface\":" << static_cast<unsigned>(ep.interfaceNumber) << L",\"alternateSetting\":" << static_cast<unsigned>(ep.alternateSetting)
              << L",\"address\":" << static_cast<unsigned>(ep.address) << L",\"transferType\":\"" << transport::usbTransferType(ep.attributes)
              << L"\",\"maxPacketSize\":" << ep.maxPacketSize << L",\"interval\":" << static_cast<unsigned>(ep.interval) << L",\"pipeId\":" << static_cast<unsigned>(ep.pipeId) << L"}";
        }
        o << L"],\"gattStatus\":\"" << jsonEscape(d.gattStatus) << L"\",\"gattServices\":[";
        for(std::size_t g=0;g<d.gattServices.size();++g) { if(g) o << L','; const auto& service=d.gattServices[g];
            o << L"{\"uuid\":\"" << jsonEscape(service.uuid) << L"\",\"characteristics\":[";
            for(std::size_t c=0;c<service.characteristics.size();++c) { if(c) o << L','; const auto& ch=service.characteristics[c];
                o << L"{\"uuid\":\"" << jsonEscape(ch.uuid) << L"\",\"properties\":" << ch.properties << L"}";
            }
            o << L"]}";
        }
        o << L"],\"connection\":\"" << jsonEscape(d.connection)
          << L"\",\"receiverHint\":\"" << jsonEscape(d.receiverHint) << L"\",\"identifiedManufacturer\":\"" << jsonEscape(d.identifiedManufacturer) << L"\",\"identifiedModel\":\"" << jsonEscape(d.identifiedModel) << L"\",\"identificationStatus\":\"" << jsonEscape(d.identificationStatus) << L"\",\"identificationEvidence\":\"" << jsonEscape(d.identificationEvidence) << L"\",\"capabilities\":[";
        for (std::size_t c=0;c<d.capabilitySummary.size();++c) { if(c) o << L','; o << L"\"" << jsonEscape(d.capabilitySummary[c]) << L"\""; }
        o << L"],\"compatibility\":\"DETECTED ONLY\"}";
    }
    o << L"],\"groups\":[";
    for (std::size_t i=0;i<groups.size();++i) {
        const auto& g=groups[i]; if(i) o << L',';
        o << L"{\"id\":\"" << jsonEscape(g.id) << L"\",\"containerId\":\"" << jsonEscape(g.containerId)
          << L"\",\"displayName\":\"" << jsonEscape(g.displayName) << L"\",\"interfaceCount\":" << g.interfaces.size()
          << L",\"receiverHint\":\"" << jsonEscape(g.receiverHint) << L"\",\"compatibility\":\"DETECTED ONLY\"}";
    }
    o << L"],\"observationsPath\":\"" << jsonEscape(observationsPath) << L"\"}";
    return o.str();
}
}
