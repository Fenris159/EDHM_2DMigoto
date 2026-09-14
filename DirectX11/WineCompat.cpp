#include "WineCompat.h"

#include "log.h"
#include "globals.h"

#include <Windows.h>

#include <array>
#include <cctype>
#include <stdio.h>
#include <string.h>

namespace {

typedef const char* (__cdecl *wine_get_version_fn)(void);

struct WineDetectionState {
	bool detected = false;
	std::array<char, 128> version = {};
	std::array<char, 160> platform_label = {};
	INIT_ONCE initialization_once = INIT_ONCE_STATIC_INIT;
};

WineDetectionState& GetWineDetectionState()
{
	static WineDetectionState state;
	return state;
}

template <size_t Size>
void SanitizeForLog(std::array<char, Size>& value)
{
	for (char& character : value) {
		const unsigned char byte = static_cast<unsigned char>(character);
		if (character == '\0')
			break;
		if (!std::isprint(byte))
			character = '?';
	}
}

bool HasEnvironmentVariable(const char* name)
{
	return name && GetEnvironmentVariableA(name, nullptr, 0) > 0;
}

wine_get_version_fn ResolveWineGetVersion()
{
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll)
		return nullptr;

	// Classic Wine export. Present under Proton as well.
	return reinterpret_cast<wine_get_version_fn>(
		GetProcAddress(ntdll, "wine_get_version"));
}

BOOL CALLBACK InitializeWineDetection(PINIT_ONCE, PVOID, PVOID*)
{
	WineDetectionState& state = GetWineDetectionState();
	wine_get_version_fn wine_get_version = ResolveWineGetVersion();
	if (wine_get_version) {
		state.detected = true;
		const char* ver = wine_get_version();
		const bool proton = HasEnvironmentVariable("STEAM_COMPAT_DATA_PATH");
		if (ver && ver[0]) {
			strncpy_s(state.version.data(), state.version.size(), ver, _TRUNCATE);
			SanitizeForLog(state.version);
			_snprintf_s(state.platform_label.data(), state.platform_label.size(), _TRUNCATE,
				proton ? "Proton (Wine %s)" : "Wine %s", state.version.data());
		} else {
			strcpy_s(state.platform_label.data(), state.platform_label.size(),
				proton ? "Proton/Wine (version unknown)" : "Wine (version unknown)");
		}
		return TRUE;
	}

	// Secondary signals (rare if ntdll export is missing, but cheap).
	if (HasEnvironmentVariable("WINEPREFIX") ||
	    HasEnvironmentVariable("WINELOADER") ||
	    HasEnvironmentVariable("WINEDEBUG")) {
		state.detected = true;
		strcpy_s(state.platform_label.data(), state.platform_label.size(), "Wine (env heuristic)");
		return TRUE;
	}

	strcpy_s(state.platform_label.data(), state.platform_label.size(), "Windows");
	return TRUE;
}

void EnsureWineDetection()
{
	WineDetectionState& state = GetWineDetectionState();
	InitOnceExecuteOnce(&state.initialization_once, InitializeWineDetection, nullptr, nullptr);
}

bool EnvironmentOptionEnabled(const char* name)
{
	std::array<char, 32> value = {};
	DWORD length = GetEnvironmentVariableA(name, value.data(), static_cast<DWORD>(value.size()));
	if (!length)
		return false;
	if (length >= value.size())
		return true;
	return strcmp(value.data(), "0") != 0;
}

void LogDllOverrides()
{
	std::array<char, 1024> value = {};
	DWORD length = GetEnvironmentVariableA(
		"WINEDLLOVERRIDES", value.data(), static_cast<DWORD>(value.size()));
	if (!length) {
		LogInfo("  WINEDLLOVERRIDES: not present in process environment "
			"(a winecfg override may still be active)\n");
		return;
	}
	if (length >= value.size()) {
		LogInfo("  WINEDLLOVERRIDES: present but too long to print safely\n");
		return;
	}
	SanitizeForLog(value);
	LogInfo("  WINEDLLOVERRIDES: %s\n", value.data());
}

void LogFlatpakEnvironment()
{
	std::array<char, 256> flatpak_id = {};
	DWORD length = GetEnvironmentVariableA(
		"FLATPAK_ID", flatpak_id.data(), static_cast<DWORD>(flatpak_id.size()));
	if (!length) {
		LogInfo("  Flatpak sandbox: not detected in process environment\n");
		return;
	}
	if (length >= flatpak_id.size()) {
		LogInfo("  Flatpak sandbox: detected (FLATPAK_ID too long to print safely)\n");
		return;
	}
	SanitizeForLog(flatpak_id);
	LogInfo("  Flatpak sandbox: detected (%s)\n", flatpak_id.data());
}

} // namespace

