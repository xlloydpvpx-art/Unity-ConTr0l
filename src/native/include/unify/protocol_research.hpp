#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace unify {
struct ProtocolResearchItem {
    std::wstring id;
    std::wstring manufacturer;
    std::wstring modelFamily;
    std::wstring transport;
    std::wstring status;
    std::wstring evidenceLevel;
    std::wstring notes;
    std::vector<std::wstring> sources;
};

class ProtocolResearchCenter {
public:
    bool load(const std::filesystem::path& file);
    std::size_t itemCount() const { return items_.size(); }
    std::size_t verifiedCount() const;
    const std::wstring& loadStatus() const { return loadStatus_; }
    const std::vector<ProtocolResearchItem>& items() const { return items_; }
    std::wstring toJson() const;
private:
    std::vector<ProtocolResearchItem> items_;
    std::wstring loadStatus_{L"NOT LOADED"};
};
}
