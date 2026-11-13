#include "unify/observation_store.hpp"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace unify {
namespace {
std::string utf8(const std::wstring& w) {
    if(w.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w.data(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);
    if(n<=0) return {};
    std::string s(n,'\0'); WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w.data(),static_cast<int>(w.size()),s.data(),n,nullptr,nullptr); return s;
}
std::string esc(const std::wstring& w) {
    const auto s=utf8(w); std::ostringstream o;
    for(unsigned char c:s) { if(c=='"') o<<"\\\""; else if(c=='\\') o<<"\\\\"; else if(c=='\n') o<<"\\n"; else if(c=='\r') o<<"\\r"; else if(c<0x20) o<<' '; else o<<c; }
    return o.str();
}
}
bool ObservationStore::saveUnknown(const std::vector<DeviceInfo>& devices,const std::filesystem::path& file) {
    std::error_code ec; std::filesystem::create_directories(file.parent_path(),ec);
    auto tmp=file; tmp += L".tmp";
    std::ofstream out(tmp,std::ios::binary|std::ios::trunc); if(!out) return false;
    out << "{\"schemaVersion\":1,\"policy\":\"Local observations only; identity does not imply protocol support\",\"observations\":[";
    bool first=true;
    for(const auto& d:devices) {
        if(!d.vid && !d.pid) continue;
        if(!first) out<<','; first=false;
        out << "{\"vid\":\"" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << d.vid
            << "\",\"pid\":\"" << std::setw(4) << d.pid << std::dec << "\",\"manufacturer\":\"" << esc(d.manufacturer)
            << "\",\"product\":\"" << esc(d.product)
            << "\",\"containerId\":\"" << esc(d.containerId) << "\",\"status\":\"DETECTED ONLY\"}";
    }
    out << "]}"; out.close(); if(!out) { std::filesystem::remove(tmp,ec); return false; }
    return MoveFileExW(tmp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
}
