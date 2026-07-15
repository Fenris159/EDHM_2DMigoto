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
