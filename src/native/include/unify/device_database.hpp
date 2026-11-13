#pragma once
#include "unify/device_scanner.hpp"
#include "unify/capability_engine.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace unify {
struct ManufacturerRecord {
    std::wstring name;
    std::vector<std::wstring> aliases;
    std::wstring status;
};
struct DeviceRecord {
    std::uint16_t vid{};
    std::uint16_t pid{};
    std::uint16_t revision{};
    bool revisionSpecified{};
    std::wstring manufacturer;
    std::wstring model;
    std::wstring evidence;
    std::wstring status;
    std::vector<CapabilityRecord> capabilities;
};
class DeviceDatabase {
public:
    bool load(const std::filesystem::path& file);
    void identify(DeviceInfo& device) const;
    std::size_t manufacturerCount() const { return manufacturers_.size(); }
    std::size_t verifiedDeviceCount() const { return devices_.size(); }
    std::size_t rejectedVerifiedRecords() const { return rejectedVerifiedRecords_; }
    std::size_t conflictingModelKeys() const { return conflictingModelKeys_; }
    const std::wstring& loadStatus() const { return loadStatus_; }
private:
    std::vector<ManufacturerRecord> manufacturers_;
    std::vector<DeviceRecord> devices_;
    std::wstring loadStatus_{L"NOT LOADED"};
    std::size_t rejectedVerifiedRecords_{};
    std::size_t conflictingModelKeys_{};
};
}
