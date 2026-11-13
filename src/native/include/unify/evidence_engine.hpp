#pragma once
#include <string>
#include <vector>
namespace unify::evidence {
enum class Type { HardwareId, Revision, ManufacturerString, ProductString, SerialString, ContainerId, ParentId, HidDescriptor, ParsedUsage, TrustedCatalog, VendorDocumentation, PhysicalTest, IdentityConflict };
struct Item { Type type{Type::HardwareId}; std::wstring value, source, note; bool independentlyVerified{false}; };
struct Assessment { unsigned score{}; std::wstring identityLevel, confidence, state; std::vector<std::wstring> conflicts; };
class Engine { public: static Assessment assess(const std::vector<Item>& evidence); };
const wchar_t* typeName(Type type);
} // namespace unify::evidence
