// ==WindhawkMod==
// @id              hide-loading-cursor
// @name            Hide loading cursor
// @description     Replace the "working in background" cursor with the normal arrow cursor
// @version         1.0.0
// @author          Roosmsg
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -ladvapi32
// ==/WindhawkMod==
// ==WindhawkModReadme==
/*
# Hide loading cursor
Replaces the "working in background" cursor (arrow + spinner) with the normal
arrow cursor. Disabling the mod restores your cursor scheme.
*/
// ==/WindhawkModReadme==
#include <windhawk_utils.h>

#include <windows.h>

#include <string>

namespace {

constexpr wchar_t kCursorsRegPath[] = L"Control Panel\\Cursors";
constexpr wchar_t kModRegPath[] = L"Software\\WindhawkMods\\hide-loading-cursor";
constexpr wchar_t kArrowValueName[] = L"Arrow";
constexpr wchar_t kAppStartingValueName[] = L"AppStarting";
constexpr wchar_t kAppliedValueName[] = L"Applied";
constexpr wchar_t kPrevExistsValueName[] = L"PrevAppStartingExists";
constexpr wchar_t kPrevTypeValueName[] = L"PrevAppStartingType";
constexpr wchar_t kPrevValueValueName[] = L"PrevAppStartingValue";

bool g_applied = false;

bool QueryStringValue(HKEY key,
                      const wchar_t* name,
                      std::wstring* value,
                      DWORD* type) {
    DWORD localType = 0;
    DWORD bytes = 0;
    LONG status =
        RegQueryValueExW(key, name, nullptr, &localType, nullptr, &bytes);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    if (localType != REG_SZ && localType != REG_EXPAND_SZ) {
        return false;
    }

    std::wstring buffer;
    buffer.resize(bytes / sizeof(wchar_t));
    status = RegQueryValueExW(
        key, name, nullptr, &localType,
        reinterpret_cast<BYTE*>(&buffer[0]), &bytes);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    if (!buffer.empty() && buffer.back() == L'\0') {
        buffer.pop_back();
    }

    if (value) {
        *value = buffer;
    }
    if (type) {
        *type = localType;
    }

    return true;
}

bool QueryDwordValue(HKEY key, const wchar_t* name, DWORD* value) {
    DWORD localType = 0;
    DWORD data = 0;
    DWORD size = sizeof(data);
    LONG status = RegQueryValueExW(key, name, nullptr, &localType,
                                   reinterpret_cast<BYTE*>(&data), &size);
    if (status != ERROR_SUCCESS || localType != REG_DWORD) {
        return false;
    }
    if (value) {
        *value = data;
    }
    return true;
}

bool SetDwordValue(HKEY key, const wchar_t* name, DWORD value) {
    return RegSetValueExW(key, name, 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&value),
                          sizeof(value)) == ERROR_SUCCESS;
}

bool SetStringValue(HKEY key,
                    const wchar_t* name,
                    const std::wstring& value,
                    DWORD type) {
    DWORD localType = type;
    if (localType != REG_SZ && localType != REG_EXPAND_SZ) {
        localType = REG_SZ;
    }
    return RegSetValueExW(
               key, name, 0, localType,
               reinterpret_cast<const BYTE*>(value.c_str()),
               static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) ==
           ERROR_SUCCESS;
}

bool GetWindowsArrowPath(std::wstring* path) {
    wchar_t windowsDir[MAX_PATH];
    UINT len = GetWindowsDirectoryW(windowsDir, ARRAYSIZE(windowsDir));
    if (len == 0 || len >= ARRAYSIZE(windowsDir)) {
        return false;
    }

    std::wstring value = windowsDir;
    value += L"\\Cursors\\aero_arrow.cur";
    if (path) {
        *path = value;
    }

    return true;
}

bool StoreOriginalAppStarting(HKEY modKey, HKEY cursorsKey) {
    std::wstring appStarting;
    DWORD appStartingType = REG_SZ;
    if (QueryStringValue(cursorsKey, kAppStartingValueName, &appStarting,
                         &appStartingType)) {
        if (!SetDwordValue(modKey, kPrevExistsValueName, 1) ||
            !SetDwordValue(modKey, kPrevTypeValueName, appStartingType) ||
            !SetStringValue(modKey, kPrevValueValueName, appStarting,
                            appStartingType)) {
            return false;
        }
    } else {
        if (!SetDwordValue(modKey, kPrevExistsValueName, 0)) {
            return false;
        }
        RegDeleteValueW(modKey, kPrevTypeValueName);
        RegDeleteValueW(modKey, kPrevValueValueName);
    }

    return true;
}

