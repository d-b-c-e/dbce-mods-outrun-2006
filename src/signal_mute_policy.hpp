#pragma once
#include <Windows.h>
#include <string_view>

// Child-process diagnostic opt-in. Cached on first use and cannot be toggled
// back to physical output by an INI edit, F6, an expired request or capture end.
// A misspelling still blocks output; it never falls back to ordinary play.
namespace OutRunSignalMute {
enum class Mode { Normal, LegacyConstant, Refused };
inline Mode Parse(std::wstring_view value, bool present) {
    return !present ? Mode::Normal : value == L"legacy" ? Mode::LegacyConstant : Mode::Refused;
}
inline Mode StartupMode() {
    static const Mode mode = [] {
        const DWORD saved = GetLastError();
        wchar_t value[32]{}; SetLastError(ERROR_SUCCESS);
        const DWORD n = GetEnvironmentVariableW(L"DBCE_OUTRUN_SIGNAL_MUTE", value, 32);
        const DWORD error = GetLastError();
        const Mode result = n >= 32 ? Mode::Refused : Parse(std::wstring_view(value, n),
            n != 0 || error != ERROR_ENVVAR_NOT_FOUND);
        SetLastError(saved); return result;
    }();
    return mode;
}
inline bool BlocksOutput() { return StartupMode() != Mode::Normal; }
}
