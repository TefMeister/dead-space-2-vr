/*
 * thunks.c — forwarders for the sixteen d3d9 exports we do not implement.
 *
 * WHY THIS EXISTS
 *
 * Stage 1 exported only Direct3DCreate9 and stopped Dead Space 2 launching. The
 * A/B (dev-archive/recon/2026-09-14-proxy-crash-ab-test/) showed a genuine
 * Microsoft d3d9.dll in the same folder runs fine, so the game does not object to
 * a foreign d3d9 as such — it objects to OURS. The system DLL exports seventeen
 * functions; we exported one. Anything else the game resolves against us comes
 * back NULL, and calling NULL gives exactly the observed crash: access violation
 * at fault offset 0x00000000 with no owning module, killed by DEP.
 *
 * WHY NAKED ASSEMBLY RATHER THAN TYPED C WRAPPERS
 *
 * Seven of the sixteen are documented (the D3DPERF_* family); the rest —
 * PSGPError, PSGPSampleTexture, DebugSetLevel, DebugSetMute,
 * Direct3DShaderValidatorCreate9, Direct3D9EnableMaximizedWindowedModeShim and
 * the On12 pair — are undocumented or vary between Windows versions. Writing a
 * typed wrapper with a guessed signature is worse than useless on 32-bit stdcall:
 * the callee cleans the stack, so a wrong argument count corrupts it silently.
 *
 * A naked thunk sidesteps the question entirely. It never touches the arguments;
 * it logs, restores every register and flag exactly, and jumps to the real
 * function with the stack byte-identical to how the caller left it. Correct for
 * any calling convention and any signature, known or not.
 *
 * WHY THEY LOG
 *
 * If simply adding the exports makes the game run, that alone would not prove
 * WHY — it would only remove the symptom, which is the trap this project's own
 * notes warn about. Each thunk logs its first call, so a successful launch tells
 * us exactly which export the game actually needed. That converts the hypothesis
 * into a finding.
 */

#include <windows.h>

void *g_thunk_target[16];           /* filled by resolve_thunks() in proxy.c */
const char *const g_thunk_name[16] = {
    "D3DPERF_BeginEvent",
    "D3DPERF_EndEvent",
    "D3DPERF_GetStatus",
    "D3DPERF_QueryRepeatFrame",
    "D3DPERF_SetMarker",
    "D3DPERF_SetOptions",
    "D3DPERF_SetRegion",
    "DebugSetLevel",
    "DebugSetMute",
    "Direct3D9EnableMaximizedWindowedModeShim",
    "Direct3DCreate9Ex",
    "Direct3DCreate9On12",
    "Direct3DCreate9On12Ex",
    "Direct3DShaderValidatorCreate9",
    "PSGPError",
    "PSGPSampleTexture",
};

static unsigned char g_seen[16];

void log_msg(const char *fmt, ...);     /* proxy.c */

/* Called from inside the thunk, with every register already saved. */
void thunk_hit(int idx) {
    if (idx < 0 || idx >= 16) return;
    if (g_seen[idx]) return;            /* first call only — no log spam */
    g_seen[idx] = 1;
    log_msg("THUNK: the game called %s  <-- an export stage 1 did NOT provide", g_thunk_name[idx]);
    if (!g_thunk_target[idx])
        log_msg("  !! and we have no real target for it. This call will fail.");
}

/*
 * pushal/popal save and restore all eight general-purpose registers; pushfl/popfl
 * the flags. The argument to thunk_hit is pushed and then removed by us (cdecl),
 * so the caller's stack frame is untouched when we jump. This must stay
 * hand-written: any compiler-generated prologue would disturb the frame the real
 * function is about to read its arguments from.
 */
#define THUNK(idx)                          \
    __asm__(".globl _Thunk_" #idx "\n"      \
            "_Thunk_" #idx ":\n"            \
            "  pushal\n"                    \
            "  pushfl\n"                    \
            "  pushl $" #idx "\n"           \
            "  call _thunk_hit\n"           \
            "  addl $4, %esp\n"             \
            "  popfl\n"                     \
            "  popal\n"                     \
            "  jmpl *(_g_thunk_target + " #idx "*4)\n")

THUNK(0);  THUNK(1);  THUNK(2);  THUNK(3);
THUNK(4);  THUNK(5);  THUNK(6);  THUNK(7);
THUNK(8);  THUNK(9);  THUNK(10); THUNK(11);
THUNK(12); THUNK(13); THUNK(14); THUNK(15);
