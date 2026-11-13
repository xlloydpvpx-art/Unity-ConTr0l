// Asserts must stay active in Release builds, otherwise the tests pass vacuously.
#undef NDEBUG
#include "unify/device_scanner.hpp"
#include "unify/device_database.hpp"
#include "unify/protocol_research.hpp"
#include "unify/adapter_registry.hpp"
#include "unify/capability_engine.hpp"
#include "unify/adapter_sdk.hpp"
#include "unify/universal_discovery.hpp"
#include "unify/hid_report_parser.hpp"
#include "unify/transport_registry.hpp"
#include "unify/evidence_engine.hpp"
#include "unify/diagnostic_engine.hpp"
#include "unify/usb_descriptor_parser.hpp"
#include "unify/transport_metadata.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
#include <stdexcept>
int main() {
    using namespace unify;
    DeviceInfo a; a.path=L"hid#one"; a.containerId=L"{ABC}"; a.product=L"Combo Keyboard"; a.vid=0x1234; a.pid=0x5678; a.type=L"Keyboard";
    DeviceInfo b=a; b.path=L"hid#two"; b.type=L"Mouse"; b.interfaceNumber=1;
    DeviceInfo c; c.path=L"hid#three"; c.product=L"Standalone";
    const auto groups=DeviceScanner::group({a,b,c});
    assert(groups.size()==2);
    assert(groups[0].interfaces.size()==2);
    assert(groups[1].interfaces.size()==1);
    DeviceInfo usbPathA; usbPathA.usbInterfacePath=L"\\\\?\\usb#interface-a";
    DeviceInfo usbPathB; usbPathB.usbInterfacePath=L"\\\\?\\usb#interface-b";
    assert(DeviceScanner::group({usbPathA,usbPathB}).size()==2);
    DeviceInfo bleA; bleA.instanceId=L"BTHLE\\DEV_A"; bleA.product=L"BLE peripheral"; bleA.connection=L"Bluetooth (PnP metadata only)";
    DeviceInfo bleB=bleA; bleB.instanceId=L"BTHLE\\DEV_B";
    const auto bluetoothGroups=DeviceScanner::group({bleA,bleB});
    assert(bluetoothGroups.size()==2); // Empty HID paths must not collapse unrelated Bluetooth devnodes.
    const auto bluetoothJson=DeviceScanner::toJson({bleA,bleB});
    assert(bluetoothJson.find(L"Bluetooth (PnP metadata only)")!=std::wstring::npos);
    const auto json=DeviceScanner::toJson({a,b,c});
    assert(json.find(L"\"groupCount\":2")!=std::wstring::npos);
    assert(json.find(L"DETECTED ONLY")!=std::wstring::npos);
    assert(json.find(L"reportLengths")!=std::wstring::npos);
    assert(json.find(L"hidDescriptorReadable")!=std::wstring::npos);
    assert(json.find(L"evidenceAssessment")!=std::wstring::npos);
    assert(json.find(L"diagnostics")!=std::wstring::npos);
    assert(json.find(L"reportIds")!=std::wstring::npos);
    assert(json.find(L"usageDetails")!=std::wstring::npos);
    DeviceInfo escaped=a; escaped.product=L"Quote \" test";
    const auto escapedJson=DeviceScanner::toJson({escaped});
    assert(escapedJson.find(L"Quote \\\" test")!=std::wstring::npos);
    // Load a controlled fixture to prove exact model matching and manufacturer-only fallback.
    const auto fixture = std::filesystem::temp_directory_path() / "unify_device_database_test.json";
    {
        std::ofstream f(fixture);
        f << R"({"schemaVersion":3,"manufacturers":[{"name":"MCHOSE","aliases":["MCHOSE Gaming","迈从"],"status":"CATALOG_ONLY"}],"devices":[{"vid":4660,"pid":22136,"revision":256,"manufacturer":"VerifiedCo","model":"Test Pro","evidence":"Vendor documentation","status":"VERIFIED"}]})";
    }
    DeviceDatabase db;
    assert(db.load(fixture));
    assert(db.manufacturerCount()==1 && db.verifiedDeviceCount()==1);
    DeviceInfo exact; exact.vid=0x1234; exact.pid=0x5678; exact.version=0x0100;
    db.identify(exact);
    assert(exact.identificationStatus==L"EXACT MODEL MATCH");
    assert(exact.identifiedModel==L"Test Pro");
    DeviceInfo alias; alias.manufacturer=L"MCHOSE Gaming";
    db.identify(alias);
    assert(alias.identificationStatus==L"MANUFACTURER ALIAS MATCH");
    assert(alias.identifiedManufacturer==L"MCHOSE");
    DeviceInfo chineseAlias; chineseAlias.manufacturer=L"迈从";
    db.identify(chineseAlias);
    assert(chineseAlias.identificationStatus==L"MANUFACTURER ALIAS MATCH");
    DeviceInfo wrongRevision; wrongRevision.vid=0x1234; wrongRevision.pid=0x5678; wrongRevision.version=0x0200;
    db.identify(wrongRevision);
    assert(wrongRevision.identificationStatus==L"UNMATCHED VID/PID");
    std::filesystem::remove(fixture);
    const auto capabilityFixture = std::filesystem::temp_directory_path() / "unify_capability_engine_test.json";
    { std::ofstream f(capabilityFixture); f << R"({"schemaVersion":3,"manufacturers":[],"devices":[{"vid":4660,"pid":22136,"manufacturer":"VerifiedCo","model":"Capability Test","evidence":"Vendor protocol document","status":"VERIFIED","capabilities":[{"name":"DPI","operation":"WRITE","status":"VERIFIED","evidence":"Model-specific protocol document"},{"name":"RGB","operation":"WRITE","status":"VERIFIED","evidence":"Model-specific protocol document","implementation":"vendor_rgb_v1"},{"name":"MACROS","operation":"WRITE","status":"VERIFIED","evidence":""},{"name":"UNKNOWN_COMMAND","operation":"WRITE","status":"VERIFIED","evidence":"Untrusted"}]}]})"; }
    DeviceDatabase capabilityDb; assert(capabilityDb.load(capabilityFixture));
    DeviceInfo capabilityDevice; capabilityDevice.vid=0x1234; capabilityDevice.pid=0x5678; capabilityDb.identify(capabilityDevice);
    assert(capabilityDevice.identificationStatus==L"EXACT MODEL MATCH");
    assert(capabilityDevice.capabilitySummary.size()==2);
    assert(capabilityDevice.capabilitySummary[0].find(L"VERIFIED_NOT_IMPLEMENTED")!=std::wstring::npos);
    assert(capabilityDevice.capabilitySummary[1].find(L"IMPLEMENTATION_NOT_REGISTERED")!=std::wstring::npos);
    std::filesystem::remove(capabilityFixture);
    const auto ambiguousFixture = std::filesystem::temp_directory_path() / "unify_device_database_ambiguous_test.json";
    {
        std::ofstream f(ambiguousFixture);
        f << R"({"schemaVersion":3,"manufacturers":[],"devices":[{"vid":4660,"pid":22136,"revision":256,"manufacturer":"A","model":"Model A","evidence":"Evidence A","status":"VERIFIED"},{"vid":4660,"pid":22136,"revision":256,"manufacturer":"B","model":"Model B","evidence":"Evidence B","status":"VERIFIED"}]})";
    }
    DeviceDatabase ambiguousDb; assert(ambiguousDb.load(ambiguousFixture));
    assert(ambiguousDb.conflictingModelKeys() == 1);
    DeviceInfo ambiguous; ambiguous.vid=0x1234; ambiguous.pid=0x5678; ambiguous.version=0x0100;
    ambiguousDb.identify(ambiguous);
    assert(ambiguous.identificationStatus==L"AMBIGUOUS DATABASE MATCH");
    assert(ambiguous.identifiedModel.empty());
    std::filesystem::remove(ambiguousFixture);
    const auto invalidFixture = std::filesystem::temp_directory_path() / "unify_device_database_invalid_records_test.json";
    {
        std::ofstream f(invalidFixture);
        f << R"({"schemaVersion":3,"manufacturers":[],"devices":[{"vid":70000,"pid":1,"manufacturer":"X","model":"Out of range","evidence":"Source","status":"VERIFIED"},{"vid":1,"pid":2,"manufacturer":"X","model":"No evidence","evidence":"","status":"VERIFIED"}]})";
    }
    DeviceDatabase invalidDb; assert(invalidDb.load(invalidFixture));
    assert(invalidDb.verifiedDeviceCount() == 0);
    assert(invalidDb.rejectedVerifiedRecords() == 2);
    assert(invalidDb.loadStatus() == L"LOADED WITH WARNINGS");
    std::filesystem::remove(invalidFixture);
    const auto aliasConflictFixture = std::filesystem::temp_directory_path() / "unify_device_database_alias_conflict_test.json";
    {
        std::ofstream f(aliasConflictFixture);
        f << R"({"schemaVersion":3,"manufacturers":[{"name":"Vendor A","aliases":["Shared OEM"],"status":"CATALOG_ONLY"},{"name":"Vendor B","aliases":["Shared OEM"],"status":"CATALOG_ONLY"}],"devices":[]})";
    }
    DeviceDatabase aliasDb; assert(aliasDb.load(aliasConflictFixture));
    DeviceInfo aliasConflict; aliasConflict.manufacturer=L"Shared OEM"; aliasDb.identify(aliasConflict);
    assert(aliasConflict.identificationStatus == L"AMBIGUOUS MANUFACTURER MATCH");
    assert(aliasConflict.identifiedManufacturer.empty());
    std::filesystem::remove(aliasConflictFixture);
    const auto unsupportedSchema = std::filesystem::temp_directory_path() / "unify_device_database_schema_test.json";
    { std::ofstream f(unsupportedSchema); f << R"({"schemaVersion":99,"manufacturers":[],"devices":[]})"; }
    DeviceDatabase schemaDb; assert(!schemaDb.load(unsupportedSchema));
    assert(schemaDb.loadStatus() == L"INVALID OR UNSUPPORTED SCHEMA");
    std::filesystem::remove(unsupportedSchema);
    DeviceDatabase productionDb;
    assert(productionDb.load(std::filesystem::path(UNIFY_SOURCE_DIR) / "database" / "devices.json"));
    assert(productionDb.manufacturerCount() >= 50);
    assert(productionDb.verifiedDeviceCount() == 0); // no VID/PID claims without evidence
    DeviceInfo localized; localized.manufacturer=L"雷柏"; productionDb.identify(localized);
    assert(localized.identifiedManufacturer==L"Rapoo");
    assert(localized.identificationStatus==L"MANUFACTURER ALIAS MATCH");
    ProtocolResearchCenter research;
    assert(research.load(std::filesystem::path(UNIFY_SOURCE_DIR) / "database" / "protocol_research.json"));
    assert(research.itemCount() >= 8);
    assert(research.verifiedCount() == 0); // targets are not compatibility claims
    const auto researchJson = research.toJson();
    assert(researchJson.find(L"RESEARCH_REQUIRED") != std::wstring::npos);
    assert(researchJson.find(L"https://") != std::wstring::npos);
    assert(researchJson.find(L"\"id\":\"logitech-gaming-hid\"") != std::wstring::npos);
    const auto invalidResearch = std::filesystem::temp_directory_path() / "unify_protocol_research_invalid.json";
    { std::ofstream f(invalidResearch); f << R"({"schemaVersion":1,"researchItems":[{"id":"bad","manufacturer":"X","status":"VERIFIED","evidenceLevel":"","sources":[]},{"id":"good","manufacturer":"Y","status":"RESEARCH_REQUIRED","evidenceLevel":"NONE","sources":[]}]})"; }
    ProtocolResearchCenter guarded; assert(guarded.load(invalidResearch));
    assert(guarded.itemCount() == 1 && guarded.verifiedCount() == 0);
    std::filesystem::remove(invalidResearch);
    // Exact-model adapter matching is fail-closed: matches never execute commands.
    AdapterRegistry adapters; AdapterDescriptor adapter;
    adapter.id=L"sample-vendor-model-a"; adapter.manufacturer=L"Sample Vendor"; adapter.model=L"Model A";
    adapter.vid=0x1234; adapter.pid=0x5678; adapter.revision=0x0100; adapter.revisionSpecified=true;
    adapter.transport=AdapterTransport::HidFeatureReport; adapter.capabilities={L"DPI"}; adapter.evidence=L"Protocol document and hardware capture";
    assert(adapters.add(adapter)); assert(!adapters.add(adapter));
    auto hit=adapters.match(0x1234,0x5678,0x0100,L"Model A",L"DPI"); assert(hit.state==L"ADAPTER_MATCHED_DISABLED"&&!hit.commandsEnabled);
    assert(adapters.match(0x1234,0x5678,0x0100,L"Model B",L"DPI").state==L"NO_EXACT_ADAPTER");
    assert(adapters.match(0x1234,0x5678,0x0200,L"Model A",L"DPI").state==L"NO_EXACT_ADAPTER");
    assert(adapters.match(0x1234,0x5678,0x0100,L"Model A",L"RGB").state==L"NO_EXACT_ADAPTER");
    AdapterDescriptor winusbAdapter=adapter; winusbAdapter.id=L"sample-winusb-adapter"; winusbAdapter.transport=AdapterTransport::UsbWinUsb;
    assert(adapters.add(winusbAdapter));
    AdapterDescriptor bleAdapter=adapter; bleAdapter.id=L"sample-ble-adapter"; bleAdapter.transport=AdapterTransport::BleGatt;
    assert(adapters.add(bleAdapter));
    assert(adapters.match(0x1234,0x5678,0x0100,L"Model A",L"DPI").commandsEnabled==false);
    // Universal capability vocabulary is broad, but every entry still requires evidence and a concrete adapter.
    AdapterDescriptor hall=adapter; hall.id=L"hall-effect-keyboard"; hall.model=L"HE Keyboard"; hall.capabilities={L"RAPID_TRIGGER",L"ADJUSTABLE_ACTUATION",L"SOCD"};
    assert(adapters.add(hall));
    const auto capabilityCatalog = std::filesystem::path(UNIFY_SOURCE_DIR) / "database" / "capability_catalog.json";
    std::ifstream catalogStream(capabilityCatalog); std::string catalogText((std::istreambuf_iterator<char>(catalogStream)), std::istreambuf_iterator<char>());
    assert(catalogText.find("RAPID_TRIGGER") != std::string::npos);
    assert(catalogText.find("PER_KEY_RGB") != std::string::npos);
    assert(catalogText.find("POLLING_RATE") != std::string::npos);
    const auto broadCapabilities = CapabilityEngine::resolve({
        {L"RAPID_TRIGGER",L"WRITE",L"VERIFIED",L"Model protocol evidence",L""},
        {L"PER_KEY_RGB",L"WRITE",L"VERIFIED",L"Model protocol evidence",L""},
        {L"UNKNOWN_VENDOR_COMMAND",L"WRITE",L"VERIFIED",L"Untrusted",L""}
    });
    assert(broadCapabilities.size()==2);
    assert(!broadCapabilities[0].executable && !broadCapabilities[1].executable);
    AdapterDescriptor unsafe=adapter; unsafe.id=L"unsafe"; unsafe.capabilities={L"RAW_HID_WRITE"}; assert(!adapters.add(unsafe));
    AdapterDescriptor noEvidence=adapter; noEvidence.id=L"no-evidence"; noEvidence.evidence=L""; assert(!adapters.add(noEvidence));
    DeviceInfo descriptorSample; descriptorSample.inputReportBytes=8; descriptorSample.outputReportBytes=16; descriptorSample.featureReportBytes=32; descriptorSample.hidDescriptorReadable=true;
    const auto descriptorJson=DeviceScanner::toJson({descriptorSample});
    assert(descriptorJson.find(L"\"input\":8")!=std::wstring::npos);
    assert(descriptorJson.find(L"\"output\":16")!=std::wstring::npos);
    assert(descriptorJson.find(L"\"feature\":32")!=std::wstring::npos);
    // Robustness: malformed or hostile database files must never crash the loader or create VERIFIED models.
    const char* malformed[] = {
        "", "{", "}", "not json at all", "{\"schemaVersion\":3,\"devices\":[", "{\"schemaVersion\":3,\"devices\":[{\"vid\":1,\"pid\":2,\"model\":\"unterminated",
        "{\"schemaVersion\":3,\"devices\":[{{{{}}}}]}", "{\"schemaVersion\":3,\"manufacturers\":[{\"name\":\"A\",\"aliases\":[\"\\", "{\"schemaVersion\":99,\"devices\":[]}",
        "{\"schemaVersion\":3,\"devices\":[{\"vid\":-5,\"pid\":2,\"manufacturer\":\"X\",\"model\":\"Y\",\"evidence\":\"Z\",\"status\":\"VERIFIED\"}]}"
    };
    int fixtureIndex = 0;
    for (const char* text : malformed) {
        const auto badFile = std::filesystem::temp_directory_path() / ("unify_malformed_" + std::to_string(fixtureIndex++) + ".json");
        { std::ofstream f(badFile, std::ios::binary); f << text; }
        DeviceDatabase badDb;
        (void)badDb.load(badFile);
        assert(badDb.verifiedDeviceCount() == 0);
        DeviceInfo probe; probe.vid = 2; probe.pid = 2; badDb.identify(probe);
        assert(probe.identificationStatus != L"EXACT MODEL MATCH");
        std::filesystem::remove(badFile);
    }
    const auto missingFile = std::filesystem::temp_directory_path() / "unify_definitely_missing_database.json";
    DeviceDatabase missingDb; assert(!missingDb.load(missingFile)); assert(missingDb.verifiedDeviceCount() == 0);
    // An unknown revision (0) must not match a record that requires a specific revision.
    {
        const auto revFile = std::filesystem::temp_directory_path() / "unify_revision_required_test.json";
        { std::ofstream f(revFile); f << R"({"schemaVersion":3,"manufacturers":[],"devices":[{"vid":4660,"pid":22136,"revision":256,"manufacturer":"V","model":"M","evidence":"E","status":"VERIFIED"}]})"; }
        DeviceDatabase revDb; assert(revDb.load(revFile));
        DeviceInfo unknownRevision; unknownRevision.vid = 0x1234; unknownRevision.pid = 0x5678; unknownRevision.version = 0;
        revDb.identify(unknownRevision);
        assert(unknownRevision.identificationStatus != L"EXACT MODEL MATCH");
        std::filesystem::remove(revFile);
    }
    // Regression: MSVC std::regex throws error_stack on long string values; the loader must not use it for them.
    {
        const auto longFile = std::filesystem::temp_directory_path() / "unify_long_string_test.json";
        const std::string longText(5000, 'x');
        { std::ofstream f(longFile); f << "{\"schemaVersion\":3,\"manufacturers\":[{\"name\":\"LongCo\",\"aliases\":[\"" << longText << "\",\"Short\"],\"status\":\"CATALOG_ONLY\"}],\"devices\":[{\"vid\":4660,\"pid\":22136,\"manufacturer\":\"V\",\"model\":\"M\",\"evidence\":\"" << longText << "\",\"status\":\"VERIFIED\"}]}"; }
        DeviceDatabase longDb; assert(longDb.load(longFile));
        assert(longDb.verifiedDeviceCount() == 1 && longDb.manufacturerCount() == 1);
        std::filesystem::remove(longFile);
    }
    // Hostile device strings must stay valid JSON strings (control characters and quotes).
    DeviceInfo hostile; hostile.product = L"</script>\"\\\n\x01 x"; hostile.manufacturer = L"\t";
    const auto hostileJson = DeviceScanner::toJson({hostile});
    assert(hostileJson.find(L'\x01') == std::wstring::npos);
    assert(hostileJson.find(L"\n") == std::wstring::npos);

    // Adapter SDK v1: exact identity, bounded operations, explicit confirmation, trust gates,
    // read-before-write, readback verification and reversible rollback are tested with a mock.
    struct MockScalarTransport : sdk::IScalarTransport {
        std::int64_t current{800}; bool online{true}; bool corruptReadback{false}; int writes{0};
        bool read(const std::wstring&, std::int64_t& value, std::wstring&) override {
            if (!online) return false;
            value = corruptReadback && writes > 0 ? current + 1 : current;
            return true;
        }
        bool write(const std::wstring&, std::int64_t value, std::wstring&) override {
            if (!online) return false;
            current=value;
            ++writes;
            return true;
        }
        bool connected() const override { return online; }
        sdk::Transport transport() const override { return sdk::Transport::HidFeature; }
    };
    struct MockAuditSink final : sdk::IAuditSink {
        std::vector<sdk::AuditEvent> events; bool available{true};
        bool record(const sdk::AuditEvent& event) override {
            if (!available) return false;
            events.push_back(event);
            return true;
        }
    };
    MockAuditSink audit;
    sdk::AdapterManifest sdkManifest;
    sdkManifest.id=L"sample-dpi-adapter"; sdkManifest.version=L"1.0.0";
    sdkManifest.vendor=L"Sample Vendor"; sdkManifest.model=L"Model A";
    sdkManifest.evidenceUri=L"https://vendor.example/protocol";
    sdkManifest.vid=0x1234; sdkManifest.pid=0x5678; sdkManifest.revision=0x0100;
    sdkManifest.revisionSpecified=true; sdkManifest.minimumFirmware=L"1.0.0"; sdkManifest.maximumFirmware=L"2.0.0";
    sdkManifest.transport=sdk::Transport::HidFeature;
    sdkManifest.operations={{L"DPI",sdk::OperationKind::Read,100,26000,L"DPI",true,false},
                            {L"DPI",sdk::OperationKind::Write,100,26000,L"DPI",true,true}};
    sdkManifest.signatureVerified=true; sdkManifest.safetyReviewed=true; sdkManifest.hardwareTested=true;
    std::wstring sdkReason;
    assert(sdk::AdapterSdk::validate(sdkManifest,sdkReason));
    sdk::DeviceIdentity sdkDevice{0x1234,0x5678,0x0100,true,L"Sample Vendor",L"Model A",L"1.2.0"};
    assert(sdk::AdapterSdk::matches(sdkManifest,sdkDevice,sdkReason));
    auto wrongDevice=sdkDevice; wrongDevice.model=L"Model B";
    sdk::RuntimePolicy productionDefault;
    auto numericFirmware=sdkDevice; numericFirmware.firmware=L"1.10.0";
    auto firmwareManifest=sdkManifest; firmwareManifest.maximumFirmware=L"1.9.9";
    assert(!sdk::AdapterSdk::matches(firmwareManifest,numericFirmware,sdkReason));
    firmwareManifest.maximumFirmware=L"1.10.0";
    assert(sdk::AdapterSdk::matches(firmwareManifest,numericFirmware,sdkReason));
    firmwareManifest.minimumFirmware=L"1.11.0";
    assert(!sdk::AdapterSdk::validate(firmwareManifest,sdkReason));
    firmwareManifest=sdkManifest; firmwareManifest.maximumFirmware=L"1.2.";
    assert(!sdk::AdapterSdk::validate(firmwareManifest,sdkReason));
    firmwareManifest=sdkManifest; firmwareManifest.version=L"1.2.x";
    assert(!sdk::AdapterSdk::validate(firmwareManifest,sdkReason));
    sdk::RuntimePolicy sdkPolicy; sdkPolicy.enableHardwareWrites=true; // Test-only mock transport.
    sdk::OperationRequest sdkRequest{L"DPI",1600,true};
    MockScalarTransport mock;
    const auto inspection=sdk::AdapterSdk::inspect(sdkManifest,sdkDevice,mock,productionDefault);
    assert(inspection.state==L"EXACT_ADAPTER_INSPECTED" && inspection.identityMatched);
    assert(inspection.capabilities.size()==1 && inspection.capabilities[0].readable && inspection.capabilities[0].writable);
    assert(inspection.capabilities[0].state==sdk::CapabilityState::WriteLocked);
    const auto trustedInspection=sdk::AdapterSdk::inspect(sdkManifest,sdkDevice,mock,sdkPolicy);
    assert(trustedInspection.trustedForWrites && trustedInspection.capabilities[0].state==sdk::CapabilityState::WriteAvailable);
    const auto inspectionWrong=sdk::AdapterSdk::inspect(sdkManifest,wrongDevice,mock,productionDefault);
    assert(inspectionWrong.state==L"IDENTITY_MISMATCH" && inspectionWrong.capabilities.empty());
    const auto readResult=sdk::AdapterSdk::executeRead(sdkManifest,sdkDevice,L"DPI",mock,audit);
    assert(readResult.success && readResult.state==L"READ_OK" && readResult.verifiedValue==800);
    MockScalarTransport invalidRead; invalidRead.current=99999;
    assert(sdk::AdapterSdk::executeRead(sdkManifest,sdkDevice,L"DPI",invalidRead,audit).state==L"READ_VALUE_OUT_OF_RANGE");
    struct WrongTransport final : MockScalarTransport {
        sdk::Transport transport() const override { return sdk::Transport::VendorBulk; }
    };
    WrongTransport wrongTransport;
    assert(sdk::AdapterSdk::executeRead(sdkManifest,sdkDevice,L"DPI",wrongTransport,audit).state==L"TRANSPORT_MISMATCH");
    auto sdkResult=sdk::AdapterSdk::execute(sdkManifest,sdkDevice,sdkRequest,sdkPolicy,mock,audit);
    assert(sdkResult.success && sdkResult.changed && sdkResult.previousValue==800 && sdkResult.verifiedValue==1600);
    assert(audit.events.size() >= 3);
    assert(audit.events[audit.events.size()-2].state==L"WRITE_REQUESTED");
    assert(audit.events.back().state==L"WRITE_VERIFIED");
    MockAuditSink unavailableAudit; unavailableAudit.available=false;
    MockScalarTransport auditBlockedTransport;
    const auto auditBlocked=sdk::AdapterSdk::execute(sdkManifest,sdkDevice,sdkRequest,sdkPolicy,auditBlockedTransport,unavailableAudit);
    assert(auditBlocked.state==L"AUDIT_LOG_UNAVAILABLE" && auditBlockedTransport.writes==0);
    auto noConfirm=sdkRequest; noConfirm.userConfirmed=false;
    assert(sdk::AdapterSdk::execute(sdkManifest,sdkDevice,noConfirm,sdkPolicy,mock,audit).state==L"AWAITING_USER_CONFIRMATION");
    auto outOfRange=sdkRequest; outOfRange.value=99999;
    assert(sdk::AdapterSdk::execute(sdkManifest,sdkDevice,outOfRange,sdkPolicy,mock,audit).state==L"REJECTED_OUT_OF_RANGE");
    assert(sdk::AdapterSdk::execute(sdkManifest,wrongDevice,sdkRequest,sdkPolicy,mock,audit).state==L"REJECTED_IDENTITY");
    auto unknownRevision=sdkDevice; unknownRevision.revisionKnown=false;
    assert(sdk::AdapterSdk::execute(sdkManifest,unknownRevision,sdkRequest,sdkPolicy,mock,audit).state==L"REJECTED_IDENTITY");
    auto unknownFirmware=sdkDevice; unknownFirmware.firmware=L"";
    assert(sdk::AdapterSdk::execute(sdkManifest,unknownFirmware,sdkRequest,sdkPolicy,mock,audit).state==L"REJECTED_IDENTITY");
    auto unsignedAdapter=sdkManifest; unsignedAdapter.signatureVerified=false;
    assert(sdk::AdapterSdk::execute(unsignedAdapter,sdkDevice,sdkRequest,sdkPolicy,mock,audit).state==L"REJECTED_UNTRUSTED_ADAPTER");
    assert(sdk::AdapterSdk::execute(sdkManifest,sdkDevice,sdkRequest,productionDefault,mock,audit).state==L"HARDWARE_WRITES_DISABLED");
    auto badManifest=sdkManifest; badManifest.evidenceUri=L"file:///evil";
    assert(!sdk::AdapterSdk::validate(badManifest,sdkReason));
    badManifest=sdkManifest; badManifest.operations[1].requiresReadback=false;
    assert(!sdk::AdapterSdk::validate(badManifest,sdkReason));
    MockScalarTransport unsafePrevious; unsafePrevious.current=99999;
    const auto unsafePreviousResult=sdk::AdapterSdk::execute(sdkManifest,sdkDevice,sdkRequest,sdkPolicy,unsafePrevious,audit);
    assert(unsafePreviousResult.state==L"PREVIOUS_VALUE_OUT_OF_RANGE" && unsafePrevious.writes==0);
    MockScalarTransport rollbackMock; rollbackMock.corruptReadback=true;
    auto rollbackResult=sdk::AdapterSdk::execute(sdkManifest,sdkDevice,sdkRequest,sdkPolicy,rollbackMock,audit);
    assert(!rollbackResult.success && rollbackResult.rollbackAttempted && !rollbackResult.rollbackSucceeded);
    MockScalarTransport disconnected; disconnected.online=false;
    assert(sdk::AdapterSdk::execute(sdkManifest,sdkDevice,sdkRequest,sdkPolicy,disconnected,audit).state==L"DEVICE_DISCONNECTED");

    // Universal discovery classifies standard interfaces without inferring vendor controls.
    using namespace unify::universal;
    Input genericKeyboard; genericKeyboard.transport=Transport::UsbHid; genericKeyboard.vid=0x1234; genericKeyboard.pid=0x5678;
    genericKeyboard.hasManufacturerString=true; genericKeyboard.hasProductString=true; genericKeyboard.usagePage=0x01; genericKeyboard.usage=0x06;
    genericKeyboard.inputReportBytes=8; genericKeyboard.outputReportBytes=2; genericKeyboard.featureReportBytes=16;
    const auto keyboardProfile=Classifier::classify(genericKeyboard);
    assert(keyboardProfile.function==Function::Keyboard);
    assert(keyboardProfile.identity==IdentityLevel::HardwareIdAndStrings);
    assert(keyboardProfile.observations.size()==4);
    assert(!keyboardProfile.configurationProtocolKnown && !keyboardProfile.hardwareWritesAllowed);
    Input genericMouse=genericKeyboard; genericMouse.usage=0x02;
    const auto mouseProfile=Classifier::classify(genericMouse);
    assert(mouseProfile.function==Function::Mouse);
    Input unknownDevice; unknownDevice.transport=Transport::UsbVendorSpecific; unknownDevice.vid=0x2222; unknownDevice.pid=0x3333;
    unknownDevice.outputReportBytes=64;
    const auto unknownProfile=Classifier::classify(unknownDevice);
    assert(unknownProfile.function==Function::Unknown);
    assert(unknownProfile.observations.size()==1);
    assert(!unknownProfile.hardwareWritesAllowed);
    Input exactInput=genericKeyboard; exactInput.catalogExactMatch=true;
    assert(Classifier::classify(exactInput).identity==IdentityLevel::CatalogExactMatch);

    // HID report descriptor parser: report IDs, report kinds, collections and strict bounds.
    using namespace unify::hid;
    const std::vector<std::uint8_t> keyboardDescriptor{
        0x05,0x01, 0x09,0x06, 0xA1,0x01, 0x85,0x01,
        0x05,0x07, 0x19,0xE0, 0x29,0xE7, 0x15,0x00, 0x25,0x01,
        0x75,0x01, 0x95,0x08, 0x81,0x02,
        0x95,0x01, 0x75,0x08, 0x81,0x01,
        0x95,0x05, 0x75,0x01, 0x05,0x08, 0x19,0x01, 0x29,0x05, 0x91,0x02,
        0x95,0x06, 0x75,0x08, 0x15,0x00, 0x26,0xFF,0x00, 0x05,0x07, 0x19,0x00, 0x2A,0xFF,0x00, 0xB1,0x02,
        0xC0
    };
    const auto parsedKeyboard=ReportDescriptorParser::parse(keyboardDescriptor);
    assert(parsedKeyboard.valid && parsedKeyboard.status==L"PARSED");
    assert(parsedKeyboard.topLevelUsagePage==0x01 && parsedKeyboard.topLevelUsage==0x06);
    assert(parsedKeyboard.collections.size()==1 && parsedKeyboard.reports.size()==3);
    bool sawInput=false,sawOutput=false,sawFeature=false;
    for(const auto& report:parsedKeyboard.reports) {
        assert(report.reportId==1);
        if(report.kind==ReportKind::Input) { sawInput=true; assert(report.bitLength==16 && report.byteLength==3); } // 2 data bytes + Report ID byte
        if(report.kind==ReportKind::Output) sawOutput=true;
        if(report.kind==ReportKind::Feature) sawFeature=true;
    }
    assert(sawInput && sawOutput && sawFeature);
    auto truncated=keyboardDescriptor; truncated.pop_back();
    assert(!ReportDescriptorParser::parse(truncated).valid);
    assert(!ReportDescriptorParser::parse({0xC0}).valid); // unmatched END_COLLECTION
    assert(!ReportDescriptorParser::parse({0x85,0x00}).valid); // zero Report ID
    std::vector<std::uint8_t> tooLarge(ReportDescriptorParser::MaxDescriptorBytes+1,0);
    assert(!ReportDescriptorParser::parse(tooLarge).valid);
    assert(!ReportDescriptorParser::parse({0xA4}).valid); // unbalanced PUSH
    assert(!ReportDescriptorParser::parse({0xFE,0x04,0x01,0xAA}).valid); // truncated long item
    assert(!ReportDescriptorParser::parse({0x75,0x08,0x95,0x01,0x81,0x02,0x85,0x01,0x81,0x02}).valid); // mixed implicit/explicit IDs

    // USB descriptor parser validates device/configuration/interface/endpoint structures without I/O.
    const std::vector<std::uint8_t> usbDeviceDescriptor{0x12,0x01,0x00,0x02,0x00,0x00,0x00,0x40,0x34,0x12,0x78,0x56,0x00,0x01,0x01,0x02,0x00,0x01};
    const auto parsedUsbDevice=unify::usb::DescriptorParser::parseDevice(usbDeviceDescriptor);
    assert(parsedUsbDevice.valid && parsedUsbDevice.vendorId==0x1234 && parsedUsbDevice.productId==0x5678);
    const std::vector<std::uint8_t> usbConfig{0x09,0x02,0x20,0x00,0x01,0x01,0x00,0x80,0x32, 0x09,0x04,0x00,0x00,0x02,0x03,0x01,0x01,0x00, 0x07,0x05,0x81,0x03,0x08,0x00,0x0A, 0x07,0x05,0x01,0x03,0x08,0x00,0x0A};
    const auto parsedUsbConfig=unify::usb::DescriptorParser::parseConfiguration(usbConfig);
    assert(parsedUsbConfig.valid && parsedUsbConfig.interfaces.size()==1);
    assert(parsedUsbConfig.interfaces[0].endpoints.size()==2 && parsedUsbConfig.interfaces[0].endpoints[0].address==0x81);
    auto usbAlternate=usbConfig; usbAlternate[2]=0x29; usbAlternate[3]=0x00; usbAlternate.insert(usbAlternate.end(),{0x09,0x04,0x00,0x01,0x00,0x03,0x01,0x01,0x00});
    const auto parsedUsbAlternate=unify::usb::DescriptorParser::parseConfiguration(usbAlternate);
    assert(parsedUsbAlternate.valid && parsedUsbAlternate.interfaces.size()==2 && parsedUsbAlternate.declaredInterfaces==1);
    auto usbTruncated=usbConfig;usbTruncated.pop_back();assert(!unify::usb::DescriptorParser::parseConfiguration(usbTruncated).valid);
    auto usbBadEndpoint=usbConfig;usbBadEndpoint[20]=0x80;assert(!unify::usb::DescriptorParser::parseConfiguration(usbBadEndpoint).valid);
    auto usbWrongCount=usbConfig;usbWrongCount[13]=0x01;assert(!unify::usb::DescriptorParser::parseConfiguration(usbWrongCount).valid);
    assert(!unify::usb::DescriptorParser::parseDevice({0x12,0x01}).valid);
    auto usbBadConfig=usbConfig;usbBadConfig[7]=0x00;assert(!unify::usb::DescriptorParser::parseConfiguration(usbBadConfig).valid);

    // Transport registry is extensible for discovery but cannot register active I/O.
    using namespace unify::transport;
    struct FakeDiscovery final : IDiscoveryBackend {
        BackendDescriptor d; std::vector<unify::transport::Observation> records; bool failEnumeration{false}; bool failDescriptor{false};
        BackendDescriptor descriptor() const override { if(failDescriptor) throw std::runtime_error("descriptor failure"); return d; }
        std::vector<unify::transport::Observation> enumerate() override { if(failEnumeration) throw std::runtime_error("fixture failure"); return records; }
    };
    Registry transports; std::wstring transportReason;
    auto usbBackend=std::make_shared<FakeDiscovery>();
    usbBackend->d={L"test.usb",L"Test USB enumerator",Kind::UsbPnP,Access::MetadataOnly,true};
    unify::transport::Observation usbObservation; usbObservation.kind=Kind::Unknown; usbObservation.vid=0x1234; usbObservation.pid=0x5678;
    usbBackend->records.push_back(usbObservation);
    assert(transports.add(usbBackend,transportReason));
    std::vector<BackendStatus> backendStatus;
    const auto transportRecords=transports.enumerateAll(backendStatus);
    assert(transportRecords.size()==1 && transportRecords[0].kind==Kind::UsbPnP);
    assert(backendStatus.size()==1 && backendStatus[0].state==L"ENUMERATED");
    auto unsafeBackend=std::make_shared<FakeDiscovery>();
    unsafeBackend->d={L"unsafe",L"Unsafe writer",Kind::UsbVendorInterface,Access::ConfigurationWrite,true};
    assert(!transports.add(unsafeBackend,transportReason));
    auto untrustedBackend=std::make_shared<FakeDiscovery>();
    untrustedBackend->d={L"untrusted",L"Untrusted enumerator",Kind::BleGatt,Access::MetadataOnly,false};
    assert(!transports.add(untrustedBackend,transportReason));
    auto descriptorThrowingBackend=std::make_shared<FakeDiscovery>();
    descriptorThrowingBackend->failDescriptor=true;
    assert(!transports.add(descriptorThrowingBackend,transportReason));
    auto mismatchedBackend=std::make_shared<FakeDiscovery>();
    mismatchedBackend->d={L"test.mismatch",L"Mismatched enumerator",Kind::UsbPnP,Access::MetadataOnly,true};
    unify::transport::Observation wrongTransportObservation; wrongTransportObservation.kind=Kind::BleGatt;
    mismatchedBackend->records.push_back(wrongTransportObservation);
    assert(transports.add(mismatchedBackend,transportReason));
    const auto mismatchResults=transports.enumerateAll(backendStatus);
    assert(mismatchResults.size()==1 && backendStatus[1].state==L"ENUMERATED_WITH_WARNINGS");
    assert(!transports.add(usbBackend,transportReason)); // duplicate ID
    auto throwingBackend=std::make_shared<FakeDiscovery>();
    throwingBackend->d={L"test.throw",L"Throwing test enumerator",Kind::OtherPnP,Access::MetadataOnly,true};
    throwingBackend->failEnumeration=true;
    assert(transports.add(throwingBackend,transportReason));
    const auto isolatedResults=transports.enumerateAll(backendStatus);
    assert(isolatedResults.size()==1 && backendStatus.size()==3 && backendStatus[2].state==L"BACKEND_ERROR");
    auto winUsbDescriptorBackend=std::make_shared<FakeDiscovery>();
    winUsbDescriptorBackend->d={L"test.winusb.descriptors",L"WinUSB descriptor metadata",Kind::UsbVendorInterface,Access::DescriptorRead,true};
    assert(transports.add(winUsbDescriptorBackend,transportReason));
    auto bleDescriptorBackend=std::make_shared<FakeDiscovery>();
    bleDescriptorBackend->d={L"test.ble.gatt",L"BLE GATT metadata",Kind::BleGatt,Access::DescriptorRead,true};
    assert(transports.add(bleDescriptorBackend,transportReason));

    // Simulated WinUSB pipe metadata and BLE GATT metadata validation. These fixtures
    // exercise bounds/shape validation without claiming physical Windows enumeration.
    using namespace unify::transport;
    std::wstring metadataReason;
    UsbEndpoint bulkIn{1,0,0x81,2,512,0,0x81};
    assert(validateUsbEndpoint(bulkIn,metadataReason));
    assert(std::wstring(usbTransferType(bulkIn.attributes))==L"BULK");
    UsbEndpoint invalidEndpoint=bulkIn; invalidEndpoint.address=0x01; invalidEndpoint.pipeId=0x82;
    assert(!validateUsbEndpoint(invalidEndpoint,metadataReason));
    invalidEndpoint=bulkIn; invalidEndpoint.maxPacketSize=2048;
    assert(!validateUsbEndpoint(invalidEndpoint,metadataReason));
    std::vector<GattService> gattFixture{{L"0000180f-0000-1000-8000-00805f9b34fb",{{L"00002a19-0000-1000-8000-00805f9b34fb",0x12u}}}};
    assert(validateGattMetadata(gattFixture,metadataReason));
    auto extendedGatt=gattFixture; extendedGatt[0].characteristics[0].properties=0x3ffu;
    assert(validateGattMetadata(extendedGatt,metadataReason));
    auto invalidGatt=gattFixture; invalidGatt[0].characteristics[0].uuid.clear();
    assert(!validateGattMetadata(invalidGatt,metadataReason));
    invalidGatt=gattFixture; invalidGatt[0].characteristics[0].properties=0x800u;
    assert(!validateGattMetadata(invalidGatt,metadataReason));
    DeviceInfo transportMetadata; transportMetadata.winUsbStatus=L"WINUSB_ENDPOINT_METADATA_ENUMERATED_NO_TRANSFERS";
    transportMetadata.usbEndpoints.push_back(bulkIn); transportMetadata.gattStatus=L"GATT_SERVICES_AND_CHARACTERISTICS_ENUMERATED";
    transportMetadata.gattServices=gattFixture;
    const auto transportMetadataJson=DeviceScanner::toJson({transportMetadata});
    assert(transportMetadataJson.find(L"usbEndpoints")!=std::wstring::npos);
    assert(transportMetadataJson.find(L"gattServices")!=std::wstring::npos);
    assert(transportMetadataJson.find(L"WINUSB_ENDPOINT_METADATA_ENUMERATED_NO_TRANSFERS")!=std::wstring::npos);
    // The SDK recognizes the transport identifiers, but discovery itself exposes no writer.
    unify::sdk::AdapterManifest transportManifest; transportManifest.id=L"test.winusb"; transportManifest.version=L"1.0.0";
    transportManifest.vendor=L"Test"; transportManifest.model=L"Fixture"; transportManifest.evidenceUri=L"https://example.invalid/protocol";
    transportManifest.vid=0x1234; transportManifest.pid=0x5678; transportManifest.transport=unify::sdk::Transport::UsbWinUsb;
    transportManifest.operations.push_back({L"DPI",unify::sdk::OperationKind::Read,0,10000,L"dpi",true,false});
    assert(unify::sdk::AdapterSdk::validate(transportManifest,metadataReason));

    // Evidence assessment distinguishes identification from compatibility and catches conflicts.
    using namespace unify::evidence;
    std::vector<Item> evidenceItems{
        {Type::HardwareId,L"VID_1234&PID_5678",L"Windows PnP",L"Observed",false},
        {Type::ManufacturerString,L"Example Vendor",L"HID string",L"Observed",false},
        {Type::ProductString,L"Example Input Device",L"HID string",L"Observed",false},
        {Type::HidDescriptor,L"sha256:descriptor",L"Descriptor capture",L"Observed",false}
    };
    const auto assessment=Engine::assess(evidenceItems);
    assert(assessment.identityLevel==L"HARDWARE_ID_AND_STRINGS");
    assert(assessment.confidence==L"MODERATE" && assessment.state==L"IDENTIFIED_PARTIALLY");
    assert(assessment.score==50);
    evidenceItems.push_back({Type::TrustedCatalog,L"exact-record",L"https://example.invalid/evidence",L"Reviewed record",true});
    const auto corroborated=Engine::assess(evidenceItems);
    assert(corroborated.identityLevel==L"CATALOG_CORROBORATED" && corroborated.confidence==L"HIGH");
    evidenceItems.push_back({Type::HardwareId,L"VID_ABCD&PID_EF01",L"Conflicting source",L"Conflict",false});
    assert(Engine::assess(evidenceItems).state==L"MANUAL_REVIEW_REQUIRED");
    assert(Engine::assess({{Type::IdentityConflict,L"AMBIGUOUS DATABASE MATCH",L"Local database",L"Manual review required",false}}).state==L"MANUAL_REVIEW_REQUIRED");
    assert(Engine::assess({}).state==L"DETECTED_ONLY");

    // Diagnostics explain unknown devices without treating report channels as configuration support.
    using namespace unify::diagnostics;
    Facts unknownFacts; unknownFacts.present=true; unknownFacts.hidInterface=true;
    unknownFacts.descriptorReadable=true; unknownFacts.descriptorValid=true; unknownFacts.usageRecognized=false;
    unknownFacts.reportInput=true; unknownFacts.adapterMatched=false;
    const auto findings=unify::diagnostics::Engine::analyze(unknownFacts);
    assert(findings.size()==2);
    assert(findings[0].code==L"HID_USAGE_UNCLASSIFIED");
    assert(findings[1].code==L"NO_ADAPTER");
    Facts conflictFacts; conflictFacts.identityConflict=true; conflictFacts.adapterMatched=true; conflictFacts.protocolVerified=false;
    const auto conflictFindings=unify::diagnostics::Engine::analyze(conflictFacts);
    bool sawConflict=false,sawUnverified=false;
    for(const auto& finding:conflictFindings) { if(finding.code==L"IDENTITY_CONFLICT") sawConflict=true; if(finding.code==L"ADAPTER_PROTOCOL_UNVERIFIED") sawUnverified=true; }
    assert(sawConflict && sawUnverified);

    std::cout << "UNIFY CONTROL v1.8.0: core, HID/USB parsers, WinUSB/GATT metadata fixtures, transport registry, evidence engine, diagnostics, database and adapter SDK tests passed\n";
}
