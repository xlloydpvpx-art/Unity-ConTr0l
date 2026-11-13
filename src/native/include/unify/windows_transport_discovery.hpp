#pragma once
#include "unify/device_scanner.hpp"
#include <vector>
namespace unify {
// Windows-only discovery helpers. Both routines collect metadata and never issue
// endpoint transfers or GATT characteristic reads/writes.
void discoverWinUsbEndpoints(DeviceInfo& device);
void discoverBluetoothGatt(std::vector<DeviceInfo>& devices);
}
