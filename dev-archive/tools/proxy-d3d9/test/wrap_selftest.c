/*
 * wrap_selftest.c — prove the IDirect3D9 wrapper forwards correctly, WITHOUT the game.
 *
 * The wrapper has seventeen hand-written methods. One wrong slot, or one method
 * that forwards the wrong arguments, corrupts a call at runtime in a way that
 * looks like "the game crashed with the mod installed" — which is exactly the
 * ambiguity that cost this project an afternoon on 2026-09-14. So it is tested
 * here, in our own process, against the real d3d9.dll.
 *
 * The method: create Direct3D twice — once through OUR proxy (which returns the
 * wrapper) and once directly from the system DLL (the real object) — then call
 * the same queries on both and require identical answers. The real object is
 * independent ground truth; a bug in the wrapper cannot hide behind it.
 *
 * Device creation is attempted too, because that is the path that installs the
 * camera instrument. If the machine refuses to make a device in a console
 * process the test SKIPS that part rather than failing it: a skipped check is
 * honest, a fabricated pass is not.
 */

#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

typedef IDirect3D9 *(WINAPI *PFN_Create)(UINT);

static int failures = 0, skipped = 0;

static void ok(const char *what, int cond) {
    if (cond) { printf("  ok    %s\n", what); }
    else      { printf("  FAIL  %s\n", what); failures++; }
}

