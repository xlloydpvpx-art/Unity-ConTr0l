#pragma once
#include "unify/device_scanner.hpp"
#include <filesystem>
namespace unify {
class ObservationStore {
public:
    // Writes a local UTF-8 snapshot of observed USB/HID identities. It never sends device commands.
    static bool saveUnknown(const std::vector<DeviceInfo>& devices, const std::filesystem::path& file);
};
}
