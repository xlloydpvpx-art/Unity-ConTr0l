#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dbt.h>
#include <wrl.h>
#include <wrl/event.h>
#include <hidsdi.h>
#include <atomic>
#include <exception>
#include <functional>
#include <filesystem>
#include <string>
#include <memory>
#include <thread>
#include <cwchar>
#include "WebView2.h"
#include "unify/device_scanner.hpp"
#include "unify/observation_store.hpp"
#include "unify/device_database.hpp"
#include "unify/protocol_research.hpp"
#include "unify/system_settings.hpp"
#include "unify/via_hid.hpp"
#include <mutex>
#include <vector>
#include <shlobj.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "hid.lib")

static constexpr UINT WM_SCAN_DONE = WM_APP + 1;
static constexpr UINT WM_VIA_DONE = WM_APP + 2;
static constexpr UINT_PTR TIMER_RESCAN = 1;

static HWND g_hwnd{};
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2> g_webview;
static unify::DeviceScanner g_scanner;
static unify::DeviceDatabase g_database;
static unify::ProtocolResearchCenter g_protocolResearch;
static std::filesystem::path appDir;
static std::filesystem::path observationsFile;
static std::filesystem::path webViewUserData;
static HDEVNOTIFY g_deviceNotify{};
// Windows settings as found when the app started, so the user can always go back.
static std::vector<unify::winsettings::Value> g_originalSettings;

// Scan state. Only the UI thread touches these; the worker thread only reads the (immutable after load)
// database/research objects and writes the observation file.
static std::thread g_scanThread;
static bool g_scanning = false;
static bool g_rescanPending = false;

// Last scan, in the same order as the JSON "devices" array. The UI refers to devices by index only.
static std::mutex g_lastScanMutex;
static std::vector<unify::DeviceInfo> g_lastScan;

static std::wstring buildScanJson() {
    auto devices = g_scanner.scan();
    for (auto& device : devices) g_database.identify(device);
    { std::lock_guard<std::mutex> lock(g_lastScanMutex); g_lastScan = devices; }
    unify::ObservationStore::saveUnknown(devices, observationsFile);
    auto json = unify::DeviceScanner::toJson(devices, observationsFile.wstring(), g_database.manufacturerCount(), g_database.verifiedDeviceCount(), g_database.loadStatus());
    const auto marker = json.rfind(L"}");
    if (marker != std::wstring::npos) {
        const auto databaseHealth = L",\"databaseHealth\":{\"rejectedVerifiedRecords\":" + std::to_wstring(g_database.rejectedVerifiedRecords()) + L",\"conflictingModelKeys\":" + std::to_wstring(g_database.conflictingModelKeys()) + L"}";
        const auto protocol = L",\"protocolResearch\":{\"status\":\"" + g_protocolResearch.loadStatus() + L"\",\"itemCount\":" + std::to_wstring(g_protocolResearch.itemCount()) + L",\"verifiedCount\":" + std::to_wstring(g_protocolResearch.verifiedCount()) + L",\"items\":" + g_protocolResearch.toJson() + L"}";
        json.insert(marker, databaseHealth + protocol);
    }
    return json;
}

// Enumerating HID interfaces can block on misbehaving devices, so it never runs on the UI thread.
// A request that arrives while a scan is running is coalesced into exactly one follow-up scan.
static void startScan() {
    if (!g_webview || !g_hwnd) return;
    if (g_scanning) { g_rescanPending = true; return; }
    g_scanning = true;
    if (g_scanThread.joinable()) g_scanThread.join(); // previous worker has already posted its result
    const HWND hwnd = g_hwnd;
    g_scanThread = std::thread([hwnd]() {
        std::wstring* json = nullptr;
        try { json = new std::wstring(buildScanJson()); } catch (...) { json = nullptr; }
        if (!PostMessageW(hwnd, WM_SCAN_DONE, 0, reinterpret_cast<LPARAM>(json))) delete json;
    });
}
static void finishScan(std::wstring* rawJson) {
    std::unique_ptr<std::wstring> json(rawJson);
    g_scanning = false;
    if (json && g_webview) g_webview->PostWebMessageAsJson(json->c_str());
    if (g_rescanPending) { g_rescanPending = false; startScan(); }
}

