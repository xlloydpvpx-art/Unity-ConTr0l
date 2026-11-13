#include "unify/adapter_sdk.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <string_view>
#include <vector>

namespace unify::sdk {
namespace {
const std::set<std::wstring> kCapabilities{
    L"DPI", L"DPI_XY", L"POLLING_RATE", L"LIFT_OFF_DISTANCE", L"ANGLE_SNAPPING",
    L"MOTION_SYNC", L"RIPPLE_CONTROL", L"DEBOUNCE_TIME", L"BUTTON_REMAP", L"SCROLL_SETTINGS",
    L"KEY_REMAP", L"RAPID_TRIGGER", L"ADJUSTABLE_ACTUATION", L"SOCD", L"DKS",
    L"SNAP_TAP", L"RGB", L"RGB_BRIGHTNESS", L"PROFILE_SWITCH", L"BATTERY_LEVEL"
};

bool validTransport(Transport transport) {
    switch (transport) {
    case Transport::HidFeature: case Transport::HidOutput: case Transport::VendorControl:
    case Transport::VendorBulk: case Transport::UsbWinUsb: case Transport::BleGatt: return true;
    default: return false;
    }
}

bool safeHttps(const std::wstring& uri) {
    if (uri.rfind(L"https://", 0) != 0 || uri.size() <= 8) return false;
    const auto hostEnd = uri.find_first_of(L"/?#", 8);
    const auto host = uri.substr(8, hostEnd == std::wstring::npos ? hostEnd : hostEnd - 8);
    return !host.empty() && host.find(L' ') == std::wstring::npos && host.find(L'@') == std::wstring::npos;
}

// Firmware versions are intentionally limited to numeric dotted components.
// Lexical comparison is incorrect (for example, "1.10" sorts before "1.2").
bool parseVersion(const std::wstring& value, std::vector<std::uint32_t>& parts) {
    parts.clear();
    if (value.empty() || value.front() == L'.' || value.back() == L'.') return false;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto end = value.find(L'.', start);
        const auto token = value.substr(start, end == std::wstring::npos ? end : end - start);
        if (token.empty() || token.size() > 10 ||
            !std::all_of(token.begin(), token.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) return false;
        try {
            const auto number = std::stoull(token);
            if (number > std::numeric_limits<std::uint32_t>::max()) return false;
            parts.push_back(static_cast<std::uint32_t>(number));
        } catch (...) { return false; }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return !parts.empty() && parts.size() <= 8;
}

int compareVersions(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
    const auto count = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto av = i < a.size() ? a[i] : 0;
        const auto bv = i < b.size() ? b[i] : 0;
        if (av < bv) return -1;
        if (av > bv) return 1;
    }
    return 0;
}

bool firmwareInRange(const std::wstring& actual, const std::wstring& minimum, const std::wstring& maximum) {
    if (minimum.empty() && maximum.empty()) return true;
    std::vector<std::uint32_t> actualParts, minParts, maxParts;
    if (!parseVersion(actual, actualParts)) return false;
    if (!minimum.empty() && (!parseVersion(minimum, minParts) || compareVersions(actualParts, minParts) < 0)) return false;
    if (!maximum.empty() && (!parseVersion(maximum, maxParts) || compareVersions(actualParts, maxParts) > 0)) return false;
    return true;
}
}

bool AdapterSdk::validate(const AdapterManifest& m, std::wstring& reason) {
    std::vector<std::uint32_t> adapterVersion;
    if (m.id.size() < 3 || m.id.size() > 64 ||
        !std::all_of(m.id.begin(), m.id.end(), [](wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'.' || c == L'_' || c == L'-'; }) ||
        !parseVersion(m.version, adapterVersion) || adapterVersion.size() != 3 || m.vendor.empty() || m.vendor.size() > 128 || m.model.empty() || m.model.size() > 128) {
        reason = L"Manifest identity or numeric version is incomplete or invalid."; return false;
    }
    if (m.vid == 0 || m.pid == 0 || !validTransport(m.transport)) {
        reason = L"VID/PID and a declared transport are required."; return false;
    }
    if (!safeHttps(m.evidenceUri)) { reason = L"Evidence must reference a valid HTTPS host."; return false; }
    if (m.revisionSpecified && m.revision == 0) { reason = L"A revision-scoped adapter must declare a nonzero revision."; return false; }
    std::vector<std::uint32_t> minParts, maxParts;
    if ((!m.minimumFirmware.empty() && !parseVersion(m.minimumFirmware, minParts)) ||
        (!m.maximumFirmware.empty() && !parseVersion(m.maximumFirmware, maxParts)) ||
        (!m.minimumFirmware.empty() && !m.maximumFirmware.empty() && compareVersions(minParts, maxParts) > 0)) {
        reason = L"Firmware bounds must be valid numeric dotted versions in ascending order."; return false;
    }
    if (m.operations.empty() || m.operations.size() > 128) { reason = L"The manifest must declare 1–128 bounded operations."; return false; }
    std::set<std::pair<std::wstring, OperationKind>> seen;
    for (const auto& op : m.operations) {
        if (!kCapabilities.contains(op.capability)) { reason = L"Operation uses a capability outside the SDK allowlist."; return false; }
        if (op.kind != OperationKind::Read && op.kind != OperationKind::Write) { reason = L"Operation kind is invalid."; return false; }
        if (op.minimum > op.maximum || op.unit.size() > 24) { reason = L"Operation bounds or unit are invalid."; return false; }
        if (!seen.emplace(op.capability, op.kind).second) { reason = L"Duplicate capability operation."; return false; }
        if (op.kind == OperationKind::Write && !op.requiresReadback) { reason = L"Writes must require readback verification."; return false; }
    }
    reason.clear(); return true;
}

bool AdapterSdk::matches(const AdapterManifest& m, const DeviceIdentity& d, std::wstring& reason) {
    if (m.vid != d.vid || m.pid != d.pid) { reason = L"VID/PID mismatch."; return false; }
    if (m.model != d.model || m.vendor != d.manufacturer) { reason = L"Exact manufacturer/model identity mismatch."; return false; }
    if (m.revisionSpecified && (!d.revisionKnown || m.revision != d.revision)) {
        reason = L"Required hardware revision is unknown or different."; return false;
    }
    if (!firmwareInRange(d.firmware, m.minimumFirmware, m.maximumFirmware)) {
        reason = L"Firmware is outside the adapter's declared range or unavailable."; return false;
    }
    reason.clear(); return true;
}

AdapterInspection AdapterSdk::inspect(const AdapterManifest& m, const DeviceIdentity& d,
                                      const IScalarTransport& transport, const RuntimePolicy& policy) {
    AdapterInspection report;
    std::wstring reason;
    if (!validate(m, reason)) {
        report.state = L"INVALID_ADAPTER"; report.reason = reason; return report;
    }
    if (!matches(m, d, reason)) {
        report.state = L"IDENTITY_MISMATCH"; report.reason = reason; return report;
    }
    report.identityMatched = true;
    report.connected = transport.connected();
    report.transportMatched = transport.transport() == m.transport;
    if (!report.connected) {
        report.state = L"DEVICE_DISCONNECTED"; report.reason = L"Device is not connected."; return report;
    }
    if (!report.transportMatched) {
        report.state = L"TRANSPORT_MISMATCH"; report.reason = L"The runtime transport does not match the adapter manifest."; return report;
    }
    report.trustedForWrites = m.signatureVerified && m.safetyReviewed && m.hardwareTested && policy.enableHardwareWrites;
    for (const auto& op : m.operations) {
        auto found = std::find_if(report.capabilities.begin(), report.capabilities.end(), [&](const auto& item) {
            return item.capability == op.capability;
        });
        if (found == report.capabilities.end()) {
            report.capabilities.push_back({op.capability, false, false, CapabilityState::NotDeclared, L""});
            found = std::prev(report.capabilities.end());
        }
        if (op.kind == OperationKind::Read) {
            found->readable = true;
            if (!found->writable) {
                found->state = CapabilityState::ReadAvailable;
                found->reason = L"An exact adapter declares a bounded read operation.";
            }
        } else {
            found->writable = true;
            found->state = report.trustedForWrites ? CapabilityState::WriteAvailable : CapabilityState::WriteLocked;
            found->reason = report.trustedForWrites
                ? L"Declared write is eligible for the SDK confirmation and verification flow."
                : L"Write remains locked: trusted signature, safety review, physical tests and enabled host policy are required.";
        }
    }
    report.state = L"EXACT_ADAPTER_INSPECTED";
    report.reason = L"Only explicitly declared capabilities are reported; inspection performs no device I/O.";
    return report;
}

OperationResult AdapterSdk::executeRead(const AdapterManifest& m, const DeviceIdentity& d,
                                        const std::wstring& capability, IScalarTransport& transport,
                                        IAuditSink& audit) {
    OperationResult result;
    auto finish = [&](OperationResult value) {
        AuditEvent event{m.id, capability, L"READ", value.state, d.vid, d.pid, {}, {}, value.verifiedValue};
        if (!audit.record(event)) {
            value.success = false;
            value.state = L"AUDIT_LOG_FAILED";
            value.message = L"The operation result could not be recorded by the audit sink.";
        }
        return value;
    };
    std::wstring reason;
    if (!validate(m, reason)) { result.state=L"REJECTED_MANIFEST"; result.message=reason; return finish(result); }
    if (!matches(m, d, reason)) { result.state=L"REJECTED_IDENTITY"; result.message=reason; return finish(result); }
    const auto it = std::find_if(m.operations.begin(), m.operations.end(), [&](const auto& op) {
        return op.capability == capability && op.kind == OperationKind::Read;
    });
    if (it == m.operations.end()) { result.state=L"REJECTED_UNDECLARED_OPERATION"; result.message=L"The adapter does not declare this read operation."; return finish(result); }
    if (!transport.connected()) { result.state=L"DEVICE_DISCONNECTED"; result.message=L"Device is not connected."; return finish(result); }
    if (transport.transport() != m.transport) { result.state=L"TRANSPORT_MISMATCH"; result.message=L"The runtime transport does not match the adapter manifest."; return finish(result); }
    AuditEvent intent{m.id, capability, L"READ", L"READ_REQUESTED", d.vid, d.pid, {}, {}, {}};
    if (!audit.record(intent)) { result.state=L"AUDIT_LOG_UNAVAILABLE"; result.message=L"Read was not started because the audit sink did not accept the request."; return result; }
    std::int64_t value{};
    if (!transport.read(capability, value, reason)) { result.state=L"READ_FAILED"; result.message=reason; return finish(result); }
    if (value < it->minimum || value > it->maximum) {
        result.state=L"READ_VALUE_OUT_OF_RANGE"; result.message=L"Device returned a value outside the adapter's declared bounds."; return finish(result);
    }
    result.success=true; result.verifiedValue=value; result.state=L"READ_OK"; result.message=L"Declared value read successfully.";
    return finish(result);
}

OperationResult AdapterSdk::execute(const AdapterManifest& m, const DeviceIdentity& d,
                                    const OperationRequest& request, const RuntimePolicy& policy,
                                    IScalarTransport& transport, IAuditSink& audit) {
    OperationResult result;
    auto finish = [&](OperationResult value) {
        AuditEvent event{m.id, request.capability, L"WRITE", value.state, d.vid, d.pid,
                         request.value, value.previousValue, value.verifiedValue};
        if (!audit.record(event)) {
            value.success = false;
            value.state = value.changed ? L"AUDIT_LOG_FAILED_AFTER_CHANGE" : L"AUDIT_LOG_FAILED";
            value.message = value.changed
                ? L"The device may have changed, but the operation result could not be recorded. Inspect the device before continuing."
                : L"The operation result could not be recorded by the audit sink.";
        }
        return value;
    };
    std::wstring reason;
    if (!validate(m, reason)) { result.state=L"REJECTED_MANIFEST"; result.message=reason; return finish(result); }
    if (!matches(m, d, reason)) { result.state=L"REJECTED_IDENTITY"; result.message=reason; return finish(result); }
    const auto it = std::find_if(m.operations.begin(), m.operations.end(), [&](const auto& op) {
        return op.capability == request.capability && op.kind == OperationKind::Write;
    });
    if (it == m.operations.end()) { result.state=L"REJECTED_UNDECLARED_OPERATION"; result.message=L"The adapter does not declare this write operation."; return finish(result); }
    if (request.value < it->minimum || request.value > it->maximum) {
        result.state=L"REJECTED_OUT_OF_RANGE"; result.message=L"Requested value is outside the adapter's verified limits."; return finish(result);
    }
    if (!request.userConfirmed) { result.state=L"AWAITING_USER_CONFIRMATION"; result.message=L"The user must confirm this device change."; return finish(result); }
    if (!m.signatureVerified || !m.safetyReviewed || !m.hardwareTested) {
        result.state=L"REJECTED_UNTRUSTED_ADAPTER"; result.message=L"Signature, safety review and physical hardware tests are all required."; return finish(result);
    }
    if (!policy.enableHardwareWrites) {
        result.state=L"HARDWARE_WRITES_DISABLED"; result.message=L"This application build has no enabled production hardware-write transport."; return finish(result);
    }
    if (!transport.connected()) { result.state=L"DEVICE_DISCONNECTED"; result.message=L"Device is not connected."; return finish(result); }
    if (transport.transport() != m.transport) { result.state=L"TRANSPORT_MISMATCH"; result.message=L"The runtime transport does not match the adapter manifest."; return finish(result); }
    AuditEvent intent{m.id, request.capability, L"WRITE", L"WRITE_REQUESTED", d.vid, d.pid, request.value, {}, {}};
    if (!audit.record(intent)) { result.state=L"AUDIT_LOG_UNAVAILABLE"; result.message=L"Write was not started because the audit sink did not accept the request."; return result; }

    std::int64_t previous{};
    if (!transport.read(request.capability, previous, reason)) {
        result.state=L"READ_BEFORE_WRITE_FAILED"; result.message=reason; return finish(result);
    }
    result.previousValue = previous;
    if (previous < it->minimum || previous > it->maximum) {
        result.state=L"PREVIOUS_VALUE_OUT_OF_RANGE";
        result.message=L"Current device value is outside the adapter's declared safe range; no write was attempted.";
        return finish(result);
    }
    if (!transport.write(request.capability, request.value, reason)) {
        result.state=L"WRITE_FAILED"; result.message=reason; return finish(result);
    }
    result.changed = true;
    if (!transport.connected()) {
        result.state=L"DISCONNECTED_AFTER_WRITE"; result.message=L"Device disconnected before verification."; return finish(result);
    }
    std::int64_t observed{};
    if (transport.read(request.capability, observed, reason) && observed == request.value) {
        result.success=true; result.verifiedValue=observed; result.state=L"WRITE_VERIFIED";
        result.message=L"The value was written and read back successfully."; return finish(result);
    }

    result.state=L"VERIFY_FAILED"; result.message=L"Readback did not match the requested value.";
    if (it->reversible && transport.connected()) {
        result.rollbackAttempted=true;
        std::wstring rollbackError;
        if (transport.write(request.capability, previous, rollbackError) && transport.connected()) {
            std::int64_t restored{};
            result.rollbackSucceeded=transport.read(request.capability, restored, rollbackError) && restored==previous;
        }
        if (result.rollbackSucceeded) result.state=L"VERIFY_FAILED_ROLLED_BACK";
        else result.state=L"VERIFY_FAILED_ROLLBACK_FAILED";
    }
    return finish(result);
}

} // namespace unify::sdk
