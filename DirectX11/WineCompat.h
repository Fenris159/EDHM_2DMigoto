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