// Layer 1 (Windows-level) settings. These run on the UI thread: each call is a quick, documented Win32 API.
static void postSettings(const std::wstring& key, bool ok, const std::wstring& error) {
    if (!g_webview) return;
    const std::wstring json = L"{\"type\":\"systemSettings\",\"settings\":" + unify::winsettings::toJson(unify::winsettings::readAll())
        + L",\"originals\":" + unify::winsettings::toJson(g_originalSettings)
        + L",\"result\":{\"key\":\"" + key + L"\",\"ok\":" + (ok ? L"true" : L"false") + L",\"error\":\"" + error + L"\"}}";
    g_webview->PostWebMessageAsJson(json.c_str());
}
static void handleSettingsCommand(const std::wstring& command) {
    if (command == L"settings:get") { postSettings(L"", true, L""); return; }
    if (command == L"settings:restore") {
        bool ok = true; std::wstring error;
        for (const auto& original : g_originalSettings) {
            std::wstring one;
            if (!unify::winsettings::write(original.key, original.value, one)) { ok = false; error = one; }
        }
        postSettings(L"*", ok, error);
        return;
    }
    constexpr wchar_t prefix[] = L"settings:set:";
    if (command.compare(0, 13, prefix) != 0) return;
    const std::wstring rest = command.substr(13);
    const auto sep = rest.find(L':');
    if (sep == std::wstring::npos) { postSettings(L"", false, L"BAD_COMMAND"); return; }
    const std::wstring key = rest.substr(0, sep), valueText = rest.substr(sep + 1);
    if (!unify::winsettings::isKnownKey(key)) { postSettings(L"", false, L"UNKNOWN_SETTING"); return; }
    wchar_t* end = nullptr; const long value = wcstol(valueText.c_str(), &end, 10);
    if (valueText.empty() || !end || *end != L'\0') { postSettings(key, false, L"BAD_VALUE"); return; }
    std::wstring error;
    const bool ok = unify::winsettings::write(key, static_cast<int>(value), error);
    postSettings(key, ok, error);
}

