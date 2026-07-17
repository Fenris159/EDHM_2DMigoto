#pragma once

// Wine / Proton detection and Linux-oriented runtime helpers for EDHM_2DMigoto.
// Detection is best-effort: ntdll wine exports are the primary signal.

// True when running under Wine (including Proton, which is Wine-based).
bool DetectWineEnvironment();

// Wine version string from wine_get_version(), or empty if not Wine / unknown.
// Pointer is valid for the process lifetime after the first call.
const char* GetWineVersionString();

// Short label for logs: "Wine 9.0", "Wine (version unknown)", or "Windows".
const char* GetHostPlatformLabel();

// wine_compat ini: -1 = auto (on under Wine), 0 = force off, 1 = force on.
// When the profile is applied:
//   load_library_redirect -> 0  (do not fight DXVK system32 d3d11)
//   check_foreground_window -> false  (Wine focus is unreliable)
// Explicit wine_compat=0 leaves Windows-oriented values alone even under Wine.
bool ApplyWineCompatProfile(
	int wine_compat_ini,
	int* load_library_redirect,
	bool* check_foreground_window);

// One-shot summary for d3d11_log.txt so Linux users can self-diagnose load issues.
void LogHostCompatReport();
