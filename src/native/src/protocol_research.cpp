#include "unify/protocol_research.hpp"
#include <algorithm>
#include <codecvt>
#include <fstream>
#include <locale>
#include <regex>
#include <cwctype>
#ifdef _WIN32
#include <windows.h>
#endif
namespace unify { namespace {
constexpr wchar_t BS = static_cast<wchar_t>(92);
std::wstring utf8ToWide(const std::string& s) {
#ifdef _WIN32
    if(s.empty()) return {}; int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0); if(n<=0)return {};
    std::wstring out(static_cast<std::size_t>(n),L'\0'); if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n))return {}; return out;
#else
    try { std::wstring_convert<std::codecvt_utf8<wchar_t>> cv; return cv.from_bytes(s); } catch(...) { return {}; }
#endif
}
std::wstring unescape(const std::wstring& s){std::wstring o;bool e=false;for(auto c:s){if(e){if(c==L'n')o+=L' ';else if(c==L't')o+=L' ';else o+=c;e=false;}else if(c==BS)e=true;else o+=c;}return o;}
// Manual scanner: MSVC std::regex throws regex_error(error_stack) on long strings
// (its backtracking stack is capped at ~1000 frames), which terminated the process.
std::wstring rawField(const std::wstring& obj,const std::wstring& key) {
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
std::wstring field(const std::wstring& obj,const std::wstring& key){return unescape(rawField(obj,key));}
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
std::vector<std::wstring> strings(const std::wstring& obj,const std::wstring& key){std::vector<std::wstring> out;auto p=obj.find(L"\""+key+L"\"");if(p==std::wstring::npos)return out;auto a=obj.find(L'[',p),b=obj.find(L']',a);if(a==std::wstring::npos||b==std::wstring::npos)return out;for(auto& raw:quotedStrings(obj.substr(a,b-a+1)))out.push_back(unescape(raw));return out;}
std::vector<std::wstring> objects(const std::wstring& json,const std::wstring& key){std::vector<std::wstring> out;auto p=json.find(L"\""+key+L"\"");if(p==std::wstring::npos)return out;auto a=json.find(L'[',p);if(a==std::wstring::npos)return out;int depth=0;bool str=false,esc=false;std::size_t begin=0;for(std::size_t i=a+1;i<json.size();++i){auto c=json[i];if(str){if(esc)esc=false;else if(c==BS)esc=true;else if(c==L'\"')str=false;continue;}if(c==L'\"'){str=true;continue;}if(c==L'{'){if(depth++==0)begin=i;}else if(c==L'}'&&depth>0&&--depth==0)out.push_back(json.substr(begin,i-begin+1));else if(c==L']'&&depth==0)break;}return out;}
std::wstring escape(const std::wstring& s){std::wstring o;for(auto c:s){if(c==BS){o+=BS;o+=BS;}else if(c==L'\"'){o+=BS;o+=L'\"';}else if(c==L'\n'){o+=BS;o+=L'n';}else if(c<0x20)o+=L' ';else o+=c;}return o;}
}
bool ProtocolResearchCenter::load(const std::filesystem::path& file){items_.clear();std::ifstream in(file,std::ios::binary);if(!in){loadStatus_=L"FILE NOT FOUND";return false;}std::string bytes((std::istreambuf_iterator<char>(in)),{});auto json=utf8ToWide(bytes);if(json.empty()||json.find(L"schemaVersion")==std::wstring::npos||json.find(L"researchItems")==std::wstring::npos){loadStatus_=L"INVALID RESEARCH CATALOG";return false;}const std::vector<std::wstring> allowed={L"RESEARCH_REQUIRED",L"IN_RESEARCH",L"EVIDENCE_COLLECTED",L"VERIFIED",L"BLOCKED"};for(const auto& obj:objects(json,L"researchItems")){ProtocolResearchItem r;r.id=field(obj,L"id");r.manufacturer=field(obj,L"manufacturer");r.modelFamily=field(obj,L"modelFamily");r.transport=field(obj,L"transport");r.status=field(obj,L"status");r.evidenceLevel=field(obj,L"evidenceLevel");r.notes=field(obj,L"notes");r.sources=strings(obj,L"sources");if(r.id.empty()||r.manufacturer.empty()||std::find(allowed.begin(),allowed.end(),r.status)==allowed.end())continue;if(r.status==L"VERIFIED"&&(r.sources.empty()||r.evidenceLevel.empty()))continue;items_.push_back(std::move(r));}loadStatus_=L"LOADED";return true;}
std::size_t ProtocolResearchCenter::verifiedCount()const{return static_cast<std::size_t>(std::count_if(items_.begin(),items_.end(),[](const auto& x){return x.status==L"VERIFIED";}));}
std::wstring ProtocolResearchCenter::toJson()const{std::wstring o=L"[";for(std::size_t i=0;i<items_.size();++i){const auto&r=items_[i];if(i)o+=L',';o+=L"{\"id\":\""+escape(r.id)+L"\",\"manufacturer\":\""+escape(r.manufacturer)+L"\",\"modelFamily\":\""+escape(r.modelFamily)+L"\",\"transport\":\""+escape(r.transport)+L"\",\"status\":\""+escape(r.status)+L"\",\"evidenceLevel\":\""+escape(r.evidenceLevel)+L"\",\"notes\":\""+escape(r.notes)+L"\",\"sources\":[";for(std::size_t j=0;j<r.sources.size();++j){if(j)o+=L',';o+=L"\""+escape(r.sources[j])+L"\"";}o+=L"]}";}return o+L"]";}
}
