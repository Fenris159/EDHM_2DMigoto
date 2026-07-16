#include "WineCompat.h"

#include "log.h"
#include "Globals.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

namespace {

typedef const char* (__cdecl *wine_get_version_fn)(void);

bool g_wine_detected = false;
char g_wine_version[128] = {};
char g_platform_label[160] = {};
INIT_ONCE g_wine_detection_once = INIT_ONCE_STATIC_INIT;

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
	wine_get_version_fn wine_get_version = ResolveWineGetVersion();
	if (wine_get_version) {
		g_wine_detected = true;
		const char* ver = wine_get_version();
		const bool proton = HasEnvironmentVariable("STEAM_COMPAT_DATA_PATH");
		if (ver && ver[0]) {
			strncpy_s(g_wine_version, ver, _TRUNCATE);
			_snprintf_s(g_platform_label, _TRUNCATE, proton ? "Proton (Wine %s)" : "Wine %s",
				g_wine_version);
		} else {
			strcpy_s(g_platform_label,
				proton ? "Proton/Wine (version unknown)" : "Wine (version unknown)");
		}
		return TRUE;
	}

	// Secondary signals (rare if ntdll export is missing, but cheap).
	if (HasEnvironmentVariable("WINEPREFIX") ||
	    HasEnvironmentVariable("WINELOADER") ||
	    HasEnvironmentVariable("WINEDEBUG")) {
		g_wine_detected = true;
		strcpy_s(g_platform_label, "Wine (env heuristic)");
		return TRUE;
	}

	strcpy_s(g_platform_label, "Windows");
	return TRUE;
}

void EnsureWineDetection()
{
	InitOnceExecuteOnce(&g_wine_detection_once, InitializeWineDetection, nullptr, nullptr);
}

bool EnvironmentOptionEnabled(const char* name)
{
	char value[32] = {};
	DWORD length = GetEnvironmentVariableA(name, value, ARRAYSIZE(value));
	if (!length)
		return false;
	if (length >= ARRAYSIZE(value))
		return true;
	return strcmp(value, "0") != 0;
}

void LogDllOverrides()
{
	char value[1024] = {};
	DWORD length = GetEnvironmentVariableA("WINEDLLOVERRIDES", value, ARRAYSIZE(value));
	if (!length) {
		LogInfo("  WINEDLLOVERRIDES: not present in process environment "
			"(a winecfg override may still be active)\n");
		return;
	}
	if (length >= ARRAYSIZE(value)) {
		LogInfo("  WINEDLLOVERRIDES: present but too long to print safely\n");
		return;
	}
	LogInfo("  WINEDLLOVERRIDES: %s\n", value);
}

void LogFlatpakEnvironment()
{
	char flatpak_id[256] = {};
	DWORD length = GetEnvironmentVariableA("FLATPAK_ID", flatpak_id, ARRAYSIZE(flatpak_id));
	if (!length) {
		LogInfo("  Flatpak sandbox: not detected in process environment\n");
		return;
	}
	if (length >= ARRAYSIZE(flatpak_id)) {
		LogInfo("  Flatpak sandbox: detected (FLATPAK_ID too long to print safely)\n");
		return;
	}
	LogInfo("  Flatpak sandbox: detected (%s)\n", flatpak_id);
}

} // namespace

bool DetectWineEnvironment()
{
	EnsureWineDetection();
	return g_wine_detected;
}

const char* GetWineVersionString()
{
	EnsureWineDetection();
	return g_wine_version;
}

const char* GetHostPlatformLabel()
{
	EnsureWineDetection();
	return g_platform_label;
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
	// into the game folder and often breaks the EDHM -> DXVK chain under Wine.
	*load_library_redirect = 0;
	*check_foreground_window = false;

	LogInfo("WineCompat: applying Linux-safe profile (platform=%s)\n", GetHostPlatformLabel());
	LogInfo("  load_library_redirect = %d\n", *load_library_redirect);
	LogInfo("  check_foreground_window = %d\n", *check_foreground_window ? 1 : 0);
	return true;
}

void LogHostCompatReport()
{
	wchar_t migoto_path[MAX_PATH] = {};
	wchar_t exe_path[MAX_PATH] = {};

	DWORD migoto_path_len = GetModuleFileNameW(migoto_handle, migoto_path, MAX_PATH);
	DWORD exe_path_len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
	if (!migoto_path_len || migoto_path_len >= MAX_PATH)
		wcscpy_s(migoto_path, L"(unknown)");
	if (!exe_path_len || exe_path_len >= MAX_PATH)
		wcscpy_s(exe_path, L"(unknown)");

	LogInfo("\n=== EDHM_2DMigoto host compatibility report ===\n");
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
	LogInfoW(L"  EDHM d3d11.dll: %ls\n", migoto_path);
	LogInfoW(L"  Process: %ls\n", exe_path);
	LogInfo("  If this log file never appears under Wine/Proton:\n");
	LogInfo("    1) Place EDHM files next to EliteDangerous64.exe (not only the launcher)\n");
	LogInfo("    2) Set WINEDLLOVERRIDES=d3d11=n,b (and d3dcompiler_47=n,b if needed)\n");
	LogInfo("    3) Do not replace EDHM's d3d11.dll with DXVK's d3d11.dll\n");
	LogInfo("=== end host compatibility report ===\n\n");
}
