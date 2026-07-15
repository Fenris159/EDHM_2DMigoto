#include "WineCompat.h"

#include "log.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

namespace {

typedef const char* (__cdecl *wine_get_version_fn)(void);

bool g_wine_detected = false;
bool g_wine_detection_done = false;
char g_wine_version[128] = {};
char g_platform_label[160] = {};

wine_get_version_fn ResolveWineGetVersion()
{
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll)
		return nullptr;

	// Classic Wine export. Present under Proton as well.
	return reinterpret_cast<wine_get_version_fn>(
		GetProcAddress(ntdll, "wine_get_version"));
}

void EnsureWineDetection()
{
	if (g_wine_detection_done)
		return;
	g_wine_detection_done = true;

	wine_get_version_fn wine_get_version = ResolveWineGetVersion();
	if (wine_get_version) {
		g_wine_detected = true;
		const char* ver = wine_get_version();
		if (ver && ver[0]) {
			strncpy_s(g_wine_version, ver, _TRUNCATE);
			_snprintf_s(g_platform_label, _TRUNCATE, "Wine %s", g_wine_version);
		} else {
			strcpy_s(g_platform_label, "Wine (version unknown)");
		}
		return;
	}

	// Secondary signals (rare if ntdll export is missing, but cheap).
	char env[8];
	if (GetEnvironmentVariableA("WINEPREFIX", env, ARRAYSIZE(env)) > 0 ||
	    GetEnvironmentVariableA("WINELOADER", env, ARRAYSIZE(env)) > 0 ||
	    GetEnvironmentVariableA("WINEDEBUG", env, ARRAYSIZE(env)) > 0) {
		g_wine_detected = true;
		strcpy_s(g_platform_label, "Wine (env heuristic)");
		return;
	}

	strcpy_s(g_platform_label, "Windows");
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
	bool dll_initialization_delay_key_present,
	int* load_library_redirect,
	bool* check_foreground_window,
	int* dll_initialization_delay)
{
	if (!load_library_redirect || !check_foreground_window || !dll_initialization_delay)
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

	// Small settle delay when the user did not set one; helps early DXVK init.
	if (!dll_initialization_delay_key_present && *dll_initialization_delay <= 0)
		*dll_initialization_delay = 250;

	LogInfo("WineCompat: applying Linux-safe profile (platform=%s)\n", GetHostPlatformLabel());
	LogInfo("  load_library_redirect = %d\n", *load_library_redirect);
	LogInfo("  check_foreground_window = %d\n", *check_foreground_window ? 1 : 0);
	LogInfo("  dll_initialization_delay = %d\n", *dll_initialization_delay);
	return true;
}
