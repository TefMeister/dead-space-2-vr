/*
 * proxy.c — Dead Space 2, stage-1 d3d9 proxy.
 *
 * PURPOSE, AND THE REASON IT IS THIS SMALL.
 *
 * `deadspace2.exe` statically imports `activation.x86.dll` (one function, `start`),
 * so EA's activation layer runs before any game code. The open question for this
 * whole project is simply: DOES THAT LAYER TOLERATE A FOREIGN DLL IN THE GAME
 * FOLDER AT ALL? Everything else — camera hunting, stereo, head tracking — is
 * downstream of that one answer.
 *
 * So this file deliberately does almost nothing. It forwards Direct3DCreate9 to
 * the real system d3d9.dll and writes a handful of lines to a log. If the game
 * still reaches its menu, the route is open and the mature proxy in
 * `staging/alan-wake-vr/proxy-d3d9/` (which already hunts view-projection
 * matrices through SetVertexShaderConstantF) can be ported in as stage 2.
 *
 * If instead the game refuses to start, the cause is unambiguous BECAUSE this
 * file is trivial — there is no hooking, no vtable patching and no allocation to
 * blame. A bigger stage-1 proxy would have made a failure uninterpretable, which
 * is the whole reason it is not one.
 *
 * REVERSIBILITY: delete the d3d9.dll next to deadspace2.exe. Nothing else is
 * touched — no game file is modified, no registry key is written.
 *
 * Built 32-bit: deadspace2.exe is PE32/i386.
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef void *(WINAPI *PFN_Direct3DCreate9)(UINT);

static HMODULE  g_self;
static HMODULE  g_real;
static PFN_Direct3DCreate9 g_real_create;
static char     g_logpath[MAX_PATH];
static CRITICAL_SECTION g_loglock;
static int      g_loglock_ready;

/* ---------------------------------------------------------------- logging */

/* Pick a log location that is certain to be writable. The game folder is the
 * convenient place and is tried first, but this install lives under
 * "D:\Program Files (x86)\...", and a non-writable folder would silently give us
 * NO log at all — which reads exactly like "the proxy never loaded" and would
 * send the next session hunting the wrong failure. So fall back to LOCALAPPDATA
 * and let the caller report which one won. */
static void log_pick_path(void) {
    char dir[MAX_PATH];
    char *slash;

    if (GetModuleFileNameA(g_self, dir, MAX_PATH)) {
        slash = strrchr(dir, '\\');
        if (slash) {
            *slash = 0;
            _snprintf(g_logpath, MAX_PATH, "%s\\ds2_proxy.log", dir);
            g_logpath[MAX_PATH - 1] = 0;
            {
                HANDLE h = CreateFileA(g_logpath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                                       OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return; }
            }
        }
    }

    {
        const char *lad = getenv("LOCALAPPDATA");
        if (lad) _snprintf(g_logpath, MAX_PATH, "%s\\ds2_proxy.log", lad);
        else     _snprintf(g_logpath, MAX_PATH, "C:\\ds2_proxy.log");
        g_logpath[MAX_PATH - 1] = 0;
    }
}

static void log_msg(const char *fmt, ...) {
    FILE *f;
    if (!g_logpath[0]) return;
    if (g_loglock_ready) EnterCriticalSection(&g_loglock);
    f = fopen(g_logpath, "a");
    if (f) {
        SYSTEMTIME st;
        va_list ap;
        GetLocalTime(&st);
        fprintf(f, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fputc('\n', f);
        fclose(f);
    }
    if (g_loglock_ready) LeaveCriticalSection(&g_loglock);
}

/* ------------------------------------------------------- the real d3d9.dll */

/* Load the SYSTEM d3d9.dll by absolute path. Never LoadLibrary("d3d9.dll") from
 * here: the game folder is first on the search path, so that would load THIS
 * file again. */
static void load_real_dll(void) {
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    if (!n || n > MAX_PATH - 16) {
        log_msg("FATAL: GetSystemDirectoryA failed (%lu)", GetLastError());
        return;
    }
    strcat(path, "\\d3d9.dll");

    g_real = LoadLibraryA(path);
    if (!g_real) {
        log_msg("FATAL: could not load the real %s (error %lu)", path, GetLastError());
        return;
    }
    g_real_create = (PFN_Direct3DCreate9)(void *)GetProcAddress(g_real, "Direct3DCreate9");
    log_msg("real d3d9 loaded from %s (module %p), Direct3DCreate9 = %p",
            path, (void *)g_real, (void *)g_real_create);
}

/* ------------------------------------------------------------- the export */

void *WINAPI Proxy_Direct3DCreate9(UINT SDKVersion) {
    void *d3d;
    log_msg("Direct3DCreate9(SDKVersion=%u) called  <-- THE GAME REACHED D3D9 INIT", SDKVersion);
    if (!g_real_create) {
        log_msg("  ...but there is no real Direct3DCreate9 to forward to. Returning NULL.");
        return NULL;
    }
    d3d = g_real_create(SDKVersion);
    log_msg("  forwarded; the real Direct3DCreate9 returned %p", d3d);
    return d3d;
}

/* ---------------------------------------------------------------- attach */

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char exe[MAX_PATH];
        g_self = (HMODULE)inst;
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_loglock);
        g_loglock_ready = 1;
        log_pick_path();

        exe[0] = 0;
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        log_msg("=== stage-1 proxy attached ===");
        log_msg("host process: %s", exe);
        log_msg("log file: %s", g_logpath);
        log_msg("PROXY LOADED — a foreign DLL was allowed into the process.");

        load_real_dll();
    } else if (reason == DLL_PROCESS_DETACH) {
        log_msg("=== detached ===");
    }
    return TRUE;
}
