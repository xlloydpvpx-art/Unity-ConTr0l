#include "unify/standards.hpp"

namespace unify::standards {

std::vector<Match> detect(std::uint16_t vid, std::uint16_t /*pid*/, std::uint16_t usagePage, std::uint16_t usage) {
    std::vector<Match> out;
    // Generic Desktop keyboard (0x06) / mouse (0x02) / pointer (0x01): Windows settings apply.
    if (usagePage == 0x01 && (usage == 0x06 || usage == 0x02 || usage == 0x01))
        out.push_back({L"WINDOWS_HID", Support::AppliesNow});
    // QMK/VIA raw HID: usage page 0xFF60, usage 0x61 (qmk_firmware RAW_USAGE_PAGE / RAW_USAGE_ID).
    if (usagePage == 0xFF60 && usage == 0x61)
        out.push_back({L"VIA_RAW_HID", Support::ReadOnlyProbe});
    // HID LampArray (usage page 0x59): the standard behind Windows "Dynamic Lighting".
    if (usagePage == 0x59)
        out.push_back({L"HID_LAMPARRAY", Support::DetectedNotImplemented});
    // Logitech HID++ travels over vendor pages 0xFF00 / 0xFF43 on Logitech devices (VID 0x046D).
    if (vid == 0x046D && (usagePage == 0xFF00 || usagePage == 0xFF43))
        out.push_back({L"LOGITECH_HIDPP", Support::DetectedNotImplemented});
    // Any other vendor-defined collection is a private channel.
    if (usagePage >= 0xFF00 && out.empty())
        out.push_back({L"VENDOR_SPECIFIC", Support::NeedsDocumentation});
    return out;
}

const wchar_t* supportName(Support support) {
    switch (support) {
    case Support::AppliesNow: return L"APPLIES_NOW";
    case Support::ReadOnlyProbe: return L"READ_ONLY_PROBE";
    case Support::DetectedNotImplemented: return L"DETECTED_NOT_IMPLEMENTED";
    case Support::NeedsDocumentation: return L"NEEDS_DOCUMENTATION";
    }
    return L"UNKNOWN";
}

std::wstring toJson(const std::vector<Match>& matches) {
    std::wstring out = L"[";
    for (std::size_t i = 0; i < matches.size(); ++i) {
        if (i) out += L",";
        out += L"{\"code\":\"" + std::wstring(matches[i].code) + L"\",\"support\":\"" + supportName(matches[i].support) + L"\"}";
    }
    return out + L"]";
}

} // namespace unify::standards
