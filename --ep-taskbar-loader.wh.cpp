// ==WindhawkMod==
// @id              --ep-taskbar-loader
// @name            ep_taskbar loader
// @description     Loads ep_taskbar
// @version         1.7
// @author          Reabstraction
// @include         explorer.exe
// @compilerOptions -lcomdlg32 -lole32
// @license         MIT
// @homepage        https://github.com/Reabstraction/ep_taskbar_loader
// @github          https://github.com/Reabstraction
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- install_path: "C:\\Program Files\\ep_taskbar"
  $name: ep_taskbar install path
  $description: "This folder should contain `ep_taskbar.X.dll`, corresponding
to your Windows version, requires a restart"
- dll_name: ""
  $name: DLL Name
  $description: "ONLY FOR DEBUG: Pin a specific DLL, requires a restart"
*/
// ==/WindhawkModSettings==

#include <TlHelp32.h>
#include <fileapi.h>
#include <processthreadsapi.h>
#include <windhawk_api.h>
#include <windows.h>
#include <winreg.h>

#include <cwchar>
#include <optional>
#include <string>

LSTATUS _RegSetKeyValueWithSDDL(HKEY hKey, const WCHAR *lpSubKey, const WCHAR *lpValueName, DWORD dwType,
                                const void *lpData, DWORD cbData, SECURITY_ATTRIBUTES *lpSecurityAttribs) {
    HKEY hKeyOrig = hKey;
    HKEY hKeyNew;
    if (lpSubKey && lpSubKey[0]) {
        LSTATUS lstat =
            RegCreateKeyExW(hKey, lpSubKey, 0, nullptr, 0, KEY_SET_VALUE, lpSecurityAttribs, &hKeyNew, nullptr);
        if (lstat)
            return lstat;
        hKey = hKeyNew;
    } else {
        hKeyNew = hKey;
    }
    LSTATUS lstat = RegSetValueExW(hKey, lpValueName, 0, dwType, (const BYTE *)lpData, cbData);
    if (hKeyNew != hKeyOrig)
        RegCloseKey(hKeyNew);
    return lstat;
}

HRESULT SHRegSetBOOL(HKEY hkey, const WCHAR *pwszSubKey, const WCHAR *pwszValue, BOOL bData) {
    DWORD dwData = bData != 0;
    LSTATUS result = _RegSetKeyValueWithSDDL(hkey, pwszSubKey, pwszValue, REG_DWORD, &dwData, sizeof(DWORD), nullptr);
    return result > 0 ? HRESULT_FROM_WIN32(result) : result;
}

HRESULT SHRegSetString(HKEY hkey, const WCHAR *pwszSubKey, const WCHAR *pwszValue, const WCHAR *pwszData) {
    SIZE_T len = wcslen(pwszData);
    HRESULT result;
    if (len >= 0x7FFFFFFF)
        result = ERROR_ARITHMETIC_OVERFLOW;
    else
        result = _RegSetKeyValueWithSDDL(hkey, pwszSubKey, pwszValue, REG_SZ, pwszData,
                                         (DWORD)(sizeof(WCHAR) * len + sizeof(WCHAR)), nullptr);
    return HRESULT_FROM_WIN32(result);
}

HRESULT SHRegGetBOOLWithREGSAM(HKEY key, LPCWSTR subKey, LPCWSTR value, REGSAM regSam, BOOL *data) {
    DWORD dwType = REG_NONE;
    DWORD dwData;
    DWORD cbData = sizeof(dwData);
    LSTATUS lRes =
        RegGetValueW(key, subKey, value, ((regSam & 0x100) << 8) | RRF_RT_REG_DWORD | RRF_RT_REG_SZ | RRF_NOEXPAND,
                     &dwType, &dwData, &cbData);

    if (lRes != ERROR_SUCCESS) {
        if (lRes == ERROR_MORE_DATA)
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        if (lRes > 0)
            return HRESULT_FROM_WIN32(lRes);
        return lRes;
    }

    if (dwType == REG_DWORD) {
        if (dwData > 1)
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        *data = dwData == 1;
    } else {
        if (cbData != 4 || (WCHAR)dwData != L'0' && (WCHAR)dwData != L'1')
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        *data = (WCHAR)dwData == L'1';
    }

    return S_OK;
}

