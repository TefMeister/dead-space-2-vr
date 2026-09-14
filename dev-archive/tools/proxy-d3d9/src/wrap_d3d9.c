/*
 * wrap_d3d9.c — hand back OUR OWN IDirect3D9, instead of patching the real one.
 *
 * WHY THIS EXISTS — a launch said so, on 2026-09-14.
 *
 * Stage 3 patched slot 16 (CreateDevice) of the IDirect3D9 vtable, guarded by a
 * check that the slot still belonged to the real d3d9.dll. Tefa played to a save
 * point and the log said:
 *
 *     REFUSING to hook IDirect3D9 slot 16: it holds 73794E60, not owned by the
 *     real d3d9.dll. Standing down - the game still runs, only the instrument is lost.
 *
 * ⭐ The guard did exactly its job and the design was still wrong. Something —
 * almost certainly the Steam overlay, which hooks D3D9 in every Steam game — is
 * already in that slot before we ever see it. Standing down happens BEFORE a
 * device exists, so the camera instrument never got a chance to install, and no
 * amount of playing would have changed that.
 *
 * ⭐ The fix is not to chain into the other hook. Chaining is how you get mutual
 * recursion: if the overlay re-applies its hook later it captures OUR pointer as
 * "the original", and the two hooks call each other forever. The sibling project
 * recursed CreateDevice 1669 times and killed a launch that way.
 *
 * The fix is to stop competing for a slot we cannot win. We own the
 * Direct3DCreate9 export, so we hand back our own object. Nothing is written
 * into any shared vtable, so no other hook is ahead of us and none is disturbed
 * — the overlay's hook still runs, layered underneath every one of our
 * forwarders.
 *
 * This is a port of the same fix on staging/alan-wake-vr/proxy-d3d9, which met
 * the identical stand-down on the identical slot on 2026-09-08.
 *
 * LIFETIME — the one thing here that can still crash. Our vtable points into
 * THIS DLL, so the DLL must not unload while the game still holds a wrapper. A
 * reference is taken on our own module while any wrapper is alive and released
 * when the last one dies.
 *
 * SCOPE: the DEVICE is still reached by vtable patch, deliberately.
 * IDirect3DDevice9 has 119 methods and a hand-written wrapper for it is a large
 * amount of mechanical code in which one wrong slot silently corrupts a call.
 * There is no evidence the device slot is contested — the stand-down happened
 * before a device existed, so that slot has never been reached to find out.
 * camhunt_install() carries the same guard, so if it IS contested we get a clean
 * stand-down of the instrument alone, with the game still running and a log line
 * naming the owner. That is when the device earns the same treatment, and not
 * before, because doing it now would be building against a guess.
 */

#include <windows.h>
#include <d3d9.h>
#include <stddef.h>
#include "camhunt.h"

void log_msg(const char *fmt, ...);     /* proxy.c */

typedef struct {
    CONST_VTBL struct IDirect3D9Vtbl *lpVtbl;   /* MUST be first: the COM ABI */
    IDirect3D9 *real;
    LONG ref;
} WrapD3D9;

#define WRAP(p) ((WrapD3D9 *)(p))
#define REAL(p) (WRAP(p)->real)

static volatile LONG g_wrap_live = 0;
static HMODULE       g_self_ref = NULL;
static HMODULE       g_real_d3d9_mod = NULL;

/* Deliberately WITHOUT UNCHANGED_REFCOUNT: we want the loader to keep us mapped
 * while our vtable is reachable. */
static void wrap_module_ref(void) {
    if (InterlockedIncrement(&g_wrap_live) != 1) return;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            (LPCSTR)(void *)&wrap_module_ref, &g_self_ref)) {
        g_self_ref = NULL;
        log_msg("WARNING: could not take a reference on our own module (err=%lu). If the game "
                "unloads d3d9.dll while still holding our IDirect3D9, the next call through it "
                "lands in unmapped memory.", GetLastError());
    }
}

static void wrap_module_unref(void) {
    if (InterlockedDecrement(&g_wrap_live) != 0) return;
    if (g_self_ref) { HMODULE m = g_self_ref; g_self_ref = NULL; FreeLibrary(m); }
}

/* ---- the seventeen methods. Every one forwards; only CreateDevice does
 * anything else, and only AFTER the real call has succeeded. They take
 * `IDirect3D9 *` rather than `WrapD3D9 *` on purpose, so the vtable below can be
 * typed as d3d9.h's own struct and the compiler checks all seventeen signatures
 * AND their order against the header. A hand-rolled array of void* would let one
 * wrong slot compile cleanly and corrupt a call at runtime. */

