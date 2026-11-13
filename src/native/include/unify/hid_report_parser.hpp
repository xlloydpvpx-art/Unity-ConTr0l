#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace unify::hid {
enum class ReportKind { Input, Output, Feature };
struct ReportSummary {
    std::uint8_t reportId{};
    ReportKind kind{ReportKind::Input};
    std::uint32_t bitLength{};
    std::uint32_t byteLength{};
};
struct CollectionSummary {
    std::uint16_t usagePage{}, usage{};
    std::uint8_t type{};
    std::uint32_t depth{};
};
struct ParseResult {
    bool valid{false};
    std::wstring status{L"NOT_PARSED"};
    std::wstring error;
    std::uint16_t topLevelUsagePage{}, topLevelUsage{};
    std::vector<ReportSummary> reports;
    std::vector<CollectionSummary> collections;
    std::size_t itemCount{};
};
// Parses a raw HID report descriptor supplied by a trusted OS/backend capture.
// This function never opens a device and never sends reports.
class ReportDescriptorParser {
public:
    static constexpr std::size_t MaxDescriptorBytes = 4096;
    static ParseResult parse(const std::vector<std::uint8_t>& bytes);
};
} // namespace unify::hid
