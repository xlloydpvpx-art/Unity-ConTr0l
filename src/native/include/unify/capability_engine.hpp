#pragma once
#include <string>
#include <vector>
namespace unify {
struct CapabilityRecord { std::wstring name, operation, status, evidence, implementation; };
struct CapabilityResolution { std::wstring name, operation, state, evidence; bool executable{false}; };
class CapabilityEngine { public: static std::vector<CapabilityResolution> resolve(const std::vector<CapabilityRecord>& records); };
}
