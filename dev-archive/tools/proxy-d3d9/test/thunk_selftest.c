/*
 * thunk_selftest.c — exercise the naked forwarding thunks WITHOUT the game.
 *
 * The sixteen forwarders in thunks.c are hand-written assembly. If the register
 * or stack handling is wrong, the failure mode is a crash — and if the only place
 * we ever run them is Dead Space 2, we would be unable to tell our own assembly
 * bug apart from the game's protection killing us. That is precisely the
 * confusion that cost us the first round.
 *
 * So this harness calls the thunks directly, in a 32-bit process of our own, with
 * no game and no protection anywhere near them. A crash here is our bug. A clean
 * run here means the thunks are sound and any later crash is about the game.
 *
 * It calls the two D3DPERF functions whose signatures ARE documented, so their
 * return values can be checked rather than merely observed not to crash:
 *   int WINAPI D3DPERF_BeginEvent(D3DCOLOR, LPCWSTR)   - returns nesting level
 *   int WINAPI D3DPERF_EndEvent(void)                  - returns nesting level
 * and D3DPERF_GetStatus(void), which returns a DWORD.
 */

#include <windows.h>
#include <stdio.h>

typedef int   (WINAPI *PFN_Begin)(DWORD, const wchar_t *);
typedef int   (WINAPI *PFN_End)(void);
typedef DWORD (WINAPI *PFN_GetStatus)(void);

int main(int argc, char **argv) {
    const char *dll = (argc > 1) ? argv[1] : "build\\d3d9.dll";
    HMODULE h;
    PFN_Begin  beginEvent;
    PFN_End    endEvent;
    PFN_GetStatus getStatus;
    int a, b;
    DWORD st;
    int failures = 0;

    printf("loading %s\n", dll);
    h = LoadLibraryA(dll);
    if (!h) { printf("FAIL: LoadLibrary error %lu\n", GetLastError()); return 1; }

    beginEvent = (PFN_Begin)(void *)GetProcAddress(h, "D3DPERF_BeginEvent");
    endEvent   = (PFN_End)(void *)GetProcAddress(h, "D3DPERF_EndEvent");
    getStatus  = (PFN_GetStatus)(void *)GetProcAddress(h, "D3DPERF_GetStatus");

    printf("D3DPERF_BeginEvent = %p\nD3DPERF_EndEvent   = %p\nD3DPERF_GetStatus  = %p\n",
           (void *)beginEvent, (void *)endEvent, (void *)getStatus);
    if (!beginEvent || !endEvent || !getStatus) { printf("FAIL: an export is missing\n"); return 1; }

    /* The real test. If the thunk mangles the stack, we do not come back from these. */
    printf("calling D3DPERF_BeginEvent...\n");
    a = beginEvent(0xFF00FF00, L"selftest");
    printf("  returned %d\n", a);

    printf("calling D3DPERF_EndEvent...\n");
    b = endEvent();
    printf("  returned %d\n", b);

    printf("calling D3DPERF_GetStatus...\n");
    st = getStatus();
    printf("  returned %lu\n", st);

    /* Called twice, to prove the log-once guard does not break the second call. */
    printf("calling D3DPERF_BeginEvent again...\n");
    a = beginEvent(0xFF0000FF, L"selftest2");
    printf("  returned %d\n", a);
    endEvent();

    /* Stack sanity: a local written before the calls must be intact afterwards.
     * A thunk that removed the wrong number of bytes would corrupt this frame. */
    {
        volatile int canary = 0x5A5A5A5A;
        beginEvent(0xFFFFFFFF, L"canary");
        endEvent();
        if (canary != 0x5A5A5A5A) {
            printf("FAIL: stack canary clobbered (0x%08X) - a thunk is unbalanced\n", canary);
            failures++;
        } else {
            printf("stack canary intact after a thunked call\n");
        }
    }

    printf(failures ? "\nSELFTEST FAILED\n" : "\nSELFTEST PASSED - the thunks forward and the stack is balanced\n");
    return failures ? 1 : 0;
}
