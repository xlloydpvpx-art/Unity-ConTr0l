#pragma once
// Layer 1 of the universal control model: documented, reversible Windows settings that apply to
// every keyboard and mouse regardless of vendor. Nothing here talks to device firmware.
#include <string>
#include <vector>

namespace unify::winsettings {

struct Value { std::wstring key; int value{}; };

bool isKnownKey(const std::wstring& key);
std::vector<Value> readAll();
// Validates the range, applies the setting through the Win32 API and verifies it by reading it back.
// On failure `error` holds a short ASCII reason (safe to embed in JSON).
bool write(const std::wstring& key, int value, std::wstring& error);
std::wstring toJson(const std::vector<Value>& values);

} // namespace unify::winsettings