static HRESULT STDMETHODCALLTYPE W_QueryInterface(IDirect3D9 *This, REFIID riid, void **ppvObj) {
    if (!ppvObj) return E_POINTER;
    /* Hand back OURSELVES for the interfaces we implement — returning the real
     * object would let the caller escape the wrapper, which is the whole point. */
    if (IsEqualGUID(riid, &IID_IDirect3D9) || IsEqualGUID(riid, &IID_IUnknown)) {
        IDirect3D9_AddRef(This);
        *ppvObj = This;
        return S_OK;
    }
    return IDirect3D9_QueryInterface(REAL(This), riid, ppvObj);
}
static ULONG STDMETHODCALLTYPE W_AddRef(IDirect3D9 *This) {
    return (ULONG)InterlockedIncrement(&WRAP(This)->ref);
}
static ULONG STDMETHODCALLTYPE W_Release(IDirect3D9 *This) {
    LONG n = InterlockedDecrement(&WRAP(This)->ref);
    if (n == 0) {
        IDirect3D9 *r = REAL(This);
        log_msg("wrapper IDirect3D9 %p released (real=%p)", (void *)This, (void *)r);
        if (r) IDirect3D9_Release(r);
        HeapFree(GetProcessHeap(), 0, This);
        wrap_module_unref();          /* last, and only after the object is gone */
    }
    return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE W_RegisterSoftwareDevice(IDirect3D9 *This, void *p) {
    return IDirect3D9_RegisterSoftwareDevice(REAL(This), p);
}
static UINT STDMETHODCALLTYPE W_GetAdapterCount(IDirect3D9 *This) {
    return IDirect3D9_GetAdapterCount(REAL(This));
}
static HRESULT STDMETHODCALLTYPE W_GetAdapterIdentifier(IDirect3D9 *This, UINT A, DWORD F,
                                                        D3DADAPTER_IDENTIFIER9 *p) {
    return IDirect3D9_GetAdapterIdentifier(REAL(This), A, F, p);
}
static UINT STDMETHODCALLTYPE W_GetAdapterModeCount(IDirect3D9 *This, UINT A, D3DFORMAT F) {
    return IDirect3D9_GetAdapterModeCount(REAL(This), A, F);
}
static HRESULT STDMETHODCALLTYPE W_EnumAdapterModes(IDirect3D9 *This, UINT A, D3DFORMAT F, UINT M,
                                                    D3DDISPLAYMODE *p) {
    return IDirect3D9_EnumAdapterModes(REAL(This), A, F, M, p);
}
static HRESULT STDMETHODCALLTYPE W_GetAdapterDisplayMode(IDirect3D9 *This, UINT A, D3DDISPLAYMODE *p) {
    return IDirect3D9_GetAdapterDisplayMode(REAL(This), A, p);
}
static HRESULT STDMETHODCALLTYPE W_CheckDeviceType(IDirect3D9 *This, UINT A, D3DDEVTYPE D,
                                                   D3DFORMAT AF, D3DFORMAT BF, BOOL W) {
    return IDirect3D9_CheckDeviceType(REAL(This), A, D, AF, BF, W);
}
static HRESULT STDMETHODCALLTYPE W_CheckDeviceFormat(IDirect3D9 *This, UINT A, D3DDEVTYPE D,
                                                     D3DFORMAT AF, DWORD U, D3DRESOURCETYPE R,
                                                     D3DFORMAT CF) {
    return IDirect3D9_CheckDeviceFormat(REAL(This), A, D, AF, U, R, CF);
}
static HRESULT STDMETHODCALLTYPE W_CheckDeviceMultiSampleType(IDirect3D9 *This, UINT A, D3DDEVTYPE D,
                                                              D3DFORMAT SF, BOOL W,
                                                              D3DMULTISAMPLE_TYPE MT, DWORD *pQ) {
    return IDirect3D9_CheckDeviceMultiSampleType(REAL(This), A, D, SF, W, MT, pQ);
}
static HRESULT STDMETHODCALLTYPE W_CheckDepthStencilMatch(IDirect3D9 *This, UINT A, D3DDEVTYPE D,
                                                          D3DFORMAT AF, D3DFORMAT RF, D3DFORMAT DF) {
    return IDirect3D9_CheckDepthStencilMatch(REAL(This), A, D, AF, RF, DF);
}
static HRESULT STDMETHODCALLTYPE W_CheckDeviceFormatConversion(IDirect3D9 *This, UINT A, D3DDEVTYPE D,
                                                               D3DFORMAT SF, D3DFORMAT TF) {
    return IDirect3D9_CheckDeviceFormatConversion(REAL(This), A, D, SF, TF);
}
static HRESULT STDMETHODCALLTYPE W_GetDeviceCaps(IDirect3D9 *This, UINT A, D3DDEVTYPE D, D3DCAPS9 *p) {
    return IDirect3D9_GetDeviceCaps(REAL(This), A, D, p);
}
static HMONITOR STDMETHODCALLTYPE W_GetAdapterMonitor(IDirect3D9 *This, UINT A) {
    return IDirect3D9_GetAdapterMonitor(REAL(This), A);
}

static HRESULT STDMETHODCALLTYPE W_CreateDevice(IDirect3D9 *This, UINT Adapter, D3DDEVTYPE DeviceType,
                                                HWND hFocusWindow, DWORD BehaviorFlags,
                                                D3DPRESENT_PARAMETERS *pp,
                                                IDirect3DDevice9 **ppDevice) {
    HRESULT hr;
    int pure = (BehaviorFlags & D3DCREATE_PUREDEVICE) != 0;

    log_msg("IDirect3D9::CreateDevice (through OUR wrapper): Adapter=%u DeviceType=%d "
            "hFocusWindow=%p BehaviorFlags=0x%lX%s",
            Adapter, DeviceType, (void *)hFocusWindow, (unsigned long)BehaviorFlags,
            pure ? "  <-- PUREDEVICE: D3D9 refuses Get* on shader constants on a pure device"
                 : "  (not a pure device)");
    if (pp)
        log_msg("  pp: %ux%u windowed=%d swapeffect=%d autoDS=%d refresh=%u presentInterval=0x%X",
                pp->BackBufferWidth, pp->BackBufferHeight, pp->Windowed, pp->SwapEffect,
                pp->EnableAutoDepthStencil, pp->FullScreen_RefreshRateInHz, pp->PresentationInterval);
    else
        log_msg("  pPresentationParameters is NULL");

    hr = IDirect3D9_CreateDevice(REAL(This), Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                 pp, ppDevice);
    log_msg("  -> hr=0x%08lX device=%p", (unsigned long)hr,
            ppDevice ? (void *)*ppDevice : NULL);

    if (SUCCEEDED(hr) && ppDevice && *ppDevice)
        camhunt_install(*ppDevice, g_real_d3d9_mod);
    else
        log_msg("  no device produced - the camera instrument cannot install");
    return hr;
}

/* Typed as d3d9.h's OWN vtable struct, so the compiler checks every signature
 * and the slot order against the header. */
static CONST_VTBL struct IDirect3D9Vtbl g_wrap_vtbl = {
    W_QueryInterface, W_AddRef, W_Release,
    W_RegisterSoftwareDevice, W_GetAdapterCount, W_GetAdapterIdentifier,
    W_GetAdapterModeCount, W_EnumAdapterModes, W_GetAdapterDisplayMode,
    W_CheckDeviceType, W_CheckDeviceFormat, W_CheckDeviceMultiSampleType,
    W_CheckDepthStencilMatch, W_CheckDeviceFormatConversion, W_GetDeviceCaps,
    W_GetAdapterMonitor, W_CreateDevice
};

/* Checked at compile time rather than trusted: the cost of being wrong is a call
 * landing on the wrong function with the wrong arguments. */
typedef char wrap_vtbl_size_check[
    (sizeof(struct IDirect3D9Vtbl) == 17 * sizeof(void *)) ? 1 : -1];
typedef char wrap_createdevice_slot_check[
    (offsetof(struct IDirect3D9Vtbl, CreateDevice) == 16 * sizeof(void *)) ? 1 : -1];

void *wrap_d3d9(void *real, HMODULE real_d3d9_mod) {
    WrapD3D9 *w;
    if (!real) return NULL;
    g_real_d3d9_mod = real_d3d9_mod;
    w = (WrapD3D9 *)HeapAlloc(GetProcessHeap(), 0, sizeof(WrapD3D9));
    if (!w) {
        log_msg("WARNING: out of memory allocating the IDirect3D9 wrapper - handing back the REAL "
                "object. The game will run; this mod will not see CreateDevice.");
        return real;
    }
    w->lpVtbl = &g_wrap_vtbl;
    w->real = (IDirect3D9 *)real;
    w->ref = 1;
    wrap_module_ref();
    log_msg("returning OUR IDirect3D9 %p wrapping the real %p. Nothing was written into a shared "
            "vtable, so no other hook is ahead of us and none is disturbed - the Steam overlay's "
            "hook still runs, layered below every one of our forwarders.",
            (void *)w, real);
    return w;
}
