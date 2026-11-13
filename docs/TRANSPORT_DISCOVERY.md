# Extensible transport discovery — v1.6.0

The portable `unify::transport::Registry` accepts trusted discovery backends implementing `IDiscoveryBackend`. A backend declares its transport kind and maximum discovery access, then returns metadata observations.

Accepted access levels are `MetadataOnly` and `DescriptorRead`. `ReadOnlyProtocol` and `ConfigurationWrite` are rejected by this discovery registry: protocol operations belong in a separately reviewed adapter implementation and remain subject to the adapter SDK's exact-identity, firmware, trust, audit and runtime-policy gates.

The registry caches validated backend descriptors, isolates exceptions per backend, discards observations whose transport kind conflicts with the registered backend, and records a status for each enumerator. It has no generic open, send, report-write, bulk-write or control-transfer API.

Windows discovery uses SetupAPI/Configuration Manager for HID device interfaces, generic USB PnP devnodes, standard USB device-interface paths and Bluetooth PnP devnodes. In v1.6.0, WinUSB pipe metadata and BLE GATT service/characteristic metadata are also queried when Windows exposes a supported interface/device. The application does not transfer endpoint data or read/write GATT values.

## v1.6.0 Windows transport backends

### WinUSB endpoint metadata
The Windows scanner attempts `CreateFileW` and `WinUsb_Initialize` only for paths returned by SetupAPI's standard USB device-interface enumeration. It does not enumerate arbitrary vendor-specific interface GUIDs, so a device whose WinUSB path is exposed only under a custom GUID may remain PnP-only until a future GUID-discovery extension. When initialization succeeds, it queries the default and associated WinUSB interfaces, alternate-setting descriptors, and `WINUSB_PIPE_INFORMATION` records. Endpoint metadata is bounded and validated before serialization. The implementation does not call `WinUsb_ReadPipe`, `WinUsb_WritePipe`, `WinUsb_ControlTransfer`, or any other transfer API. A failed open/initialization is reported as a status, not interpreted as evidence that a device lacks endpoints.

### Bluetooth LE GATT metadata
A dedicated MTA worker uses Windows C++/WinRT `BluetoothLEDevice`, `DeviceInformation`, and `GattDeviceService` APIs. The scan waits at most 15 seconds for the GATT worker; if it exceeds that limit, the worker is detached with owned result storage and its late result is discarded. It enumerates Windows-visible BLE devices, their GATT service UUIDs, and characteristic UUID/property flags. It does not call characteristic `ReadValueAsync`, `WriteValueAsync`, or notification subscription APIs. System-level and per-device exceptions are isolated; an unavailable device is not treated as protocol evidence.

### SDK boundary
`UsbWinUsb` is an SDK transport identifier so reviewed adapters can name the physical transport. It is not a generic WinUSB protocol engine. The existing exact-identity, signed-manifest, safety-review, physical-test, readback and host-policy checks remain required for any adapter operation. Discovery registration continues to reject write-capable backends.


### Official API references
- [WinUsb_QueryInterfaceSettings](https://learn.microsoft.com/en-us/windows/win32/api/winusb/nf-winusb-winusb_queryinterfacesettings) and [WinUsb_QueryPipe](https://learn.microsoft.com/en-us/windows/win32/api/winusb/nf-winusb-winusb_querypipe) provide interface/alternate-setting and endpoint-pipe metadata.
- [WinUSB access and endpoint query flow](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/using-winusb-api-to-communicate-with-a-usb-device).
- [BluetoothLEDevice.GetGattServicesAsync](https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothledevice.getgattservicesasync) and [GattDeviceService.GetCharacteristicsAsync](https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattdeviceservice.getcharacteristicsasync) enumerate GATT services and characteristics.
- [GattCharacteristicProperties](https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattcharacteristicproperties) defines the exposed property flags.
