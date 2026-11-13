#include "unify/hid_report_parser.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <limits>

namespace unify::hid {
namespace {
struct GlobalState {
    std::uint32_t usagePage{}, reportSize{}, reportCount{};
    std::uint8_t reportId{};
};
struct LocalState {
    std::vector<std::uint32_t> usages;
    std::uint32_t usageMin{}, usageMax{};
    bool hasUsageMin{false};
};
enum class KindKey : std::size_t { Input=0, Output=1, Feature=2 };
std::uint32_t unsignedValue(const std::uint8_t* p, std::size_t n) {
    std::uint32_t v=0; for(std::size_t i=0;i<n;++i) v |= static_cast<std::uint32_t>(p[i]) << (8*i); return v;
}
void fail(ParseResult& r, const wchar_t* message) { r.valid=false; r.status=L"INVALID_DESCRIPTOR"; r.error=message; }
}
ParseResult ReportDescriptorParser::parse(const std::vector<std::uint8_t>& b) {
    ParseResult r;
    if (b.empty()) { fail(r,L"Descriptor is empty."); return r; }
    if (b.size()>MaxDescriptorBytes) { fail(r,L"Descriptor exceeds the 4096-byte safety limit."); return r; }
    GlobalState g; std::vector<GlobalState> globals; LocalState local;
    std::array<std::array<std::uint64_t,256>,3> bits{};
    std::uint32_t collectionDepth=0; bool sawImplicitReport=false, sawExplicitReport=false;
    std::size_t i=0;
    while(i<b.size()) {
        const auto prefix=b[i++];
        if(prefix==0xFE) { // Long item: skip only after strict bounds validation.
            if(i+2>b.size()) { fail(r,L"Truncated long-item header."); return r; }
            const auto length=b[i++]; ++i; // long-item tag
            if(length>b.size()-i) { fail(r,L"Long item extends beyond descriptor bounds."); return r; }
            i+=length; ++r.itemCount; continue;
        }
        const std::size_t sizeCode=prefix&0x03;
        const std::size_t dataSize=sizeCode==3?4:sizeCode;
        const auto type=(prefix>>2)&0x03; const auto tag=(prefix>>4)&0x0F;
        if(dataSize>b.size()-i) { fail(r,L"Truncated short item."); return r; }
        const auto value=unsignedValue(b.data()+i,dataSize); i+=dataSize; ++r.itemCount;
        if(type==1) { // Global
            switch(tag) {
            case 0: g.usagePage=value; break;
            case 7: g.reportSize=value; break;
            case 8:
                if(value==0 || value>255) { fail(r,L"Report ID must be in the range 1..255."); return r; }
                g.reportId=static_cast<std::uint8_t>(value); break;
            case 9: g.reportCount=value; break;
            case 10:
                if(globals.size()>=16) { fail(r,L"Global PUSH nesting limit exceeded."); return r; }
                globals.push_back(g); break;
            case 11:
                if(globals.empty()) { fail(r,L"Global POP without matching PUSH."); return r; }
                g=globals.back(); globals.pop_back(); break;
            default: break;
            }
        } else if(type==2) { // Local
            switch(tag) {
            case 0: if(local.usages.size()<128) local.usages.push_back(value); break;
            case 1: local.usageMin=value; local.hasUsageMin=true; break;
            case 2: local.usageMax=value; break;
            default: break;
            }
        } else if(type==0) { // Main
            if(tag==8 || tag==9 || tag==11) {
                if(g.reportSize>4096 || g.reportCount>4096 || (g.reportSize && g.reportCount>65536/g.reportSize)) {
                    fail(r,L"Report size/count exceeds parser safety limits."); return r;
                }
                if(g.reportId==0) sawImplicitReport=true; else sawExplicitReport=true;
                if(sawImplicitReport && sawExplicitReport) { fail(r,L"Descriptor mixes implicit and explicit Report IDs."); return r; }
                const auto kind=tag==8?KindKey::Input:(tag==9?KindKey::Output:KindKey::Feature);
                auto& total=bits[static_cast<std::size_t>(kind)][g.reportId];
                const std::uint64_t add=static_cast<std::uint64_t>(g.reportSize)*g.reportCount;
                if(total+add>8u*1024u*1024u) { fail(r,L"Aggregate report bit length exceeds safety limit."); return r; }
                total+=add;
            } else if(tag==10) { // Collection
                if(collectionDepth>=32) { fail(r,L"Collection nesting limit exceeded."); return r; }
                const auto usage=!local.usages.empty()?local.usages.front():(local.hasUsageMin?local.usageMin:0);
                const auto usagePage=(usage>0xffffu)?(usage>>16):g.usagePage;
                CollectionSummary c; c.usagePage=static_cast<std::uint16_t>(usagePage&0xffff); c.usage=static_cast<std::uint16_t>(usage&0xffff); c.type=static_cast<std::uint8_t>(value&0xff); c.depth=collectionDepth;
                r.collections.push_back(c);
                if(collectionDepth==0 && r.topLevelUsagePage==0) { r.topLevelUsagePage=c.usagePage; r.topLevelUsage=c.usage; }
                ++collectionDepth;
            } else if(tag==12) {
                if(collectionDepth==0) { fail(r,L"END_COLLECTION without matching COLLECTION."); return r; }
                --collectionDepth;
            }
            local=LocalState{}; // Local state is cleared after every Main item.
        }
    }
    if(collectionDepth!=0) { fail(r,L"Unclosed HID collection."); return r; }
    if(!globals.empty()) { fail(r,L"Unbalanced Global PUSH/POP state."); return r; }
    for(std::size_t k=0;k<3;++k) for(std::size_t id=0;id<256;++id) if(bits[k][id]) {
        ReportSummary s; s.reportId=static_cast<std::uint8_t>(id); s.kind=static_cast<ReportKind>(k);
        s.bitLength=static_cast<std::uint32_t>(bits[k][id]); s.byteLength=static_cast<std::uint32_t>((bits[k][id]+7)/8)+(id?1u:0u);
        r.reports.push_back(s);
    }
    r.valid=true; r.status=L"PARSED"; r.error.clear(); return r;
}
} // namespace unify::hid
