# Release checklist — UNIFY CONTROL 1.6.0

- [x] Continue the v1.5.0 source tree and preserve its architecture.
- [x] Add bounded portable USB endpoint metadata validation.
- [x] Add Windows WinUSB initialization and interface/alternate-setting/pipe metadata enumeration.
- [x] Add Windows Bluetooth LE GATT service/characteristic metadata enumeration.
- [x] Integrate `UsbWinUsb` into the existing Adapter SDK transport enum.
- [x] Keep discovery paths free of endpoint transfers and GATT value operations.
- [x] Add simulated USB endpoint/GATT metadata fixtures and error/bounds tests.
- [x] Serialize transport status and metadata to the frontend.
- [x] Keep desktop hardware writes disabled and reject write-capable discovery backends.
- [x] Portable C++20 build, CTest, JavaScript syntax and JSON validation.
- [ ] Windows/MSVC/Windows SDK/C++/WinRT/WebView2 build and startup.
- [ ] Physical WinUSB and Bluetooth LE device matrix, driver/permission and hot-plug testing.
- [ ] Cryptographic adapter signing/trust store and isolated runtime loading.
- [ ] Any hardware write operation (not enabled; requires separately reviewed, protocol-specific physical validation).