bool ApplyCursorOverride() {
    HKEY modKey = nullptr;
    LONG status = RegCreateKeyExW(
        HKEY_CURRENT_USER, kModRegPath, 0, nullptr, 0,
        KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &modKey, nullptr);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    DWORD applied = 0;
    bool alreadyApplied =
        QueryDwordValue(modKey, kAppliedValueName, &applied) && applied != 0;

    HKEY cursorsKey = nullptr;
    status = RegOpenKeyExW(HKEY_CURRENT_USER, kCursorsRegPath, 0,
                           KEY_QUERY_VALUE | KEY_SET_VALUE, &cursorsKey);
    if (status != ERROR_SUCCESS) {
        RegCloseKey(modKey);
        return false;
    }

    if (!alreadyApplied && !StoreOriginalAppStarting(modKey, cursorsKey)) {
        RegCloseKey(cursorsKey);
        RegCloseKey(modKey);
        return false;
    }

    std::wstring arrowValue;
    DWORD arrowType = REG_SZ;
    if (!QueryStringValue(cursorsKey, kArrowValueName, &arrowValue,
                          &arrowType) ||
        arrowValue.empty()) {
        if (!GetWindowsArrowPath(&arrowValue)) {
            RegCloseKey(cursorsKey);
            RegCloseKey(modKey);
            return false;
        }
        arrowType = REG_SZ;
    }

    bool needsUpdate = true;
    std::wstring currentAppStarting;
    DWORD currentType = REG_SZ;
    if (QueryStringValue(cursorsKey, kAppStartingValueName,
                         &currentAppStarting, &currentType) &&
        _wcsicmp(currentAppStarting.c_str(), arrowValue.c_str()) == 0) {
        needsUpdate = false;
    }

    if (needsUpdate) {
        status = RegSetValueExW(
            cursorsKey, kAppStartingValueName, 0, arrowType,
            reinterpret_cast<const BYTE*>(arrowValue.c_str()),
            static_cast<DWORD>((arrowValue.size() + 1) * sizeof(wchar_t)));
        if (status != ERROR_SUCCESS) {
            RegCloseKey(cursorsKey);
            RegCloseKey(modKey);
            return false;
        }
    }

    if (!alreadyApplied && !SetDwordValue(modKey, kAppliedValueName, 1)) {
        RegCloseKey(cursorsKey);
        RegCloseKey(modKey);
        return false;
    }

    RegCloseKey(cursorsKey);
    RegCloseKey(modKey);

    if (needsUpdate) {
        SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr,
                              SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }

    return true;
}

void RestoreCursors() {
    HKEY modKey = nullptr;
    LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, kModRegPath, 0,
                                KEY_QUERY_VALUE, &modKey);
    if (status != ERROR_SUCCESS) {
        return;
    }

    DWORD applied = 0;
    if (!QueryDwordValue(modKey, kAppliedValueName, &applied) || applied == 0) {
        RegCloseKey(modKey);
        return;
    }

    DWORD prevExists = 0;
    QueryDwordValue(modKey, kPrevExistsValueName, &prevExists);

    DWORD prevType = REG_SZ;
    QueryDwordValue(modKey, kPrevTypeValueName, &prevType);

    std::wstring prevValue;
    bool hasPrevValue =
        QueryStringValue(modKey, kPrevValueValueName, &prevValue, nullptr);

    RegCloseKey(modKey);

    HKEY cursorsKey = nullptr;
    status = RegOpenKeyExW(HKEY_CURRENT_USER, kCursorsRegPath, 0, KEY_SET_VALUE,
                           &cursorsKey);
    if (status != ERROR_SUCCESS) {
        return;
    }

    if (prevExists != 0 && hasPrevValue) {
        RegSetValueExW(cursorsKey, kAppStartingValueName, 0, prevType,
                       reinterpret_cast<const BYTE*>(prevValue.c_str()),
                       static_cast<DWORD>((prevValue.size() + 1) *
                                          sizeof(wchar_t)));
    } else {
        RegDeleteValueW(cursorsKey, kAppStartingValueName);
    }

    RegCloseKey(cursorsKey);

    SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    RegDeleteKeyW(HKEY_CURRENT_USER, kModRegPath);
}

}  // namespace

BOOL Wh_ModInit() {
    g_applied = ApplyCursorOverride();
    return g_applied;
}

void Wh_ModUninit() {
    if (g_applied) {
        RestoreCursors();
    }
}