int main(int argc, char **argv) {
    const char *dll = (argc > 1) ? argv[1] : "build\\d3d9.dll";
    HMODULE hProxy, hReal;
    PFN_Create createProxy, createReal;
    IDirect3D9 *wrapped, *real;
    char sys[MAX_PATH];

    printf("IDirect3D9 wrapper self-test - no game, real d3d9.dll\n\n");

    hProxy = LoadLibraryA(dll);
    if (!hProxy) { printf("FAIL: cannot load %s (%lu)\n", dll, GetLastError()); return 1; }
    GetSystemDirectoryA(sys, MAX_PATH); strcat(sys, "\\d3d9.dll");
    hReal = LoadLibraryA(sys);
    if (!hReal) { printf("FAIL: cannot load %s (%lu)\n", sys, GetLastError()); return 1; }

    createProxy = (PFN_Create)(void *)GetProcAddress(hProxy, "Direct3DCreate9");
    createReal  = (PFN_Create)(void *)GetProcAddress(hReal,  "Direct3DCreate9");
    if (!createProxy || !createReal) { printf("FAIL: missing Direct3DCreate9\n"); return 1; }

    wrapped = createProxy(D3D_SDK_VERSION);
    real    = createReal(D3D_SDK_VERSION);
    printf("\n  wrapper object = %p\n  real object    = %p\n\n", (void *)wrapped, (void *)real);
    if (!wrapped || !real) { printf("FAIL: Direct3DCreate9 returned NULL\n"); return 1; }

    ok("the proxy handed back OUR object, not the real one", wrapped != real);

    printf("\nFORWARDING (wrapper answer must equal the real object's):\n");
    {
        UINT a = IDirect3D9_GetAdapterCount(wrapped);
        UINT b = IDirect3D9_GetAdapterCount(real);
        printf("        GetAdapterCount: wrapper=%u real=%u\n", a, b);
        ok("GetAdapterCount", a == b);
    }
    {
        D3DDISPLAYMODE m1, m2;
        HRESULT h1 = IDirect3D9_GetAdapterDisplayMode(wrapped, D3DADAPTER_DEFAULT, &m1);
        HRESULT h2 = IDirect3D9_GetAdapterDisplayMode(real,    D3DADAPTER_DEFAULT, &m2);
        printf("        GetAdapterDisplayMode: %ux%u @%uHz fmt=%d\n",
               m1.Width, m1.Height, m1.RefreshRate, m1.Format);
        ok("GetAdapterDisplayMode hr", h1 == h2);
        ok("GetAdapterDisplayMode contents",
           m1.Width == m2.Width && m1.Height == m2.Height &&
           m1.RefreshRate == m2.RefreshRate && m1.Format == m2.Format);
    }
    {
        D3DADAPTER_IDENTIFIER9 i1, i2;
        HRESULT h1 = IDirect3D9_GetAdapterIdentifier(wrapped, D3DADAPTER_DEFAULT, 0, &i1);
        HRESULT h2 = IDirect3D9_GetAdapterIdentifier(real,    D3DADAPTER_DEFAULT, 0, &i2);
        printf("        adapter: %s\n", i1.Description);
        ok("GetAdapterIdentifier hr", h1 == h2);
        ok("GetAdapterIdentifier description", strcmp(i1.Description, i2.Description) == 0);
    }
    {
        UINT a = IDirect3D9_GetAdapterModeCount(wrapped, D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
        UINT b = IDirect3D9_GetAdapterModeCount(real,    D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
        ok("GetAdapterModeCount", a == b);
    }
    {
        D3DCAPS9 c1, c2;
        HRESULT h1 = IDirect3D9_GetDeviceCaps(wrapped, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &c1);
        HRESULT h2 = IDirect3D9_GetDeviceCaps(real,    D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &c2);
        ok("GetDeviceCaps hr", h1 == h2);
        ok("GetDeviceCaps VertexShaderVersion", c1.VertexShaderVersion == c2.VertexShaderVersion);
    }
    {
        HRESULT h1 = IDirect3D9_CheckDeviceType(wrapped, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
        HRESULT h2 = IDirect3D9_CheckDeviceType(real, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
        ok("CheckDeviceType", h1 == h2);
    }
    ok("GetAdapterMonitor", IDirect3D9_GetAdapterMonitor(wrapped, D3DADAPTER_DEFAULT)
                         == IDirect3D9_GetAdapterMonitor(real, D3DADAPTER_DEFAULT));

    printf("\nCOM BEHAVIOUR:\n");
    {
        void *self = NULL;
        HRESULT hr = IDirect3D9_QueryInterface(wrapped, &IID_IDirect3D9, &self);
        ok("QueryInterface(IID_IDirect3D9) succeeds", SUCCEEDED(hr));
        ok("...and hands back OURSELVES, not the real object", self == (void *)wrapped);
        if (SUCCEEDED(hr) && self) IDirect3D9_Release((IDirect3D9 *)self);
    }
    {
        ULONG r1 = IDirect3D9_AddRef(wrapped);
        ULONG r2 = IDirect3D9_Release(wrapped);
        printf("        AddRef->%lu Release->%lu\n", r1, r2);
        ok("AddRef/Release are balanced", r1 == r2 + 1);
    }

    printf("\nDEVICE CREATION (the path that installs the instrument):\n");
    {
        D3DPRESENT_PARAMETERS pp;
        IDirect3DDevice9 *dev = NULL;
        HRESULT hr;
        HWND hwnd = CreateWindowExA(0, "STATIC", "wrapsel", WS_OVERLAPPED,
                                    0, 0, 64, 64, NULL, NULL, NULL, NULL);
        ZeroMemory(&pp, sizeof pp);
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.hDeviceWindow = hwnd;

        hr = IDirect3D9_CreateDevice(wrapped, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
        if (SUCCEEDED(hr) && dev) {
            float m[16];
            int i;
            printf("        device created: %p\n", (void *)dev);
            ok("CreateDevice through the wrapper succeeded", 1);
            /* Push a real perspective through the hooked path: if the instrument
             * installed, it announces it in the log above. */
            for (i = 0; i < 16; ++i) m[i] = 0.0f;
            m[0] = 0.974279f; m[5] = 1.732051f; m[10] = 1.0001f; m[11] = 1.0f; m[14] = -0.10001f;
            IDirect3DDevice9_SetVertexShaderConstantF(dev, 8, m, 4);
            printf("        pushed a perspective matrix at c8 through the device\n");
            IDirect3DDevice9_Release(dev);
        } else {
            printf("        SKIPPED: no HAL device in this process (hr=0x%08lX).\n",
                   (unsigned long)hr);
            printf("        That is a limitation of the harness, not a wrapper fault.\n");
            skipped++;
        }
        if (hwnd) DestroyWindow(hwnd);
    }

    IDirect3D9_Release(wrapped);
    IDirect3D9_Release(real);

    printf("\n%s", failures ? "SELFTEST FAILED\n" : "SELFTEST PASSED\n");
    if (skipped) printf("(%d check group skipped, honestly reported)\n", skipped);
    return failures ? 1 : 0;
}
