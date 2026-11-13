#pragma once
#include <string>
#include <vector>
namespace unify::diagnostics {
enum class Severity { Info, Warning, Error };
struct Facts { bool present{true}, hidInterface{false}, descriptorReadable{false}, descriptorValid{false}, usageRecognized{false}, reportInput{false}, reportOutput{false}, reportFeature{false}, identityConflict{false}, adapterMatched{false}, protocolVerified{false}; };
struct Finding { Severity severity{Severity::Info}; std::wstring code, title, detail, recommendation; };
class Engine { public: static std::vector<Finding> analyze(const Facts& facts); };
const wchar_t* severityName(Severity value);
} // namespace unify::diagnostics
