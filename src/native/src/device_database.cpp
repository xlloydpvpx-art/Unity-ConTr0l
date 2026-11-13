#include "unify/device_database.hpp"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <codecvt>
#include <locale>
#include <map>
#include <set>
#include <tuple>
#ifdef _WIN32
#include <windows.h>
#endif

namespace unify {
namespace {
std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}
std::wstring unescape(const std::wstring& s) {
    std::wstring out; bool esc=false;
    for (wchar_t c : s) {
        if (esc) { if(c==L'n') out+=L'\n'; else if(c==L'r') out+=L'\r'; else if(c==L't') out+=L'\t'; else out+=c; esc=false; }
        else if(c==L'\\') esc=true; else out+=c;
    }
    if(esc) out+=L'\\';
    return out;
}
std::vector<std::wstring> objectsIn(const std::wstring& json, const std::wstring& arrayName) {
    std::vector<std::wstring> result;
    const auto key=json.find(L"\""+arrayName+L"\""); if(key==std::wstring::npos) return result;
    const auto start=json.find(L'[',key); if(start==std::wstring::npos) return result;
    bool inString=false, escaped=false; int depth=0; std::size_t objectStart=std::wstring::npos;
    for(std::size_t i=start+1;i<json.size();++i) {
        const wchar_t c=json[i];
        if(inString) { if(escaped) escaped=false; else if(c==L'\\') escaped=true; else if(c==L'"') inString=false; continue; }
        if(c==L'"') { inString=true; continue; }
        if(c==L'{') { if(depth++==0) objectStart=i; }
        else if(c==L'}' && depth>0 && --depth==0 && objectStart!=std::wstring::npos) result.push_back(json.substr(objectStart,i-objectStart+1));
        else if(c==L']' && depth==0) break;
    }
    return result;
}
// Manual scanner: MSVC std::regex throws regex_error(error_stack) on long strings
// (its backtracking stack is capped at ~1000 frames), which terminated the process.
std::wstring rawStringField(const std::wstring& obj,const std::wstring& key) {
    const std::wstring needle=L"\""+key+L"\"";
    for(std::size_t p=obj.find(needle); p!=std::wstring::npos; p=obj.find(needle,p+1)) {
        std::size_t i=p+needle.size();
        while(i<obj.size() && iswspace(obj[i])) ++i;
        if(i>=obj.size() || obj[i]!=L':') continue;
        ++i;
        while(i<obj.size() && iswspace(obj[i])) ++i;
        if(i>=obj.size() || obj[i]!=L'"') continue;
        ++i;
        std::wstring out;
        for(; i<obj.size(); ++i) {
            if(obj[i]==L'\\') { out+=obj[i]; if(++i>=obj.size()) return {}; out+=obj[i]; }
            else if(obj[i]==L'"') return out;
            else out+=obj[i];
        }
        return {}; // unterminated string: same result as a failed regex match
    }
    return {};
}
std::wstring stringField(const std::wstring& obj,const std::wstring& key) {
    return unescape(rawStringField(obj,key));
}
unsigned long long numberField(const std::wstring& obj,const std::wstring& key,unsigned long long fallback=0) {
    const std::wregex re(L"\\\""+key+L"\\\"\\s*:\\s*(0[xX][0-9a-fA-F]+|[0-9]+)"); std::wsmatch m;
    try {
        if(!std::regex_search(obj,m,re)) return fallback;
        const auto token=m[1].str();
        const bool hex=token.size()>2 && token[0]==L'0' && (token[1]==L'x'||token[1]==L'X');
        return std::stoull(token,nullptr,hex?16:10);
    } catch(...) { return fallback; }
}
std::vector<std::wstring> quotedStrings(const std::wstring& part) {
    std::vector<std::wstring> out;
    for(std::size_t i=0;i<part.size();) {
        if(part[i]!=L'"') { ++i; continue; }
        std::wstring cur; std::size_t j=i+1; bool closed=false;
        for(;j<part.size();++j) {
            if(part[j]==L'\\') { cur+=part[j]; if(++j>=part.size()) break; cur+=part[j]; }
            else if(part[j]==L'"') { closed=true; break; }
            else cur+=part[j];
        }
        if(!closed) break;
        out.push_back(std::move(cur)); i=j+1;
    }
    return out;
}
std::vector<std::wstring> stringArray(const std::wstring& obj,const std::wstring& key) {
    std::vector<std::wstring> out; const auto p=obj.find(L"\""+key+L"\""); if(p==std::wstring::npos) return out;
    const auto a=obj.find(L'[',p), b=obj.find(L']',a); if(a==std::wstring::npos||b==std::wstring::npos) return out;
    for(auto& raw:quotedStrings(obj.substr(a,b-a+1))) out.push_back(unescape(raw));
    return out;
}
std::wstring fromUtf8(const std::string& bytes) {
#ifdef _WIN32
    if(bytes.empty()) return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
    if(n<=0) return {};
    std::wstring out(static_cast<std::size_t>(n),L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),out.data(),n)) return {};
    return out;