HRESULT SHRegGetDWORD(HKEY hkey, const WCHAR *pwszSubKey, const WCHAR *pwszValue, DWORD *pdwData) {
    DWORD dwSize = sizeof(DWORD);
    LSTATUS lres = RegGetValueW(hkey, pwszSubKey, pwszValue, RRF_RT_REG_DWORD, nullptr, pdwData, &dwSize);
    return HRESULT_FROM_WIN32(lres);
}

HRESULT SHRegSetDWORD(HKEY hkey, const WCHAR *pwszSubKey, const WCHAR *pwszValue, DWORD dwData) {
    LSTATUS lres = _RegSetKeyValueWithSDDL(hkey, pwszSubKey, pwszValue, REG_DWORD, &dwData, sizeof(dwData), nullptr);
    return HRESULT_FROM_WIN32(lres);
}

extern "C" DWORD SHRegGetUSDWORDW(const WCHAR *pwszSubKey, const WCHAR *pwszValue, DWORD dwDefault) {
    DWORD dwValue = 0;
    if (FAILED(SHRegGetDWORD(HKEY_CURRENT_USER, pwszSubKey, pwszValue, &dwValue))) {
        if (FAILED(SHRegGetDWORD(HKEY_LOCAL_MACHINE, pwszSubKey, pwszValue, &dwValue)))
            return dwDefault;
    }
    return dwValue;
}

#define INIT_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                                                     \
    EXTERN_C const GUID DECLSPEC_SELECTANY name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

INIT_GUID(CLSID_TrayUIComponent, 0x88FC85D3, 0x7090, 0x4F53, 0x8F, 0x7A, 0xEB, 0x02, 0x68, 0x16, 0x27, 0x88);
INIT_GUID(IID_ITrayUI, 0x12b454e1, 0x6e50, 0x42b8, 0xbc, 0x3e, 0xae, 0x7f, 0x54, 0x91, 0x99, 0xd6);
INIT_GUID(IID_ITrayUIComponent, 0x27775f88, 0x01d3, 0x46ec, 0xa1, 0xc1, 0x64, 0xb4, 0xc0, 0x9b, 0x21, 0x1b);

interface ITrayUIComponent // : IInspectable
{
    const struct ITrayUIComponentVtbl *lpVtbl;
};
typedef interface ITrayUIHost ITrayUIHost;

typedef interface ITrayUI ITrayUI;

typedef struct ITrayUIComponentVtbl // : IUnknownVtbl
{
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE *QueryInterface)(__RPC__in ITrayUIComponent *This,
                                               /* [in] */ __RPC__in REFIID riid,
                                               /* [annotation][iid_is][out] */
                                               _COM_Outptr_ void **ppvObject);

    ULONG(STDMETHODCALLTYPE *AddRef)(__RPC__in ITrayUIComponent *This);

    ULONG(STDMETHODCALLTYPE *Release)(__RPC__in ITrayUIComponent *This);

    HRESULT(STDMETHODCALLTYPE *InitializeWithTray)(__RPC__in ITrayUIComponent *This,
                                                   /* [in] */ __RPC__in ITrayUIHost *host,
                                                   /* [out] */ __RPC__out ITrayUI **result);
    END_INTERFACE
} ITrayUIComponentVtbl;

typedef HRESULT (*explorer_TrayUI_CreateInstanceFunc_t)(ITrayUIHost *host, REFIID riid, void **ppv);
explorer_TrayUI_CreateInstanceFunc_t explorer_TrayUI_CreateInstanceFunc;

static ULONG STDMETHODCALLTYPE nimplAddRefRelease(ITrayUIComponent *This) { return 1; }

