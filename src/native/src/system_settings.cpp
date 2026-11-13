#include "unify/system_settings.hpp"
#include <windows.h>
#include <climits>

namespace unify::winsettings {
namespace {
struct Def { const wchar_t* key; int minValue; int maxValue; };
constexpr Def kDefs[] = {
    {L"mouseSpeed", 1, 20},      // SPI_SETMOUSESPEED
    {L"mouseAccel", 0, 1},       // "Enhance pointer precision" (SPI_SETMOUSE)
    {L"swapButtons", 0, 1},      // SwapMouseButton
    {L"doubleClickMs", 200, 900},// SetDoubleClickTime
    {L"wheelLines", 1, 100},     // SPI_SETWHEELSCROLLLINES
    {L"keyDelay", 0, 3},         // SPI_SETKEYBOARDDELAY (0 = 250 ms ... 3 = 1 s)
    {L"keyRate", 0, 31},         // SPI_SETKEYBOARDSPEED (0 = ~2.5/s ... 31 = ~30/s)
};
const Def* find(const std::wstring& key) {
    for (const auto& d : kDefs) if (key == d.key) return &d;
    return nullptr;
}
bool readOne(const std::wstring& key, int& out) {
    if (key == L"mouseSpeed") { int v = 0; if (!SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &v, 0)) return false; out = v; return true; }
    if (key == L"mouseAccel") { int m[3] = {0, 0, 0}; if (!SystemParametersInfoW(SPI_GETMOUSE, 0, m, 0)) return false; out = m[2] != 0 ? 1 : 0; return true; }
    if (key == L"swapButtons") { out = GetSystemMetrics(SM_SWAPBUTTON) != 0 ? 1 : 0; return true; }
    if (key == L"doubleClickMs") { out = static_cast<int>(GetDoubleClickTime()); return true; }
    if (key == L"wheelLines") {
        UINT v = 0; if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &v, 0)) return false;
        out = (v == 0 || v > 100) ? 100 : static_cast<int>(v); // WHEEL_PAGESCROLL (UINT_MAX) is reported as the maximum
        return true;
    }
    if (key == L"keyDelay") { int v = 0; if (!SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &v, 0)) return false; out = v; return true; }
    if (key == L"keyRate") { DWORD v = 0; if (!SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &v, 0)) return false; out = static_cast<int>(v); return true; }
    return false;
}
bool applyOne(const std::wstring& key, int value) {
    constexpr UINT flags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;
    if (key == L"mouseSpeed") return SystemParametersInfoW(SPI_SETMOUSESPEED, 0, reinterpret_cast<PVOID>(static_cast<INT_PTR>(value)), flags) != FALSE;
    if (key == L"mouseAccel") {
        int m[3] = {0, 0, 0};
        if (value) { m[0] = 6; m[1] = 10; m[2] = 1; } // Windows defaults for "Enhance pointer precision"
        return SystemParametersInfoW(SPI_SETMOUSE, 0, m, flags) != FALSE;
    }
    if (key == L"swapButtons") { SwapMouseButton(value ? TRUE : FALSE); return true; }
    if (key == L"doubleClickMs") return SetDoubleClickTime(static_cast<UINT>(value)) != FALSE;
    if (key == L"wheelLines") return SystemParametersInfoW(SPI_SETWHEELSCROLLLINES, static_cast<UINT>(value), nullptr, flags) != FALSE;
    if (key == L"keyDelay") return SystemParametersInfoW(SPI_SETKEYBOARDDELAY, static_cast<UINT>(value), nullptr, flags) != FALSE;
    if (key == L"keyRate") return SystemParametersInfoW(SPI_SETKEYBOARDSPEED, static_cast<UINT>(value), nullptr, flags) != FALSE;
    return false;
}
} // namespace

bool isKnownKey(const std::wstring& key) { return find(key) != nullptr; }

std::vector<Value> readAll() {
    std::vector<Value> out;
    for (const auto& d : kDefs) { int v = 0; if (readOne(d.key, v)) out.push_back({d.key, v}); }
    return out;
}

bool write(const std::wstring& key, int value, std::wstring& error) {
    const Def* def = find(key);
    if (!def) { error = L"UNKNOWN_SETTING"; return false; }
    if (value < def->minValue || value > def->maxValue) { error = L"OUT_OF_RANGE"; return false; }
    if (!applyOne(key, value)) { error = L"WINDOWS_REJECTED"; return false; }
    int actual = 0;
    if (!readOne(key, actual) || actual != value) { error = L"READBACK_MISMATCH"; return false; }
    return true;
}

std::wstring toJson(const std::vector<Value>& values) {
    std::wstring out = L"{";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += L",";
        out += L"\"" + values[i].key + L"\":" + std::to_wstring(values[i].value);
    }
    return out + L"}";
}

} // namespace unify::winsettings
