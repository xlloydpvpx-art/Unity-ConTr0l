#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace unify::transport {
enum class Kind { UsbHid, UsbPnP, UsbVendorInterface, BluetoothHid, BleGatt, OtherPnP, Unknown };
enum class Access { MetadataOnly, DescriptorRead, ReadOnlyProtocol, ConfigurationWrite };
struct Observation {
    Kind kind{Kind::Unknown};
    std::wstring instanceId, interfacePath, containerId, parentId;
    std::uint16_t vid{}, pid{}, revision{};
    bool revisionKnown{false};
    std::wstring manufacturer, product;
};
struct BackendDescriptor {
    std::wstring id, displayName;
    Kind kind{Kind::Unknown};
    Access maximumAccess{Access::MetadataOnly};
    bool trusted{false};
};
class IDiscoveryBackend {
public:
    virtual ~IDiscoveryBackend() = default;
    virtual BackendDescriptor descriptor() const = 0;
    virtual std::vector<Observation> enumerate() = 0;
};
struct BackendStatus { std::wstring id, state, reason; };
// Registry only coordinates discovery providers. It intentionally exposes no generic
// open/write/send method: a backend cannot gain write permission by registration.
class Registry {
public:
    bool add(std::shared_ptr<IDiscoveryBackend> backend, std::wstring& reason);
    std::vector<Observation> enumerateAll(std::vector<BackendStatus>& status) const;
    std::size_t size() const noexcept { return backends_.size(); }
private:
    struct Entry { BackendDescriptor descriptor; std::shared_ptr<IDiscoveryBackend> backend; };
    std::vector<Entry> backends_;
};
const wchar_t* kindName(Kind kind);
const wchar_t* accessName(Access access);
} // namespace unify::transport