bool DetectWineEnvironment()
{
	EnsureWineDetection();
	return GetWineDetectionState().detected;
}

const char* GetWineVersionString()
{
	EnsureWineDetection();
	return GetWineDetectionState().version.data();
}

const char* GetHostPlatformLabel()
{
	EnsureWineDetection();
	return GetWineDetectionState().platform_label.data();
}

bool ApplyWineCompatProfile(
	int wine_compat_ini,
	int* load_library_redirect,
	bool* check_foreground_window)
{
	if (!load_library_redirect || !check_foreground_window)
		return false;

	const bool wine = DetectWineEnvironment();
	bool apply = false;

	if (wine_compat_ini == 1)
		apply = true;
	else if (wine_compat_ini == 0)
		apply = false;
	else
		apply = wine; // auto

	if (!apply) {
		LogInfo("WineCompat: profile not applied (wine_compat=%d, platform=%s)\n",
			wine_compat_ini, GetHostPlatformLabel());
		return false;
	}

	// Force DXVK-friendly load behaviour. Redirect=2 pulls system d3d11 back
	// into the game folder and often breaks the EDHM 3Dmigoto -> DXVK chain under Wine.
	*load_library_redirect = 0;
	*check_foreground_window = false;

	LogInfo("WineCompat: applying Linux-safe profile (platform=%s)\n", GetHostPlatformLabel());
	LogInfo("  load_library_redirect = %d\n", *load_library_redirect);
	LogInfo("  check_foreground_window = %d\n", *check_foreground_window ? 1 : 0);
	return true;
}

void LogHostCompatReport()
{
	std::array<wchar_t, MAX_PATH> migoto_path = {};
	std::array<wchar_t, MAX_PATH> exe_path = {};

	DWORD migoto_path_len = GetModuleFileNameW(migoto_handle, migoto_path.data(), migoto_path.size());
	DWORD exe_path_len = GetModuleFileNameW(nullptr, exe_path.data(), exe_path.size());
	if (!migoto_path_len || migoto_path_len >= MAX_PATH)
		wcscpy_s(migoto_path.data(), migoto_path.size(), L"(unknown)");
	if (!exe_path_len || exe_path_len >= MAX_PATH)
		wcscpy_s(exe_path.data(), exe_path.size(), L"(unknown)");

	LogInfo("\n=== 3Dmigoto host compatibility report (EDHM profile) ===\n");
	LogInfo("  Platform: %s\n", GetHostPlatformLabel());
	LogInfo("  wine_compat ini: %d (-1=auto 0=off 1=on)\n", G ? G->wine_compat : -1);
	LogInfo("  wine_compat profile applied: %s\n",
		(G && G->wine_compat_profile_applied) ? "yes" : "no");
	if (G) {
		LogInfo("  load_library_redirect = %d\n", G->load_library_redirect);
		LogInfo("  check_foreground_window = %d\n", G->check_foreground_window ? 1 : 0);
		LogInfo("  dll_initialization_delay = %d\n", G->gDllInitializationDelay);
		LogInfo("  legacy proxy_d3d11 configured: %s\n", G->CHAIN_DLL_PATH[0] ? "yes" : "no");
	}
	LogDllOverrides();
	LogFlatpakEnvironment();
	if (EnvironmentOptionEnabled("PROTON_USE_WINED3D"))
		LogInfo("  WARNING: PROTON_USE_WINED3D is enabled; Proton will use OpenGL wined3d instead of DXVK.\n");
	if (EnvironmentOptionEnabled("PROTON_NO_D3D11"))
		LogInfo("  WARNING: PROTON_NO_D3D11 is enabled; remove it because Elite and EDHM require D3D11.\n");
	LogInfoW(L"  3Dmigoto d3d11.dll (EDHM build): %ls\n", migoto_path.data());
	LogInfoW(L"  Process: %ls\n", exe_path.data());
	LogInfo("  If this log file never appears under Wine/Proton:\n");
	LogInfo("    1) Place the EDHM configuration and 3Dmigoto d3d11.dll next to EliteDangerous64.exe (not only the launcher)\n");
	LogInfo("    2) Set WINEDLLOVERRIDES=d3d11=n,b (and d3dcompiler_47=n,b if needed)\n");
	LogInfo("    3) Do not replace the 3Dmigoto d3d11.dll used by EDHM with DXVK's d3d11.dll\n");
	LogInfo("=== end 3Dmigoto host compatibility report ===\n\n");
}
