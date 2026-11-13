#include "unify/usb_descriptor_parser.hpp"
#include <algorithm>
#include <set>
namespace unify::usb {
namespace {
std::uint16_t le16(const std::vector<std::uint8_t>& b,std::size_t p){return static_cast<std::uint16_t>(b[p] | (static_cast<std::uint16_t>(b[p+1])<<8));}
void fail(ConfigurationResult& r,const wchar_t* e){r.valid=false;r.error=e;}
}
DeviceDescriptor DescriptorParser::parseDevice(const std::vector<std::uint8_t>& b) {
    DeviceDescriptor d;
    if(b.size()!=18 || b[0]!=18 || b[1]!=DescriptorDevice){d.error=L"A USB device descriptor must be exactly 18 bytes with descriptor type 0x01.";return d;}
    if(b[7]==0){d.error=L"Endpoint zero maximum packet size is invalid.";return d;}
    d.usbVersion=le16(b,2);d.deviceClass=b[4];d.deviceSubClass=b[5];d.deviceProtocol=b[6];d.maxPacketSize0=b[7];
    d.vendorId=le16(b,8);d.productId=le16(b,10);d.deviceVersion=le16(b,12);d.configurations=b[17];
    if(d.configurations==0){d.error=L"USB device descriptor declares no configurations.";return d;}
    d.valid=true;return d;
}
ConfigurationResult DescriptorParser::parseConfiguration(const std::vector<std::uint8_t>& b) {
    ConfigurationResult r;
    if(b.size()<9){fail(r,L"Configuration descriptor is shorter than its 9-byte header.");return r;}
    if(b.size()>MaxDescriptorBytes){fail(r,L"USB descriptor blob exceeds 65535-byte safety limit.");return r;}
    if(b[0]<9 || b[1]!=DescriptorConfiguration){fail(r,L"Configuration descriptor header is invalid.");return r;}
    r.totalLength=le16(b,2);r.declaredInterfaces=b[4];r.configurationValue=b[5];
    if(r.configurationValue==0 || r.declaredInterfaces==0){fail(r,L"Configuration value and declared interface count must be nonzero.");return r;}
    if((b[7]&0x80)==0 || (b[7]&0x1f)!=0){fail(r,L"Configuration bmAttributes contains invalid reserved bits.");return r;}
    if(r.totalLength<9 || r.totalLength>b.size()){fail(r,L"wTotalLength is outside the supplied descriptor buffer.");return r;}
    if(r.totalLength!=b.size()){fail(r,L"Supplied buffer length does not match wTotalLength.");return r;}
    InterfaceDescriptor* current=nullptr;
    std::size_t p=0;
    while(p<r.totalLength){
        if(r.totalLength-p<2){fail(r,L"Trailing descriptor header is truncated.");return r;}
        const auto length=b[p],type=b[p+1];
        if(length<2 || length>r.totalLength-p){fail(r,L"Descriptor has invalid bLength or extends beyond wTotalLength.");return r;}
        ++r.descriptorCount;
        if(p==0 && (type!=DescriptorConfiguration || length<9)){fail(r,L"First descriptor is not a valid configuration descriptor.");return r;}
        if(type==DescriptorInterface){
            if(length<9){fail(r,L"Interface descriptor is shorter than 9 bytes.");return r;}
            InterfaceDescriptor item;item.number=b[p+2];item.alternateSetting=b[p+3];item.declaredEndpoints=b[p+4];item.interfaceClass=b[p+5];item.interfaceSubClass=b[p+6];item.interfaceProtocol=b[p+7];
            r.interfaces.push_back(std::move(item));current=&r.interfaces.back();
        } else if(type==DescriptorEndpoint){
            if(length<7 || !current){fail(r,L"Endpoint descriptor is malformed or appears before an interface descriptor.");return r;}
            EndpointDescriptor ep;ep.address=b[p+2];ep.attributes=b[p+3];ep.maxPacketSize=le16(b,p+4);ep.interval=b[p+6];
            if((ep.address&0x70)!=0 || (ep.address&0x0f)==0){fail(r,L"Endpoint address contains reserved bits or endpoint number zero.");return r;}
            if(ep.maxPacketSize==0){fail(r,L"Endpoint maximum packet size cannot be zero.");return r;}
            current->endpoints.push_back(ep);
        } else if(type==DescriptorConfiguration && p!=0){
            fail(r,L"Unexpected nested configuration descriptor.");return r;
        }
        p+=length;
    }
    std::set<std::uint8_t> interfaceNumbers;
    for(const auto& interface:r.interfaces) interfaceNumbers.insert(interface.number);
    if(interfaceNumbers.size()!=r.declaredInterfaces){fail(r,L"Number of distinct interface numbers does not match bNumInterfaces.");return r;}
    for(const auto& interface:r.interfaces) if(interface.endpoints.size()!=interface.declaredEndpoints){fail(r,L"Endpoint descriptor count does not match bNumEndpoints for an interface alternate setting.");return r;}
    r.valid=true;r.error.clear();return r;
}
} // namespace unify::usb
