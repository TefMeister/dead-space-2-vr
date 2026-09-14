/*
 * proxy.c — Dead Space 2 d3d9 proxy.
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
 * So all seventeen are exported: this one implemented, the other sixteen forwarded
 * by naked thunks in thunks.c. Each thunk logs its first call — and that is what
 * turned the fix into a finding: the 2026-09-14 log named **D3DPERF_GetStatus**,
 * called 6.1 s in, immediately before Direct3DCreate9 was reached for the first
 * time. Stage 1 had died at ~8 s having never got there. `[verified-live 2026-09-14]`
 *
 * The rival explanation — a signature check on the DLL, since the genuine one is
 * signed and ours is not — is `[disproved 2026-09-14]`: our unsigned DLL runs the
 * game fine once the exports are complete.
 *
 * The IDirect3D9 side is a WRAPPER, not a vtable patch — see wrap_d3d9.c for the
 * launch that forced that change. The camera instrument is camhunt.c.
 *
 * REVERSIBILITY: delete the d3d9.dll next to deadspace2.exe. Nothing else is
 * touched — no game file is modified, no registry key is written.
 *
 * Built 32-bit: deadspace2.exe is PE32/i386.
 */

#include <windows.h>
#include <d3d9.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "camhunt.h"

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

/* ------------------------------------------- reaching the device

 * The camera instrument hooks a method on IDirect3DDevice9, and the only way to
 * get that device is to see it created.
 *
 * ⚠️ We used to patch slot 16 of the real IDirect3D9's vtable. A launch on
 * 2026-09-14 proved that cannot work here: something (almost certainly the Steam
 * overlay) is already in that slot, the ownership guard correctly stood down,
 * and standing down happens BEFORE a device exists - so the instrument never
 * installed no matter how long the game was played.
 *
 * We own the Direct3DCreate9 export, so we now hand back our own IDirect3D9
 * object instead and stop racing for a slot we cannot win. See wrap_d3d9.c.
 */
void *wrap_d3d9(void *real, HMODULE real_d3d9_mod);   /* wrap_d3d9.c */

void *WINAPI Proxy_Direct3DCreate9(UINT SDKVersion) {
    void *d3d;
    log_msg("Direct3DCreate9(SDKVersion=%u) called  <-- THE GAME REACHED D3D9 INIT", SDKVersion);
    if (!g_real_create) {
        log_msg("  ...but there is no real Direct3DCreate9 to forward to. Returning NULL.");
        return NULL;
    }
    d3d = g_real_create(SDKVersion);
    log_msg("  forwarded; the real Direct3DCreate9 returned %p", d3d);
    return wrap_d3d9(d3d, g_real);
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
        log_msg("=== stage-4 proxy attached: 17 exports, IDirect3D9 wrapper, camera instrument ===");
        log_msg("host process: %s", exe);
        log_msg("log file: %s", g_logpath);
        log_msg("PROXY LOADED — a foreign DLL was allowed into the process.");

        load_real_dll();
    } else if (reason == DLL_PROCESS_DETACH) {
        /* Both vtables are shared per interface class and the pointers we wrote
         * live inside this DLL, so they MUST come back out before we can be
         * unloaded. Getting this wrong is a crash on exit. */
        /* The DEVICE vtable is shared per interface class and the pointer we
         * wrote lives inside this DLL, so it must come back out before we can be
         * unloaded. The IDirect3D9 side needs no such undo any more: we never
         * wrote into its vtable, and our wrapper holds a module reference for as
         * long as the game holds the wrapper. */
        camhunt_remove();
        log_msg("=== detached ===");
    }
    return TRUE;
}
