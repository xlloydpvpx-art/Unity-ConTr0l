#include "unify/windows_transport_discovery.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winusb.h>
#include <setupapi.h>
#include <usbiodef.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <algorithm>
#include <cwctype>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <utility>

#pragma comment(lib, "winusb.lib")
#pragma comment(lib, "runtimeobject.lib")

namespace unify {
namespace {
std::wstring lower(std::wstring value) {
    std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
    return value;
}
struct ApartmentScope {
    ApartmentScope() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~ApartmentScope() { winrt::uninit_apartment(); }
    ApartmentScope(const ApartmentScope&)=delete;
    ApartmentScope& operator=(const ApartmentScope&)=delete;
};
struct FileHandleGuard {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~FileHandleGuard() { if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct WinUsbHandleGuard {
    WINUSB_INTERFACE_HANDLE value{};
    ~WinUsbHandleGuard() { if(value) WinUsb_Free(value); }
    WinUsbHandleGuard(const WinUsbHandleGuard&)=delete;
    WinUsbHandleGuard& operator=(const WinUsbHandleGuard&)=delete;
    WinUsbHandleGuard()=default;
};
void scanGattOnWorker(std::vector<DeviceInfo>& out) {
    try {
        ApartmentScope apartment;
        using namespace winrt::Windows::Devices::Enumeration;
        using namespace winrt::Windows::Devices::Bluetooth;
        using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
        const auto selector=BluetoothLEDevice::GetDeviceSelector();
        const auto found=DeviceInformation::FindAllAsync(selector).get();
        for (const auto& info:found) {
            if(out.size()>=4096) break;
            DeviceInfo device;
            device.instanceId=info.Id().c_str();
            device.product=info.Name().c_str();
            device.type=L"Bluetooth LE device";
            device.connection=L"Bluetooth LE (GATT metadata discovery)";
            device.compatibility=L"DETECTED ONLY";
            try {
                auto ble=BluetoothLEDevice::FromIdAsync(info.Id()).get();
                if(!ble) { device.gattStatus=L"DEVICE_UNAVAILABLE"; out.push_back(std::move(device)); continue; }
                const auto result=ble.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
                if(result.Status()!=GattCommunicationStatus::Success) {
                    device.gattStatus=L"GATT_SERVICE_DISCOVERY_FAILED";
                    out.push_back(std::move(device)); continue;
                }
                std::vector<transport::GattService> services;
                std::size_t totalCharacteristics=0;
                bool truncated=false;
                bool partial=false;
                for(const auto& service:result.Services()) {
                    if(services.size()>=512) { truncated=true; break; }
                    transport::GattService item;
                    item.uuid=winrt::to_hstring(service.Uuid()).c_str();
                    const auto chars=service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached).get();
                    if(chars.Status()==GattCommunicationStatus::Success) {
                        for(const auto& characteristic:chars.Characteristics()) {
                            if(item.characteristics.size()>=2048 || totalCharacteristics>=4096) { truncated=true; break; }
                            item.characteristics.push_back({winrt::to_hstring(characteristic.Uuid()).c_str(),
                                static_cast<std::uint32_t>(characteristic.CharacteristicProperties())});
                            ++totalCharacteristics;
                        }
                    } else partial=true;
                    services.push_back(std::move(item));
                }
                std::wstring reason;
                if(!transport::validateGattMetadata(services,reason)) device.gattStatus=L"GATT_METADATA_REJECTED";
                else { device.gattServices=std::move(services); device.gattStatus=truncated?L"GATT_ENUMERATION_LIMIT_REACHED_PARTIAL":(partial?L"GATT_CHARACTERISTICS_PARTIALLY_UNAVAILABLE":L"GATT_SERVICES_AND_CHARACTERISTICS_ENUMERATED"); }
            } catch(const winrt::hresult_error&) { device.gattStatus=L"GATT_DISCOVERY_ERROR_ISOLATED"; }
            out.push_back(std::move(device));
        }
    } catch(const winrt::hresult_error&) {
        // System-level WinRT failure is isolated; do not manufacture a device row.
    } catch(...) {
        // Unknown platform failure is isolated; do not manufacture a device row.
    }
}
}
void discoverWinUsbEndpoints(DeviceInfo& device) {
    device.winUsbStatus=L"NO_USB_INTERFACE_PATH";
    if(device.usbInterfacePath.empty()) return;
    FileHandleGuard file{CreateFileW(device.usbInterfacePath.c_str(),GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE) { device.winUsbStatus=L"WINUSB_OPEN_DENIED_OR_NOT_BOUND"; return; }
    WinUsbHandleGuard base;
    if(!WinUsb_Initialize(file.value,&base.value)) { device.winUsbStatus=L"WINUSB_DRIVER_NOT_BOUND_OR_INITIALIZE_FAILED"; return; }
    device.winUsbStatus=L"WINUSB_INITIALIZED_NO_ENDPOINTS";
    bool truncated=false;
    try {
        auto collect=[&](WINUSB_INTERFACE_HANDLE handle) {
            for(unsigned alt=0;alt<32 && !truncated;++alt) {
                USB_INTERFACE_DESCRIPTOR iface{};
                if(!WinUsb_QueryInterfaceSettings(handle,static_cast<UCHAR>(alt),&iface)) continue;
                if(iface.bNumEndpoints>32) continue;
                for(UCHAR index=0;index<iface.bNumEndpoints;++index) {
                    WINUSB_PIPE_INFORMATION pipe{};
                    if(!WinUsb_QueryPipe(handle,static_cast<UCHAR>(alt),index,&pipe)) continue;
                    transport::UsbEndpoint endpoint;
                    endpoint.interfaceNumber=iface.bInterfaceNumber;
                    endpoint.alternateSetting=iface.bAlternateSetting;
                    endpoint.address=pipe.PipeId;
                    endpoint.attributes=static_cast<UCHAR>(pipe.PipeType);
                    endpoint.maxPacketSize=pipe.MaximumPacketSize;
                    endpoint.interval=pipe.Interval;
                    endpoint.pipeId=pipe.PipeId;
                    std::wstring reason;
                    if(transport::validateUsbEndpoint(endpoint,reason)) {
                        if(device.usbEndpoints.size()>=4096) { truncated=true; break; }
                        device.usbEndpoints.push_back(endpoint);
                    }
                }
            }
        };
        collect(base.value);
        for(UCHAR associated=0;associated<16 && !truncated;++associated) {
            WinUsbHandleGuard handle;
            if(!WinUsb_GetAssociatedInterface(base.value,associated,&handle.value)) break;
            collect(handle.value);
        }
        if(truncated) device.winUsbStatus=L"WINUSB_ENUMERATION_LIMIT_REACHED_PARTIAL";
        else if(!device.usbEndpoints.empty()) {
            device.winUsbStatus=L"WINUSB_ENDPOINT_METADATA_ENUMERATED_NO_TRANSFERS";
            device.connection=L"USB/WinUSB (endpoint metadata only)";
        }
    } catch(...) {
        device.winUsbStatus=L"WINUSB_DISCOVERY_ERROR_ISOLATED";
    }
}
void discoverBluetoothGatt(std::vector<DeviceInfo>& devices) {
    try {
        auto discovered=std::make_shared<std::vector<DeviceInfo>>();
        auto completed=std::make_shared<std::promise<void>>();
        auto ready=completed->get_future();
        std::thread worker([discovered,completed]{
            scanGattOnWorker(*discovered);
            try { completed->set_value(); } catch(...) {}
        });
        // Do not let a sleeping/unavailable BLE peripheral stall the entire device scan.
        if(ready.wait_for(std::chrono::seconds(15))!=std::future_status::ready) {
            worker.detach(); // Shared result storage remains alive until the worker exits.
            return;
        }
        worker.join();
        for(auto& item:*discovered) {
            if(item.instanceId.empty()) { devices.push_back(std::move(item)); continue; }
            const auto key=lower(item.instanceId);
            auto existing=std::find_if(devices.begin(),devices.end(),[&](const DeviceInfo& d){return !d.instanceId.empty()&&lower(d.instanceId)==key;});
            if(existing==devices.end()) devices.push_back(std::move(item));
            else { existing->gattServices=std::move(item.gattServices); existing->gattStatus=std::move(item.gattStatus); existing->connection=L"Bluetooth LE (GATT metadata discovery)"; }
        }
    } catch(...) {
        // Discovery failure must not prevent USB/HID inventory from being returned.
    }
}
}
#endif