#else
    try { std::wstring_convert<std::codecvt_utf8<wchar_t>> cv; return cv.from_bytes(bytes); }
    catch(...) { return {}; }
#endif
}
bool hasWholeName(const std::wstring& source,const std::wstring& needle) {
    const auto a=lower(source), b=lower(needle); if(b.empty()) return false;
    std::size_t p=a.find(b);
    while(p!=std::wstring::npos) {
        const bool left=p==0 || !std::iswalnum(a[p-1]);
        const auto end=p+b.size(); const bool right=end==a.size() || !std::iswalnum(a[end]);
        if(left&&right) return true;
        p=a.find(b,p+1);
    }
    return false;
}
}
bool DeviceDatabase::load(const std::filesystem::path& file) {
    manufacturers_.clear(); devices_.clear(); rejectedVerifiedRecords_=0; conflictingModelKeys_=0;
    std::ifstream in(file,std::ios::binary);
    if(!in) { loadStatus_=L"FILE NOT FOUND"; return false; }
    std::string bytes((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    const auto json=fromUtf8(bytes);
    if(json.empty() || numberField(json,L"schemaVersion",0)!=3) { loadStatus_=L"INVALID OR UNSUPPORTED SCHEMA"; return false; }
    for(const auto& obj:objectsIn(json,L"manufacturers")) {
        ManufacturerRecord r; r.name=stringField(obj,L"name"); r.aliases=stringArray(obj,L"aliases"); r.status=stringField(obj,L"status");
        if(!r.name.empty()) manufacturers_.push_back(std::move(r));
    }
    for(const auto& obj:objectsIn(json,L"devices")) {
        const auto status=stringField(obj,L"status");
        if(status!=L"VERIFIED") continue;
        const auto vid=numberField(obj,L"vid",65536), pid=numberField(obj,L"pid",65536);
        const auto rev=obj.find(L"\"revision\"");
        const auto revision=numberField(obj,L"revision",65536);
        DeviceRecord r; r.manufacturer=stringField(obj,L"manufacturer"); r.model=stringField(obj,L"model");
        r.evidence=stringField(obj,L"evidence"); r.status=status; r.revisionSpecified=rev!=std::wstring::npos;
        for (const auto& capObj : objectsIn(obj, L"capabilities")) {
            CapabilityRecord cap; cap.name=stringField(capObj,L"name"); cap.operation=stringField(capObj,L"operation");
            cap.status=stringField(capObj,L"status"); cap.evidence=stringField(capObj,L"evidence"); cap.implementation=stringField(capObj,L"implementation");
            r.capabilities.push_back(std::move(cap));
        }
        if(vid==0 || vid>65535 || pid==0 || pid>65535 || (r.revisionSpecified && revision>65535) || r.manufacturer.empty() || r.model.empty() || r.evidence.empty()) {
            ++rejectedVerifiedRecords_; continue;
        }
        r.vid=static_cast<std::uint16_t>(vid); r.pid=static_cast<std::uint16_t>(pid);
        if(r.revisionSpecified) r.revision=static_cast<std::uint16_t>(revision);
        devices_.push_back(std::move(r));
    }
    using Key=std::tuple<std::uint16_t,std::uint16_t,bool,std::uint16_t>;
    std::map<Key,std::set<std::wstring>> keys;
    for(const auto& r:devices_) keys[{r.vid,r.pid,r.revisionSpecified,r.revision}].insert(r.manufacturer+L" / "+r.model);
    for(const auto& entry:keys) if(entry.second.size()>1) ++conflictingModelKeys_;
    loadStatus_=L"LOADED";
    if(rejectedVerifiedRecords_ || conflictingModelKeys_) loadStatus_=L"LOADED WITH WARNINGS";
    return true;
}
void DeviceDatabase::identify(DeviceInfo& d) const {
    d.identifiedManufacturer.clear(); d.identifiedModel.clear(); d.identificationEvidence.clear(); d.capabilitySummary.clear(); d.identificationStatus=L"UNKNOWN";
    // Exact VID/PID/revision entries take precedence; unverified records are never loaded.
    const DeviceRecord* best=nullptr;
    bool ambiguous=false;
    for(const auto& r:devices_) if(r.vid==d.vid && r.pid==d.pid && (!r.revisionSpecified || r.revision==d.version)) {
        if(!best || (r.revisionSpecified && !best->revisionSpecified)) { best=&r; ambiguous=false; }
        else if(r.revisionSpecified==best->revisionSpecified && (r.manufacturer!=best->manufacturer || r.model!=best->model)) ambiguous=true;
    }
    if(best && ambiguous) { d.identificationStatus=L"AMBIGUOUS DATABASE MATCH"; return; }
    if(best) {
        d.identifiedManufacturer=best->manufacturer; d.identifiedModel=best->model; d.identificationStatus=L"EXACT MODEL MATCH"; d.identificationEvidence=best->evidence;
        d.capabilitySummary.clear();
        for (const auto& cap : CapabilityEngine::resolve(best->capabilities)) d.capabilitySummary.push_back(cap.name + L" / " + cap.operation + L" / " + cap.state);
        return;
    }
    const auto observed=lower(d.manufacturer);
    std::set<std::wstring> exactNames, exactAliasNames, partialNames, partialAliasNames;
    for(const auto& r:manufacturers_) {
        if(observed==lower(r.name)) exactNames.insert(r.name);
        for(const auto& alias:r.aliases) {
            if(observed==lower(alias)) exactAliasNames.insert(r.name);
            if(hasWholeName(d.manufacturer,alias)) partialAliasNames.insert(r.name);
        }
        if(hasWholeName(d.manufacturer,r.name)) partialNames.insert(r.name);
    }
    const auto choose=[&](const std::set<std::wstring>& candidates,const wchar_t* status)->bool {
        if(candidates.size()>1) { d.identificationStatus=L"AMBIGUOUS MANUFACTURER MATCH"; return true; }
        if(candidates.size()==1) { d.identifiedManufacturer=*candidates.begin(); d.identificationStatus=status; return true; }
        return false;
    };
    if(choose(exactNames,L"MANUFACTURER CATALOG MATCH")) return;
    if(choose(exactAliasNames,L"MANUFACTURER ALIAS MATCH")) return;
    if(choose(partialNames,L"MANUFACTURER CATALOG MATCH")) return;
    if(choose(partialAliasNames,L"MANUFACTURER ALIAS MATCH")) return;
    if(d.vid || d.pid) d.identificationStatus=L"UNMATCHED VID/PID";
}
}
