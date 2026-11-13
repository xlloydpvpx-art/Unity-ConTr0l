#pragma once
// Layer 2 of the universal control model: recognise OPEN or well-documented configuration standards
// from what Windows already reports (VID/PID and the HID top-level usage). Recognition never
// authorises I/O by itself; each standard states what the app can actually do with it today.
#include <cstdint>
#include <string>
#include <vector>

namespace unify::standards {

enum class Support {
    AppliesNow,             // works today through documented OS APIs (layer 1)
    ReadOnlyProbe,          // the app can verify the protocol with read-only requests
    DetectedNotImplemented, // a documented standard exists but this build does not speak it yet
    NeedsDocumentation      // vendor channel: needs vendor docs or a protocol capture
};

struct Match {
    const wchar_t* code;
    Support support;
};

std::vector<Match> detect(std::uint16_t vid, std::uint16_t pid, std::uint16_t usagePage, std::uint16_t usage);
const wchar_t* supportName(Support support);
std::wstring toJson(const std::vector<Match>& matches);

} // namespace unify::standards
