#include "unify/via_hid.hpp"
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <cstring>

namespace unify::via {
namespace {

struct Handle {
    HANDLE h{INVALID_HANDLE_VALUE};
    ~Handle() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
struct EventHandle {
    HANDLE h{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    ~EventHandle() { if (h) CloseHandle(h); }
};

// One request/response round trip on an already opened raw HID interface (report ID 0, 32-byte payload).
bool exchange(HANDLE device, const Frame& request, Frame& reply, DWORD timeoutMs, std::wstring& error) {
    if (!isAllowedReadCommand(request[0])) { error = L"COMMAND_NOT_ALLOWED"; return false; }
    EventHandle writeEvent, readEvent;
    if (!writeEvent.h || !readEvent.h) { error = L"EVENT_FAILED"; return false; }

    std::uint8_t out[kFrameBytes + 1] = {0};
    std::memcpy(out + 1, request.data(), kFrameBytes);
    OVERLAPPED ow{}; ow.hEvent = writeEvent.h;
    DWORD written = 0;
    if (!WriteFile(device, out, static_cast<DWORD>(sizeof(out)), nullptr, &ow)) {
        if (GetLastError() != ERROR_IO_PENDING) { error = L"WRITE_FAILED"; return false; }
        if (WaitForSingleObject(writeEvent.h, timeoutMs) != WAIT_OBJECT_0) { CancelIoEx(device, &ow); GetOverlappedResult(device, &ow, &written, TRUE); error = L"WRITE_TIMEOUT"; return false; }
    }
    if (!GetOverlappedResult(device, &ow, &written, FALSE)) { error = L"WRITE_FAILED"; return false; }

    std::uint8_t in[kFrameBytes + 1] = {0};
    OVERLAPPED orr{}; orr.hEvent = readEvent.h;
    DWORD got = 0;
    if (!ReadFile(device, in, static_cast<DWORD>(sizeof(in)), nullptr, &orr)) {
        if (GetLastError() != ERROR_IO_PENDING) { error = L"READ_FAILED"; return false; }
        if (WaitForSingleObject(readEvent.h, timeoutMs) != WAIT_OBJECT_0) { CancelIoEx(device, &orr); GetOverlappedResult(device, &orr, &got, TRUE); error = L"NO_REPLY"; return false; }
    }
    if (!GetOverlappedResult(device, &orr, &got, FALSE) || got < kFrameBytes) { error = L"SHORT_REPLY"; return false; }
    std::memcpy(reply.data(), in + 1, kFrameBytes);
    return true;
}

} // namespace

ProbeResult probe(const std::wstring& devicePath) {
    ProbeResult result;
    Handle dev;
    dev.h = CreateFileW(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (dev.h == INVALID_HANDLE_VALUE) { result.error = L"OPEN_FAILED"; return result; }

    // The raw interface must be exactly report ID 0 + 32 bytes in both directions before we send anything.
    PHIDP_PREPARSED_DATA pre = nullptr;
    if (!HidD_GetPreparsedData(dev.h, &pre)) { result.error = L"NO_DESCRIPTOR"; return result; }
    HIDP_CAPS caps{};
    const bool capsOk = HidP_GetCaps(pre, &caps) == HIDP_STATUS_SUCCESS;
    HidD_FreePreparsedData(pre);
    if (!capsOk || caps.UsagePage != kUsagePage || caps.Usage != kUsage) { result.error = L"NOT_VIA_INTERFACE"; return result; }
    if (caps.InputReportByteLength != kFrameBytes + 1 || caps.OutputReportByteLength != kFrameBytes + 1) { result.error = L"UNEXPECTED_REPORT_SIZE"; return result; }

    Frame reply{};
    if (!exchange(dev.h, buildGetProtocolVersion(), reply, 600, result.error)) return result;
    if (!parseProtocolVersion(reply, result.protocolVersion)) { result.error = L"NOT_A_VIA_KEYBOARD"; return result; }
    if (!exchange(dev.h, buildGetLayerCount(), reply, 600, result.error)) return result;
    if (!parseLayerCount(reply, result.layerCount)) { result.error = L"BAD_LAYER_REPLY"; return result; }
    result.ok = true;
    return result;
}

} // namespace unify::via
