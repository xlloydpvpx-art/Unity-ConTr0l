#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace unify::universal {
// Transport is descriptive only. It never authorizes I/O by itself.
enum class Transport { UsbHid, BluetoothHid, BlePnP, UsbPnP, UsbVendorSpecific, OtherPnP, Unknown };
enum class Function { Keyboard, Mouse, ConsumerControl, GameController, Composite, Unknown };
enum class IdentityLevel { Unknown, HardwareId, HardwareIdAndStrings, CatalogExactMatch };
enum class Observation { InputReports, OutputReportChannel, FeatureReportChannel, StandardKeyboardUsage, StandardMouseUsage, ConsumerUsage, GameControllerUsage };

struct Input {
    Transport transport{Transport::Unknown};
    std::uint16_t vid{}, pid{}, revision{}, usagePage{}, usage{};
    bool revisionKnown{false};
    bool hasManufacturerString{false}, hasProductString{false}, catalogExactMatch{false};
    std::uint16_t inputReportBytes{}, outputReportBytes{}, featureReportBytes{};
};

struct Profile {
    Transport transport{Transport::Unknown};
    Function function{Function::Unknown};
    IdentityLevel identity{IdentityLevel::Unknown};
    bool hasVendorProductId{false};
    bool hasRevision{false};
    // Observations describe exposed interfaces; they do not imply safe configuration.
    std::vector<Observation> observations;
    bool configurationProtocolKnown{false};
    bool hardwareWritesAllowed{false};
    std::wstring status;
};

class Classifier {
public:
    static Profile classify(const Input& input);
    static const wchar_t* transportName(Transport value);
    static const wchar_t* functionName(Function value);
    static const wchar_t* identityName(IdentityLevel value);
};
} // namespace unify::universal
