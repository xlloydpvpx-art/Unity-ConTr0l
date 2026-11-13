#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace unify::sdk {

enum class Transport { HidFeature, HidOutput, VendorControl, VendorBulk, UsbWinUsb, BleGatt, Unknown };
enum class OperationKind { Read, Write };

struct DeviceIdentity {
    std::uint16_t vid{}, pid{}, revision{};
    bool revisionKnown{false};
    std::wstring manufacturer, model, firmware;
};

struct OperationSpec {
    std::wstring capability;
    OperationKind kind{OperationKind::Read};
    std::int64_t minimum{}, maximum{};
    std::wstring unit;
    bool requiresReadback{true};
    bool reversible{false};
};

struct AdapterManifest {
    std::wstring id, version, vendor, model, evidenceUri;
    std::uint16_t vid{}, pid{}, revision{};
    bool revisionSpecified{false};
    std::wstring minimumFirmware, maximumFirmware;
    Transport transport{Transport::Unknown};
    std::vector<OperationSpec> operations;
    bool signatureVerified{false};
    bool safetyReviewed{false};
    bool hardwareTested{false};
};

struct RuntimePolicy {
    // Production builds must leave this false until a reviewed hardware transport exists.
    bool enableHardwareWrites{false};
};

class IScalarTransport {
public:
    virtual ~IScalarTransport() = default;
    virtual bool read(const std::wstring& capability, std::int64_t& value, std::wstring& error) = 0;
    virtual bool write(const std::wstring& capability, std::int64_t value, std::wstring& error) = 0;
    virtual bool connected() const = 0;
    // A transport must identify itself; Unknown never satisfies a manifest.
    virtual Transport transport() const { return Transport::Unknown; }
};

struct OperationRequest {
    std::wstring capability;
    std::int64_t value{};
    bool userConfirmed{false};
};

struct OperationResult {
    bool success{false};
    bool changed{false};
    bool rollbackAttempted{false};
    bool rollbackSucceeded{false};
    std::wstring state, message;
    std::optional<std::int64_t> previousValue, verifiedValue;
};

enum class CapabilityState { ReadAvailable, WriteLocked, WriteAvailable, NotDeclared };

struct CapabilityAvailability {
    std::wstring capability;
    bool readable{false};
    bool writable{false};
    CapabilityState state{CapabilityState::NotDeclared};
    std::wstring reason;
};

struct AdapterInspection {
    bool identityMatched{false};
    bool transportMatched{false};
    bool connected{false};
    bool trustedForWrites{false};
    std::wstring state, reason;
    std::vector<CapabilityAvailability> capabilities;
};

struct AuditEvent {
    std::wstring adapterId, capability, operation, state;
    std::uint16_t vid{}, pid{};
    std::optional<std::int64_t> requestedValue, previousValue, observedValue;
};

class IAuditSink {
public:
    virtual ~IAuditSink() = default;
    // Return false when the event could not be durably recorded.
    virtual bool record(const AuditEvent& event) = 0;
};

class AdapterSdk {
public:
    static OperationResult executeRead(const AdapterManifest& manifest,
                                       const DeviceIdentity& device,
                                       const std::wstring& capability,
                                       IScalarTransport& transport,
                                       IAuditSink& audit);
    static bool validate(const AdapterManifest& manifest, std::wstring& reason);
    static bool matches(const AdapterManifest& manifest, const DeviceIdentity& device, std::wstring& reason);
    // Reports only operations explicitly declared by an exact, validated adapter.
    // It never probes or communicates with the device.
    static AdapterInspection inspect(const AdapterManifest& manifest,
                                     const DeviceIdentity& device,
                                     const IScalarTransport& transport,
                                     const RuntimePolicy& policy);
    static OperationResult execute(const AdapterManifest& manifest,
                                   const DeviceIdentity& device,
                                   const OperationRequest& request,
                                   const RuntimePolicy& policy,
                                   IScalarTransport& transport,
                                   IAuditSink& audit);
};

} // namespace unify::sdk