static HRESULT STDMETHODCALLTYPE ITrayUIComponent_QueryInterface(ITrayUIComponent *This, REFIID riid,
                                                                 void **ppvObject) {
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE ITrayUIComponent_InitializeWithTray(ITrayUIComponent *This, ITrayUIHost *host,
                                                                     ITrayUI **result) {
    HRESULT res = explorer_TrayUI_CreateInstanceFunc(host, IID_ITrayUI, (void **)result);
    Wh_Log(L"Initialized tray");
    return res;
}

static const ITrayUIComponentVtbl instanceof_ITrayUIComponentVtbl = {.QueryInterface = ITrayUIComponent_QueryInterface,
                                                                     .AddRef = nimplAddRefRelease,
                                                                     .Release = nimplAddRefRelease,
                                                                     .InitializeWithTray =
                                                                         ITrayUIComponent_InitializeWithTray};
static const ITrayUIComponent instanceof_ITrayUIComponent = {&instanceof_ITrayUIComponentVtbl};

using CoCreateInstance_t = decltype(CoCreateInstance);
CoCreateInstance_t *CoCreateInstance_orig;

HRESULT CoCreateInstance_hook(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, void **ppv) {
    if (IsEqualGUID(rclsid, CLSID_TrayUIComponent) && IsEqualGUID(riid, IID_ITrayUIComponent)) {
        if (explorer_TrayUI_CreateInstanceFunc) {
            Wh_Log(L"OK!");
            SHRegSetDWORD(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"eptRetries",
                          0);
            *ppv = (void *)&instanceof_ITrayUIComponent;
            return S_OK;
        }
    }

    return CoCreateInstance_orig(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

#ifdef _WIN64
const size_t OFFSET_SAME_TEB_FLAGS = 0x17EE;
#else
const size_t OFFSET_SAME_TEB_FLAGS = 0x0FCA;
#endif

enum ImmersiveContextMenuOptions {
    ICMO_NONE = 0x0,
    ICMO_USEPPI = 0x1,
    ICMO_OVERRIDECOMPATCHECK = 0x2,
    ICMO_FORCEMOUSESTYLING = 0x4,
    ICMO_USESYSTEMTHEME = 0x8,
    ICMO_ICMBRUSHAPPLIED = 0x10,
};

typedef void (*SetImmersiveMenuFunctions_t)(void *a, void *b, void *c);

inline constexpr bool EqualsIgnoreCase(const std::wstring &a, const std::wstring &b) {
    if (a.length() != b.length())
        return false;
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

void KillExplorerInstances() {
    DWORD currentProcessId = GetCurrentProcessId();
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        Wh_Log(L"Failed to take process snapshot.");
        return;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            std::wstring processName = pe.szExeFile;

            if (EqualsIgnoreCase(processName, L"explorer.exe") && pe.th32ProcessID != currentProcessId) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    if (TerminateProcess(hProcess, 0)) {
                        Wh_Log(L"Terminated explorer.exe with PID %u", pe.th32ProcessID);
                    } else {
                        Wh_Log(L"Failed to terminate explorer.exe with PID %u", pe.th32ProcessID);
                    }
                    CloseHandle(hProcess);
                } else {
                    Wh_Log(L"Failed to open process %i", pe.th32ProcessID);
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}

typedef DWORD (*RtlGetVersion_t)(_Out_ PRTL_OSVERSIONINFOW lpVersionInformation);

#ifdef __x86_64__
#define ARCH_STR L"amd64"
#elif defined(__x86__)
#error "x86 is not supported"
#else
#define ARCH_STR L"arm64"
#endif

inline const std::optional<std::wstring> PickTaskbarDll() {
    RTL_OSVERSIONINFOEXW info;
    ZeroMemory(&info, sizeof(info));
    info.dwOSVersionInfoSize = sizeof(info);

    HMODULE ntos = LoadLibraryW(L"ntdll.dll");
    RtlGetVersion_t RtlGetVersion;
    RtlGetVersion = (RtlGetVersion_t)GetProcAddress(ntos, "RtlGetVersion");

    if (RtlGetVersion == NULL) {
        Wh_Log(L"RtlGetVersion was not found");
        return std::wstring();
    }
    RtlGetVersion((OSVERSIONINFOW *)&info);

    DWORD b = info.dwBuildNumber;

    PCWSTR res = Wh_GetStringSetting(L"pinned_version");
    if (wcslen(res) > 0) {
        std::wstring pinned = res;
        Wh_FreeStringSetting(res);
        return pinned;
    }
    Wh_FreeStringSetting(res);

    if (b == 15063                     // Windows 10 1703
        || b == 16299                  // Windows 10 1709
        || b == 17134                  // Windows 10 1803
        || b == 17763                  // Windows 10 1809
        || (b >= 18362 && b <= 18363)  // Windows 10 1903, 1909
        || (b >= 19041 && b <= 19045)) // Windows 10 20H2, 21H2, 22H2
    {
        return L"ep_taskbar.0." ARCH_STR ".dll";
    }

    if (b >= 21343 && b <= 22000) // Windows 11 21H2
    {
        return L"ep_taskbar.1." ARCH_STR ".dll";
    }

    if ((b >= 22621 && b <= 22635)     // 22H2-23H2 Release, Release Preview, and Beta channels
        || (b >= 23403 && b <= 25197)) // Early pre-reboot Dev channel until
                                       // post-reboot Dev channel
    {
        return L"ep_taskbar.2." ARCH_STR ".dll";
    }

    if (b >= 25201 && b <= 25915) // Pre-reboot Dev channel until early Canary
                                  // channel, nuked ITrayComponentHost methods
                                  // related to classic search box
    {
        return L"ep_taskbar.3." ARCH_STR ".dll";
    }

    if (b >= 25921 && b <= 26040) // Canary channel with nuked classic system tray
    {
        return L"ep_taskbar.4." ARCH_STR ".dll";
    }

    if (b >= 26052) // Same as 4 but with 2 new methods in ITrayComponentHost
                    // between GetTrayUI and ProgrammableTaskbarReportClick
    {
        return L"ep_taskbar.5." ARCH_STR ".dll";
    }

    return L""; // Empty = failure
}

BOOL FileExists(LPCTSTR szPath) {
    DWORD dwAttrib = GetFileAttributes(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

BOOL Wh_ModInit() {
    HWND hwnd = FindWindowExW(NULL, NULL, L"Shell_TrayWnd", NULL);
    bool shouldBecause = false;
    if (hwnd) {
        DWORD id;
        GetWindowThreadProcessId(hwnd, &id);
        shouldBecause = id == GetCurrentProcessId();
        if (!shouldBecause)
            return TRUE;
    }

    PCWSTR installPath = Wh_GetStringSetting(L"install_path");
    std::optional<std::wstring> dllVer = PickTaskbarDll();
    // We didn't detect a compatible version, exit
    if (!dllVer)
        return FALSE;
    std::wstring finalInstallPath = installPath;
    Wh_FreeStringSetting(installPath);
    if (finalInstallPath.length() == 0) {
        finalInstallPath = L"C:\\Program Files\\ep_taskbar";
    }
    std::wstring dll = finalInstallPath + L"\\" + dllVer.value();
    if (dll.length() > 256) {
        Wh_Log(L"DLL path is too long");
        return FALSE;
    }
    if (!FileExists(dll.c_str())) {
        Wh_Log(L"ep_taskbar was not found! Are you sure the correct version was installed? Tried: %s", dll.c_str());
        return FALSE;
    }
    Wh_Log(L"Final DLL path: %s", dll.c_str());

    // We call CoCreateInstance, it will do nothing and error if it was loaded,
    // or throw a different error if it wasn't initialized
    GUID empty = GUID{};
    void *ptr = 0;
    HRESULT hr = CoCreateInstance(empty, NULL, 0, empty, &ptr);
    // -2147221008 is the error code returned by CoCreateInstance if
    // CoInitialize wasn't called yet
    bool isBeforeCoInit = hr == -2147221008;

    if (shouldBecause) {
        HMODULE mod = GetModuleHandle(dll.c_str());
        shouldBecause = mod == 0;
    }

    if (!isBeforeCoInit && shouldBecause) {
        // Should help with bootloops?
        // I haven't tested this yet
        DWORD tries = 0;
        SHRegGetDWORD(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"eptRetries",
                      &tries);
        if (tries > 5) {
            Wh_Log(L"ep_taskbar_loader failed to load ep_taskbar 5 times, we will exit now");
            SHRegSetDWORD(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"eptRetries",
                          0);
            return TRUE;
        }
        SHRegSetDWORD(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"eptRetries",
                      tries + 1);

        // Don't ask me how, don't ask me why, useful for debugging
        bool isInitialThread = *(USHORT *)((BYTE *)NtCurrentTeb() + OFFSET_SAME_TEB_FLAGS) & 0x0400;

        Wh_Log(L"Looping %i %i %i", GetModuleHandle(dll.c_str()), isInitialThread, shouldBecause);
        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        wchar_t cmdline[] = L"explorer.exe";

        HRESULT _ = CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        TerminateProcess(GetCurrentProcess(), 0); // Ensure we kill ourselves
        return FALSE;
    };
    CoInitialize(0);

    // Kill other instances of explorer, otherwise it'll exit, believing another
    // copy of explorer is running and that it should use that one instead
    KillExplorerInstances();

    HMODULE hTwinuiPcshell = LoadLibraryW(L"twinui.pcshell.dll");

    void *CImmersiveContextMenuOwnerDrawHelper__s_ContextMenuWndProc = 0;
    void *ImmersiveContextMenuHelper__ApplyOwnerDrawToMenuFunc = 0;
    void *ImmersiveContextMenuHelper__RemoveOwnerDrawFromMenuFunc = 0;

    WH_FIND_SYMBOL_OPTIONS opts = WH_FIND_SYMBOL_OPTIONS{sizeof(WH_FIND_SYMBOL_OPTIONS), NULL, FALSE};
    WH_FIND_SYMBOL sym = WH_FIND_SYMBOL{0, 0, 0};
    HANDLE hand = Wh_FindFirstSymbol(hTwinuiPcshell, &opts, &sym);

    BOOL next = hand != 0;
    while (next) {
        if (sym.symbolDecorated == 0) {
            Wh_Log(L"Symbol is not decorated, this shouldn't happen");
        }
        if (0 ==
            wcscmp(sym.symbol,
                   L"public: static bool __cdecl CImmersiveContextMenuOwnerDrawHelper::s_ContextMenuWndProc(struct "
                   L"HWND__ *,unsigned int,unsigned __int64,__int64,bool *,enum GRID_HOW_SELECTED_FLAGS *)")) {
            // Wh_Log(L"MATCH!");
            CImmersiveContextMenuOwnerDrawHelper__s_ContextMenuWndProc = sym.address;
        } else if (0 == wcscmp(sym.symbol,
                               L"long __cdecl ImmersiveContextMenuHelper::ApplyOwnerDrawToMenu(struct HMENU__ *,struct "
                               L"HWND__ *,struct tagPOINT *,enum ImmersiveContextMenuOptions,class "
                               L"CSimplePointerArrayNewMem<struct ContextMenuRenderingData,class "
                               L"CSimpleArrayStandardCompareHelper<struct ContextMenuRenderingData *> > &)")) {
            ImmersiveContextMenuHelper__ApplyOwnerDrawToMenuFunc = sym.address;
        } else if (0 == wcscmp(sym.symbol,
                               L"void __cdecl ImmersiveContextMenuHelper::RemoveOwnerDrawFromMenu(struct HMENU__ "
                               L"*,struct HWND__ *)")) {
            ImmersiveContextMenuHelper__RemoveOwnerDrawFromMenuFunc = sym.address;
        }
        next = Wh_FindNextSymbol(hand, &sym);
    }

    HMODULE hEpTaskbar = LoadLibraryW(dll.c_str());
    if (hEpTaskbar == NULL) {
        Wh_Log(L"Could not load DLL");
        return FALSE;
    }

    if (CImmersiveContextMenuOwnerDrawHelper__s_ContextMenuWndProc &&
        ImmersiveContextMenuHelper__ApplyOwnerDrawToMenuFunc &&
        ImmersiveContextMenuHelper__RemoveOwnerDrawFromMenuFunc) {
        Wh_Log(L"We found the immersive menu functions!");
        SetImmersiveMenuFunctions_t SetImmersiveMenuFunctions =
            (SetImmersiveMenuFunctions_t)GetProcAddress(hEpTaskbar, "SetImmersiveMenuFunctions");
        if (SetImmersiveMenuFunctions == NULL) {
            Wh_Log(L"SetImmersiveMenuFunctions was not found!");
            return FALSE;
        }
        SetImmersiveMenuFunctions(CImmersiveContextMenuOwnerDrawHelper__s_ContextMenuWndProc,
                                  ImmersiveContextMenuHelper__ApplyOwnerDrawToMenuFunc,
                                  ImmersiveContextMenuHelper__RemoveOwnerDrawFromMenuFunc);
    } else {
        Wh_Log(L"Failed finding one or more symbols, do you have an internet connection?");
    };
    Wh_Log(L"ptrs: %li %li %li", CImmersiveContextMenuOwnerDrawHelper__s_ContextMenuWndProc,
           ImmersiveContextMenuHelper__ApplyOwnerDrawToMenuFunc,
           ImmersiveContextMenuHelper__RemoveOwnerDrawFromMenuFunc);

    explorer_TrayUI_CreateInstanceFunc = reinterpret_cast<decltype(explorer_TrayUI_CreateInstanceFunc)>(
        GetProcAddress(hEpTaskbar, "EP_TrayUI_CreateInstance"));
    if (explorer_TrayUI_CreateInstanceFunc == NULL) {
        Wh_Log(L"Could not find EP_TrayUI_CreateInstance");
        return FALSE;
    }

    Wh_SetFunctionHook((void *)CoCreateInstance, (void *)CoCreateInstance_hook, (void **)&CoCreateInstance_orig);

    return TRUE;
}

void Wh_AfterInit() { Wh_Log(L"Afterinit"); }

void Wh_ModUninit() { Wh_Log(L"Uninit"); }

/*
License:

MIT License

Copyright (c) 2025 Reabstraction

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
