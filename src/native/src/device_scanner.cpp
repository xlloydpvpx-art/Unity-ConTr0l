#include "unify/device_scanner.hpp"
#include "unify/windows_transport_discovery.hpp"
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <usbiodef.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "cfgmgr32.lib")

namespace unify {
namespace {
std::wstring getDevProperty(HDEVINFO set, SP_DEVINFO_DATA& dev, DWORD property) {
    DWORD type=0, bytes=0; SetupDiGetDeviceRegistryPropertyW(set,&dev,property,&type,nullptr,0,&bytes);
    if (!bytes) return L"";
    std::vector<BYTE> b(bytes+sizeof(wchar_t));
    if (!SetupDiGetDeviceRegistryPropertyW(set,&dev,property,&type,b.data(),bytes,nullptr)) return L"";
    return reinterpret_cast<const wchar_t*>(b.data());
}
std::wstring getInstanceId(HDEVINFO set, SP_DEVINFO_DATA& dev) {
    DWORD n=0; SetupDiGetDeviceInstanceIdW(set,&dev,nullptr,0,&n); if(!n) return L"";
    std::vector<wchar_t> b(n+1); return SetupDiGetDeviceInstanceIdW(set,&dev,b.data(),static_cast<DWORD>(b.size()),nullptr) ? b.data() : L"";
}
std::wstring readHidString(HANDLE h, BOOLEAN (WINAPI *fn)(HANDLE, PVOID, ULONG)) {
    wchar_t buf[256]{};
    // HidD_Get*String returns TRUE on success. Keep blank strings when the interface denies access.
    return fn && fn(h,buf,sizeof(buf)) ? std::wstring(buf) : std::wstring();
}
std::uint16_t parseInterfaceNumber(const std::wstring& path) {
    const auto p=path.find(L"&mi_"); if(p==std::wstring::npos || p+6>path.size()) return 0xffff;
    try { return static_cast<std::uint16_t>(std::stoul(path.substr(p+4,2),nullptr,16)); } catch(...) { return 0xffff; }
}
std::uint16_t parseHardwareToken(const std::wstring& value, const wchar_t* token) {
    const auto p=value.find(token); if(p==std::wstring::npos || p+8>value.size()) return 0;
    try { return static_cast<std::uint16_t>(std::stoul(value.substr(p+4,4),nullptr,16)); } catch(...) { return 0; }
}
bool containsInsensitive(const std::wstring& s, const wchar_t* needle) {
    std::wstring a=s,b=needle; std::transform(a.begin(),a.end(),a.begin(),[](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); }); std::transform(b.begin(),b.end(),b.begin(),[](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); }); return a.find(b)!=std::wstring::npos;
}

}
std::vector<DeviceInfo> DeviceScanner::scan() const {
    std::vector<DeviceInfo> out;
    GUID hidGuid{}; HidD_GetHidGuid(&hidGuid);
    HDEVINFO set=SetupDiGetClassDevsW(&hidGuid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(set==INVALID_HANDLE_VALUE) return out;
    for(DWORD i=0;;++i) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(set,nullptr,&hidGuid,i,&iface)) break;
        DWORD needed=0; SetupDiGetDeviceInterfaceDetailW(set,&iface,nullptr,0,&needed,nullptr); if(!needed) continue;
        std::vector<BYTE> storage(needed); auto detail=reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(storage.data());
        detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA dev{}; dev.cbSize=sizeof(dev);
        if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,needed,nullptr,&dev)) continue;
        DeviceInfo d; d.path=detail->DevicePath; d.instanceId=getInstanceId(set,dev);
        if(containsInsensitive(d.instanceId,L"BTHLE")) d.connection=L"Bluetooth LE HID";
        else if(containsInsensitive(d.instanceId,L"BTHENUM")) d.connection=L"Bluetooth HID";
        // PnP IDs provide VID/PID/revision even if opening the HID collection is denied.
        const std::wstring ids=getDevProperty(set,dev,SPDRP_HARDWAREID);
        d.vid=parseHardwareToken(d.instanceId,L"VID_"); d.pid=parseHardwareToken(d.instanceId,L"PID_");
        // REV_ is not part of the HID instance ID; the first hardware ID (HID\VID_xxxx&PID_xxxx&REV_xxxx...) carries it.
        d.version=parseHardwareToken(ids,L"REV_"); if(!d.version) d.version=parseHardwareToken(d.instanceId,L"REV_");
        DEVPROPTYPE propType=0; GUID container{}; DWORD propBytes=0;
        if(SetupDiGetDevicePropertyW(set,&dev,&DEVPKEY_Device_ContainerId,&propType,reinterpret_cast<PBYTE>(&container),sizeof(container),&propBytes,0) && propType==DEVPROP_TYPE_GUID) {
            wchar_t containerText[64]{}; StringFromGUID2(container,containerText,64); d.containerId=containerText;
        }
        d.manufacturer=getDevProperty(set,dev,SPDRP_MFG);
        if(d.manufacturer.empty()) d.manufacturer=getDevProperty(set,dev,SPDRP_DEVICEDESC);
        d.product=getDevProperty(set,dev,SPDRP_FRIENDLYNAME);
        if(d.product.empty()) d.product=getDevProperty(set,dev,SPDRP_DEVICEDESC);
        d.location=getDevProperty(set,dev,SPDRP_LOCATION_INFORMATION);
        DEVINST parent{}; if(CM_Get_Parent(&parent,dev.DevInst,0)==CR_SUCCESS) {
            wchar_t parentId[MAX_DEVICE_ID_LEN]{}; if(CM_Get_Device_IDW(parent,parentId,MAX_DEVICE_ID_LEN,0)==CR_SUCCESS) d.parentInstanceId=parentId;
        }
        d.receiverHint=(containsInsensitive(d.product,L"receiver")||containsInsensitive(d.product,L"dongle")||containsInsensitive(d.parentInstanceId,L"receiver")||containsInsensitive(d.parentInstanceId,L"dongle")||containsInsensitive(ids,L"receiver")||containsInsensitive(ids,L"dongle")) ? L"POSSIBLE RECEIVER (UNVERIFIED)" : L"UNKNOWN";
        d.interfaceNumber=parseInterfaceNumber(d.path);
        HANDLE h=CreateFileW(d.path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(h==INVALID_HANDLE_VALUE) h=CreateFileW(d.path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(h!=INVALID_HANDLE_VALUE) {
            HIDD_ATTRIBUTES attr{}; attr.Size=sizeof(attr);
            if(HidD_GetAttributes(h,&attr)) { d.vid=attr.VendorID; d.pid=attr.ProductID; d.version=attr.VersionNumber; }
            const auto hm=readHidString(h,HidD_GetManufacturerString), hp=readHidString(h,HidD_GetProductString), hs=readHidString(h,HidD_GetSerialNumberString);
            if(!hm.empty()) d.manufacturer=hm;
            if(!hp.empty()) d.product=hp;
            d.serial=hs;
            PHIDP_PREPARSED_DATA pp=nullptr;
            if(HidD_GetPreparsedData(h,&pp)) { HIDP_CAPS caps{}; if(HidP_GetCaps(pp,&caps)==HIDP_STATUS_SUCCESS) {
                d.hidDescriptorReadable=true; d.usagePage=caps.UsagePage; d.usage=caps.Usage;
                d.inputReportBytes=caps.InputReportByteLength; d.outputReportBytes=caps.OutputReportByteLength; d.featureReportBytes=caps.FeatureReportByteLength;
                d.inputButtonCaps=caps.NumberInputButtonCaps; d.outputButtonCaps=caps.NumberOutputButtonCaps; d.featureButtonCaps=caps.NumberFeatureButtonCaps;
                d.inputValueCaps=caps.NumberInputValueCaps; d.outputValueCaps=caps.NumberOutputValueCaps; d.featureValueCaps=caps.NumberFeatureValueCaps;
                d.hidDescriptorAnalysis=L"WINDOWS_PREPARSED_CAPABILITIES";
                std::set<std::uint8_t> reportIds;
                auto usageText=[&](const wchar_t* direction, USAGE page, USAGE minUsage, USAGE maxUsage, UCHAR reportId, USHORT linkCollection) {
                    std::wostringstream detail; detail << direction << L" page=0x" << std::hex << std::uppercase << page
                        << L" usage=0x" << minUsage;
                    if(maxUsage!=minUsage) detail << L"-0x" << maxUsage;
                    detail << L" reportId=" << std::dec << static_cast<unsigned>(reportId) << L" linkCollection=" << linkCollection;
                    d.hidUsageDetails.push_back(detail.str());
                    if(reportId) reportIds.insert(reportId);
                };
                auto collectButtons=[&](HIDP_REPORT_TYPE type, USHORT count, const wchar_t* direction) {
                    if(!count) return;
                    std::vector<HIDP_BUTTON_CAPS> items(count); USHORT actual=count;
                    if(HidP_GetButtonCaps(type,items.data(),&actual,pp)!=HIDP_STATUS_SUCCESS) return;
                    for(USHORT n=0;n<actual;++n) {
                        const auto& item=items[n];
                        const USAGE lo=item.IsRange?item.Range.UsageMin:item.NotRange.Usage;
                        const USAGE hi=item.IsRange?item.Range.UsageMax:item.NotRange.Usage;
                        usageText(direction,item.UsagePage,lo,hi,item.ReportID,item.LinkCollection);
                    }
                };
                auto collectValues=[&](HIDP_REPORT_TYPE type, USHORT count, const wchar_t* direction) {
                    if(!count) return;
                    std::vector<HIDP_VALUE_CAPS> items(count); USHORT actual=count;
                    if(HidP_GetValueCaps(type,items.data(),&actual,pp)!=HIDP_STATUS_SUCCESS) return;
                    for(USHORT n=0;n<actual;++n) {
                        const auto& item=items[n];
                        const USAGE lo=item.IsRange?item.Range.UsageMin:item.NotRange.Usage;
                        const USAGE hi=item.IsRange?item.Range.UsageMax:item.NotRange.Usage;
                        usageText(direction,item.UsagePage,lo,hi,item.ReportID,item.LinkCollection);
                    }
                };
                collectButtons(HidP_Input,caps.NumberInputButtonCaps,L"INPUT_BUTTON");
                collectButtons(HidP_Output,caps.NumberOutputButtonCaps,L"OUTPUT_BUTTON");
                collectButtons(HidP_Feature,caps.NumberFeatureButtonCaps,L"FEATURE_BUTTON");
                collectValues(HidP_Input,caps.NumberInputValueCaps,L"INPUT_VALUE");
                collectValues(HidP_Output,caps.NumberOutputValueCaps,L"OUTPUT_VALUE");
                collectValues(HidP_Feature,caps.NumberFeatureValueCaps,L"FEATURE_VALUE");
                for(const auto id:reportIds) d.hidReportIds.push_back(std::to_wstring(id));
                if(caps.UsagePage==0x01&&caps.Usage==0x06) d.type=L"Keyboard";
                else if(caps.UsagePage==0x01&&caps.Usage==0x02) d.type=L"Mouse";
                else if(caps.UsagePage==0x0C) d.type=L"Consumer control";
            } HidD_FreePreparsedData(pp); }
            CloseHandle(h);
        }
        out.push_back(std::move(d));
    }
    SetupDiDestroyDeviceInfoList(set);

    // Universal USB PnP fallback: expose present USB VID/PID devnodes that do not
    // publish a HID collection (including vendor-specific interfaces). Metadata only:
    // this path deliberately does not open WinUSB, bulk, interrupt or control pipes.
    HDEVINFO usbDevices=SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_PRESENT);
    if(usbDevices!=INVALID_HANDLE_VALUE) {
        for(DWORD i=0;;++i) {
            SP_DEVINFO_DATA dev{}; dev.cbSize=sizeof(dev);
            if(!SetupDiEnumDeviceInfo(usbDevices,i,&dev)) break;
            const auto instance=getInstanceId(usbDevices,dev);
            const auto hardwareIds=getDevProperty(usbDevices,dev,SPDRP_HARDWAREID);
            if(instance.empty() || (!containsInsensitive(instance,L"USB\\VID_") && !containsInsensitive(hardwareIds,L"USB\\VID_"))) continue;
            if(std::any_of(out.begin(),out.end(),[&](const DeviceInfo& existing){ return existing.instanceId==instance; })) continue;
            DeviceInfo d; d.instanceId=instance; d.vid=parseHardwareToken(instance,L"VID_"); d.pid=parseHardwareToken(instance,L"PID_");
            if(!d.vid) d.vid=parseHardwareToken(hardwareIds,L"VID_");
            if(!d.pid) d.pid=parseHardwareToken(hardwareIds,L"PID_");
            d.version=parseHardwareToken(hardwareIds,L"REV_");
            d.manufacturer=getDevProperty(usbDevices,dev,SPDRP_MFG);
            d.product=getDevProperty(usbDevices,dev,SPDRP_FRIENDLYNAME);
            if(d.product.empty()) d.product=getDevProperty(usbDevices,dev,SPDRP_DEVICEDESC);
            d.location=getDevProperty(usbDevices,dev,SPDRP_LOCATION_INFORMATION);
            d.type=L"USB PnP device"; d.connection=L"USB (PnP metadata only)"; d.compatibility=L"DETECTED ONLY";
            DEVPROPTYPE propType=0; GUID container{}; DWORD propBytes=0;
            if(SetupDiGetDevicePropertyW(usbDevices,&dev,&DEVPKEY_Device_ContainerId,&propType,reinterpret_cast<PBYTE>(&container),sizeof(container),&propBytes,0) && propType==DEVPROP_TYPE_GUID) {
                wchar_t containerText[64]{}; StringFromGUID2(container,containerText,64); d.containerId=containerText;
            }
            DEVINST parent{}; if(CM_Get_Parent(&parent,dev.DevInst,0)==CR_SUCCESS) {
                wchar_t parentId[MAX_DEVICE_ID_LEN]{}; if(CM_Get_Device_IDW(parent,parentId,MAX_DEVICE_ID_LEN,0)==CR_SUCCESS) d.parentInstanceId=parentId;
            }
            out.push_back(std::move(d));
        }
        SetupDiDestroyDeviceInfoList(usbDevices);
    }

    // Enumerate the standard USB device interface GUID and associate interface paths
    // with the matching PnP devnode. SetupAPI supplies paths only; the WinUSB helper
    // below separately queries endpoint metadata when WinUSB initialization succeeds.
    HDEVINFO usbInterfaces=SetupDiGetClassDevsW(&GUID_DEVINTERFACE_USB_DEVICE,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(usbInterfaces!=INVALID_HANDLE_VALUE) {
        for(DWORD i=0;;++i) {
            SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize=sizeof(iface);
            if(!SetupDiEnumDeviceInterfaces(usbInterfaces,nullptr,&GUID_DEVINTERFACE_USB_DEVICE,i,&iface)) break;
            DWORD needed=0; SetupDiGetDeviceInterfaceDetailW(usbInterfaces,&iface,nullptr,0,&needed,nullptr);
            if(!needed) continue;
            std::vector<BYTE> storage(needed); auto detail=reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(storage.data());
            detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            SP_DEVINFO_DATA dev{}; dev.cbSize=sizeof(dev);
            if(!SetupDiGetDeviceInterfaceDetailW(usbInterfaces,&iface,detail,needed,nullptr,&dev)) continue;
            const auto instance=getInstanceId(usbInterfaces,dev);
            auto found=instance.empty()?out.end():std::find_if(out.begin(),out.end(),[&](const DeviceInfo& item){return item.instanceId==instance;});
            if(found!=out.end()) found->usbInterfacePath=detail->DevicePath;
            else {
                DeviceInfo d; d.instanceId=instance; d.usbInterfacePath=detail->DevicePath;
                const auto hardwareIds=getDevProperty(usbInterfaces,dev,SPDRP_HARDWAREID);
                d.vid=parseHardwareToken(instance,L"VID_"); d.pid=parseHardwareToken(instance,L"PID_");
                if(!d.vid) d.vid=parseHardwareToken(hardwareIds,L"VID_");
                if(!d.pid) d.pid=parseHardwareToken(hardwareIds,L"PID_");
                d.version=parseHardwareToken(hardwareIds,L"REV_");
                d.manufacturer=getDevProperty(usbInterfaces,dev,SPDRP_MFG);
                d.product=getDevProperty(usbInterfaces,dev,SPDRP_FRIENDLYNAME);
                if(d.product.empty()) d.product=getDevProperty(usbInterfaces,dev,SPDRP_DEVICEDESC);
                d.type=L"USB device interface"; d.connection=L"USB device interface (metadata only)";
                out.push_back(std::move(d));
            }
        }
        SetupDiDestroyDeviceInfoList(usbInterfaces);
    }

    // WinUSB endpoint metadata is queried only after Windows successfully binds the
    // standard USB device interface to WinUSB. No pipe read/write/transfer is issued.
    for(auto& device:out) if(!device.usbInterfacePath.empty()) discoverWinUsbEndpoints(device);

    // Bluetooth Classic/BLE PnP fallback: expose Windows-enumerated Bluetooth
    // devnodes even when they do not publish a HID collection. This is metadata
    // discovery only; it does not open GATT characteristics or send commands.
    HDEVINFO allDevices=SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_PRESENT);
    if(allDevices!=INVALID_HANDLE_VALUE) {
        for(DWORD i=0;;++i) {
            SP_DEVINFO_DATA dev{}; dev.cbSize=sizeof(dev);
            if(!SetupDiEnumDeviceInfo(allDevices,i,&dev)) break;
            const auto instance=getInstanceId(allDevices,dev);
            const bool isBluetooth=containsInsensitive(instance,L"BTHENUM\\") ||
                                   containsInsensitive(instance,L"BTHLEDEVICE\\") ||
                                   containsInsensitive(instance,L"BTHLE\\");
            if(!isBluetooth || instance.empty()) continue;
            if(std::any_of(out.begin(),out.end(),[&](const DeviceInfo& existing){ return existing.instanceId==instance; })) continue;
            DeviceInfo d;
            d.instanceId=instance;
            const auto hardwareIds=getDevProperty(allDevices,dev,SPDRP_HARDWAREID);
            d.vid=parseHardwareToken(instance,L"VID_");
            d.pid=parseHardwareToken(instance,L"PID_");
            if(!d.vid) d.vid=parseHardwareToken(hardwareIds,L"VID_");
            if(!d.pid) d.pid=parseHardwareToken(hardwareIds,L"PID_");
            d.version=parseHardwareToken(hardwareIds,L"REV_");
            d.manufacturer=getDevProperty(allDevices,dev,SPDRP_MFG);
            d.product=getDevProperty(allDevices,dev,SPDRP_FRIENDLYNAME);
            if(d.product.empty()) d.product=getDevProperty(allDevices,dev,SPDRP_DEVICEDESC);
            d.location=getDevProperty(allDevices,dev,SPDRP_LOCATION_INFORMATION);
            d.type=L"Bluetooth PnP device";
            d.connection=L"Bluetooth (PnP metadata only)";
            d.compatibility=L"DETECTED ONLY";
            d.identificationStatus=L"UNKNOWN";
            DEVPROPTYPE propType=0; GUID container{}; DWORD propBytes=0;
            if(SetupDiGetDevicePropertyW(allDevices,&dev,&DEVPKEY_Device_ContainerId,&propType,
                reinterpret_cast<PBYTE>(&container),sizeof(container),&propBytes,0) && propType==DEVPROP_TYPE_GUID) {
                wchar_t containerText[64]{}; StringFromGUID2(container,containerText,64); d.containerId=containerText;
            }
            DEVINST parent{};
            if(CM_Get_Parent(&parent,dev.DevInst,0)==CR_SUCCESS) {
                wchar_t parentId[MAX_DEVICE_ID_LEN]{};
                if(CM_Get_Device_IDW(parent,parentId,MAX_DEVICE_ID_LEN,0)==CR_SUCCESS) d.parentInstanceId=parentId;
            }
            out.push_back(std::move(d));
        }
        SetupDiDestroyDeviceInfoList(allDevices);
    }
    // C++/WinRT GATT enumeration runs on an isolated MTA worker. It enumerates
    // services and characteristic properties only; it never reads or writes values.
    discoverBluetoothGatt(out);
    return out;
}
}