// Read-only VIA probe (QMK keyboards). Runs on a short-lived thread because a silent device can take ~1 s to time out.
static void startViaProbe(long index) {
    if (!g_hwnd) return;
    std::wstring path;
    {
        std::lock_guard<std::mutex> lock(g_lastScanMutex);
        if (index >= 0 && static_cast<size_t>(index) < g_lastScan.size()) {
            const auto& d = g_lastScan[static_cast<size_t>(index)];
            if (d.usagePage == unify::via::kUsagePage && d.usage == unify::via::kUsage) path = d.path; // only genuine VIA raw-HID interfaces
        }
    }
    const HWND hwnd = g_hwnd;
    std::thread([hwnd, index, path]() {
        unify::via::ProbeResult r;
        if (path.empty()) r.error = L"NOT_VIA_INTERFACE"; else r = unify::via::probe(path);
        auto* json = new std::wstring(L"{\"type\":\"viaProbe\",\"index\":" + std::to_wstring(index) + L",\"ok\":" + (r.ok ? L"true" : L"false")
            + L",\"protocolVersion\":" + std::to_wstring(r.protocolVersion) + L",\"layerCount\":" + std::to_wstring(r.layerCount)
            + L",\"error\":\"" + r.error + L"\"}");
        if (!PostMessageW(hwnd, WM_VIA_DONE, 0, reinterpret_cast<LPARAM>(json))) delete json;
    }).detach();
}
static void resizeWebView() {
    if (!g_controller || !g_hwnd) return;
    RECT r{}; GetClientRect(g_hwnd, &r);
    g_controller->put_Bounds(r);
}
static void registerHidNotifications(HWND hwnd) {
    // Without this registration Windows does not send DBT_DEVICEARRIVAL/REMOVECOMPLETE for HID interfaces.
    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HidD_GetHidGuid(&filter.dbcc_classguid);
    g_deviceNotify = RegisterDeviceNotificationW(hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
    case WM_SIZE: resizeWebView(); return 0;
    case WM_DEVICECHANGE:
        if (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE || wp == DBT_DEVNODES_CHANGED) SetTimer(hwnd, TIMER_RESCAN, 350, nullptr); // debounce bursts
        return TRUE;
    case WM_TIMER: if (wp == TIMER_RESCAN) { KillTimer(hwnd, TIMER_RESCAN); startScan(); } return 0;
    case WM_SCAN_DONE: finishScan(reinterpret_cast<std::wstring*>(lp)); return 0;
    case WM_VIA_DONE: {
        std::unique_ptr<std::wstring> json(reinterpret_cast<std::wstring*>(lp));
        if (json && g_webview) g_webview->PostWebMessageAsJson(json->c_str());
        return 0;
    }
    case WM_DESTROY:
        if (g_deviceNotify) { UnregisterDeviceNotification(g_deviceNotify); g_deviceNotify = nullptr; }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH); appDir=std::filesystem::path(exe).parent_path();
    wchar_t* localAppData=nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&localAppData)) && localAppData) {
        const auto base = std::filesystem::path(localAppData)/L"UNIFY_CONTROL";
        observationsFile = base/L"unknown_observations.json";
        webViewUserData = base/L"WebView2"; // writable even when installed under Program Files, so profiles persist
        CoTaskMemFree(localAppData);
    } else {
        observationsFile = appDir/L"unknown_observations.json";
        webViewUserData = appDir/L"WebView2";
    }
    { std::error_code ec; std::filesystem::create_directories(webViewUserData, ec); }
    g_originalSettings = unify::winsettings::readAll();
    g_database.load(appDir / L"database" / L"devices.json");
    g_protocolResearch.load(appDir / L"database" / L"protocol_research.json");
    WNDCLASSW wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=instance; wc.lpszClassName=L"UnifyControlWindow"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&wc);
    g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"UNIFY CONTROL",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1440,900,nullptr,nullptr,instance,nullptr);
    if(!g_hwnd) return 1;
    registerHidNotifications(g_hwnd);
    ShowWindow(g_hwnd,show);
    auto handler = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([](HRESULT hr, ICoreWebView2Environment* env)->HRESULT {
        if(FAILED(hr)||!env) return hr;
        return env->CreateCoreWebView2Controller(g_hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([](HRESULT result, ICoreWebView2Controller* controller)->HRESULT {
            if(FAILED(result)||!controller) return result;
            g_controller=controller; g_controller->get_CoreWebView2(&g_webview); resizeWebView();
            EventRegistrationToken messageToken{};
            g_webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>([](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args)->HRESULT {
                LPWSTR source=nullptr; args->get_Source(&source);
                const bool trusted=source && wcsncmp(source,L"https://unify.local/",20)==0;
                if(source) CoTaskMemFree(source);
                if(!trusted) return S_OK;
                LPWSTR raw=nullptr; if(SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                    std::wstring command(raw); CoTaskMemFree(raw);
                    if(command==L"scan") startScan();
                    else if(command.compare(0,9,L"settings:")==0) handleSettingsCommand(command);
                    else if(command.compare(0,10,L"via:probe:")==0) { wchar_t* end=nullptr; const long idx=wcstol(command.c_str()+10,&end,10); if(end && *end==L'\0' && end!=command.c_str()+10) startViaProbe(idx); }
                }
                return S_OK;
            }).Get(), &messageToken);
            ComPtr<ICoreWebView2_3> webview3;
            if (SUCCEEDED(g_webview.As(&webview3)) && webview3)
                webview3->SetVirtualHostNameToFolderMapping(L"unify.local", (appDir / L"frontend").wstring().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
            // The page requests the first scan itself once its message listener exists (see app.js);
            // scanning here would race the page load and could be lost.
            g_webview->Navigate(L"https://unify.local/index.html");
            return S_OK;
        }).Get());
    });
    HRESULT hr=CreateCoreWebView2EnvironmentWithOptions(nullptr,webViewUserData.c_str(),nullptr,handler.Get());
    if(FAILED(hr)) { MessageBoxW(g_hwnd,L"No se pudo iniciar WebView2. Instala Microsoft Edge WebView2 Runtime.",L"UNIFY CONTROL",MB_ICONERROR); return 2; }
    MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    const int code=static_cast<int>(msg.wParam);
    // If a device is hanging the worker inside a HID call, do not block shutdown waiting for it.
    if (g_scanning) ExitProcess(static_cast<UINT>(code));
    if (g_scanThread.joinable()) g_scanThread.join();
    return code;
}
