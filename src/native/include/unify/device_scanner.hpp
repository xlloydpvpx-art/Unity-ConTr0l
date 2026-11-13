#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "unify/transport_metadata.hpp"

namespace unify {
struct DeviceInfo {
    std::wstring path;
    std::wstring usbInterfacePath;
    std::wstring hidDescriptorAnalysis{L"NOT_AVAILABLE"};
    std::vector<std::wstring> hidReportIds;
    std::vector<std::wstring> hidUsageDetails;
    std::vector<transport::UsbEndpoint> usbEndpoints;
    std::vector<transport::GattService> gattServices;
    std::wstring winUsbStatus{L"NOT_ATTEMPTED"};
    std::wstring gattStatus{L"NOT_ATTEMPTED"};
    std::wstring instanceId;
    std::wstring containerId;
    std::wstring parentInstanceId;
    std::wstring location;
    std::wstring manufacturer;
    std::wstring product;
    std::wstring serial;
    std::wstring type{L"Unknown HID"};
    std::wstring connection{L"USB/HID"};
    std::wstring compatibility{L"DETECTED ONLY"};
    std::wstring receiverHint{L"UNKNOWN"};
    std::wstring identifiedManufacturer;
    std::wstring identifiedModel;
    std::wstring identificationStatus{L"UNKNOWN"};
    std::wstring identificationEvidence;
    std::vector<std::wstring> capabilitySummary;
    std::uint16_t vid{};
    std::uint16_t pid{};
    std::uint16_t usagePage{};
    std::uint16_t usage{};
    std::uint16_t version{};
    std::uint16_t interfaceNumber{0xffff};
    std::uint16_t inputReportBytes{}, outputReportBytes{}, featureReportBytes{};
    std::uint16_t inputButtonCaps{}, outputButtonCaps{}, featureButtonCaps{};
    std::uint16_t inputValueCaps{}, outputValueCaps{}, featureValueCaps{};
    bool hidDescriptorReadable{false};
};
struct DeviceGroup {
    std::wstring id;
    std::wstring containerId;
    std::wstring displayName;
    std::wstring compatibility{L"DETECTED ONLY"};
    std::wstring receiverHint{L"UNKNOWN"};
    std::wstring identifiedManufacturer;
    std::wstring identifiedModel;
    std::wstring identificationStatus{L"UNKNOWN"};
    std::wstring identificationEvidence;
    std::vector<DeviceInfo> interfaces;
};
class DeviceScanner {
public:
    std::vector<DeviceInfo> scan() const;
    static std::vector<DeviceGroup> group(const std::vector<DeviceInfo>& devices);
    static std::wstring toJson(const std::vector<DeviceInfo>& devices, const std::wstring& observationsPath = L"", std::size_t manufacturerCount = 0, std::size_t verifiedDeviceCount = 0, const std::wstring& databaseStatus = L"NOT LOADED");
};
}
