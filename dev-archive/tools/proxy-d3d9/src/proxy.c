/*
 * proxy.c — Dead Space 2, stage-2 d3d9 proxy.
 *
 * WHAT STAGE 1 ESTABLISHED, AND WHY THIS FILE CHANGED
 *
 * Stage 1 exported only Direct3DCreate9 and STOPPED THE GAME LAUNCHING. The A/B
 * in dev-archive/recon/2026-09-14-proxy-crash-ab-test/ pinned it down:
 *
 *   - with our DLL:                 4 crashes out of 4
 *   - with no DLL:                  runs
 *   - with a GENUINE Microsoft      runs
 *     d3d9.dll in the same folder:
 *
 * So the game does not object to a foreign d3d9.dll in its directory — it objects
 * to OURS. The system DLL exports seventeen functions and we exported one; any
 * other export the game resolves against us comes back NULL, and calling NULL
 * produces exactly the crash observed (0xc0000005, fault offset 0x00000000, no
 * owning module, killed by DEP).
 *
 * Stage 2 therefore exports all seventeen: this one implemented, the other sixteen
 * forwarded by naked thunks in thunks.c. Each thunk logs its first call, so if the
 * game now runs we learn WHICH export was needed rather than merely watching the
 * symptom disappear.
 *
 * ⚠️ STILL NOT PROVEN. A signature or authenticity check on the DLL would also fit
 * every observation so far — the genuine DLL is signed and ours is not. If stage 2
 * still crashes with no THUNK line in the log, that is the remaining explanation,
 * and the next probe is a dinput8.dll proxy to see whether the objection is to
 * foreign DLLs generally or to a graphics one specifically.
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

/* defined in thunks.c */
extern void *g_thunk_target[16];
extern const char *const g_thunk_name[16];

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

void log_msg(const char *fmt, ...) {
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

    /* Stage 2: resolve the sixteen exports we forward rather than implement.
     * Stage 1 shipped only Direct3DCreate9, and the A/B showed that is what stops
     * the game launching -- an unresolved import called as NULL is exactly the
     * crash we saw. See thunks.c. */
    {
        int i, missing = 0;
        for (i = 0; i < 16; i++) {
            g_thunk_target[i] = (void *)GetProcAddress(g_real, g_thunk_name[i]);
            if (!g_thunk_target[i]) {
                missing++;
                log_msg("  WARNING: the real d3d9 does not export %s", g_thunk_name[i]);
            }
        }
        log_msg("forwarding table built: %d of 16 resolved", 16 - missing);
    }
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
        log_msg("=== stage-2 proxy attached (all 17 exports) ===");
        log_msg("host process: %s", exe);
        log_msg("log file: %s", g_logpath);
        log_msg("PROXY LOADED — a foreign DLL was allowed into the process.");

        load_real_dll();
    } else if (reason == DLL_PROCESS_DETACH) {
        log_msg("=== detached ===");
    }
    return TRUE;
}
