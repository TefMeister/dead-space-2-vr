/*
 * camhunt.c — find Dead Space 2's projection matrix. READ-ONLY.
 *
 * Ported 2026-09-14 from staging/alan-wake-vr/proxy-d3d9/src/proxy.c, which
 * reached this shape the expensive way over four sessions. The three lessons
 * that cost the most there are carried over deliberately, and each is marked
 * ⭐ below so they are not "simplified away" by a later reader who has not paid
 * for them:
 *
 *   ⭐ 1. REGISTER-AGNOSTIC. The first Alan Wake instrument watched four
 *         candidate registers taken from a shader census, and the projection
 *         turned out not to be at any of them. Every 4-register window of every
 *         upload is tested here instead. We do not know Dead Space 2's register
 *         and must not pretend to.
 *
 *   ⭐ 2. KEYED BY THE PROJECTION, NOT BY THE REGISTER. Keying the
 *         "already logged this" table by register meant a shadow pass uploading
 *         a projection at c0 permanently MASKED the camera projection at c0 —
 *         hiding the one matrix the session most needed. The key here is
 *         (xs, ys), the two diagonal scale terms: they differ between a shadow
 *         frustum and the camera, and they are unchanged by transpose, so the
 *         key does not depend on settling the storage layout first.
 *
 *   ⭐ 3. A SATURATED TABLE MUST SAY SO. The distinct-signature table filled
 *         during a load-time FOV settle, and every later signature — including
 *         the settled gameplay FOV, the one value actually wanted — was dropped
 *         silently. So the drop count is printed, and a separate LIVE table
 *         reports what is happening *now* rather than what was seen first.
 *
 * BOTH PACKINGS ARE TESTED. A 4x4 can reach the GPU as four rows or four
 * columns. Testing only one packing is how the Alan Wake scan found nothing in
 * its first run. `layout R` puts the w-from-z term at index 11; `layout C` puts
 * it at index 14.
 *
 * COST. This runs on a function called thousands of times per frame, so the hot
 * path does no I/O and no formatting — only comparisons, and logging behind a
 * time gate.
 */

#include "camhunt.h"
#include <d3d9.h>
#include <stdio.h>
#include <stddef.h>

void log_msg(const char *fmt, ...);     /* proxy.c */

/* ------------------------------------------------------------------ detector */

/*
 * A D3D perspective projection has a recognisable shape whichever way it is
 * packed: the w output is +-1 times view z, the w row/column is otherwise zero,
 * and the homogeneous corner is zero. We also require the z term to be non-zero,
 * which rejects an identity-like block that would otherwise pass.
 *
 * Tolerances are deliberately loose on the +-1 (0.9..1.1) and tight on the zeros:
 * a real projection's zeros are exact, while the +-1 can carry a reversed-Z or
 * infinite-far tweak.
 */
int camhunt_classify(const float *p) {
    float a11, a14;
    if (!p) return CAMHUNT_NONE;

    a11 = p[11] < 0 ? -p[11] : p[11];
    a14 = p[14] < 0 ? -p[14] : p[14];

    /* layout R: register i is row i. w' = z * p[11]; p[15] is 0; the x and y
     * rows carry no w term, so p[3] and p[7] are 0; p[14] is the z offset. */
    if (a11 > 0.9f && a11 < 1.1f &&
        p[15] > -0.05f && p[15] < 0.05f &&
        p[3]  > -0.001f && p[3]  < 0.001f &&
        p[7]  > -0.001f && p[7]  < 0.001f &&
        p[14] != 0.0f)
        return CAMHUNT_ROW;

    /* layout C: the same matrix transposed. The +-1 moves to index 14, the
     * zeros to p[12] and p[13], and the z offset to p[11]. */
    if (a14 > 0.9f && a14 < 1.1f &&
        p[15] > -0.05f && p[15] < 0.05f &&
        p[12] > -0.001f && p[12] < 0.001f &&
        p[13] > -0.001f && p[13] < 0.001f &&
        p[11] != 0.0f)
        return CAMHUNT_COL;

    return CAMHUNT_NONE;
}

/* ------------------------------------------------------------------- tables */

#define HIST_MAX 48
struct Range { unsigned int start, count; unsigned long n; };
static struct Range g_hist[HIST_MAX];
static int           g_hist_n;
static unsigned long g_hist_other;
static ULONGLONG     g_hist_last_ms;

#define SIG_MAX  24
#define SIG_REGS 8
struct Sig {
    float xs, ys;
    unsigned long n;
    unsigned int regs[SIG_REGS];
    int nregs;
    char layout;
};
static struct Sig    g_sig[SIG_MAX];
static int           g_sig_n;
static unsigned long g_sig_dropped;
static unsigned long g_uploads_total;

#define LIVE_MAX 8
struct Live { unsigned int reg; float xs, ys; char layout; unsigned long n; };
static struct Live g_live[LIVE_MAX];
static int         g_live_n;

/* Relative tolerance, so an animating FOV (a cutscene, aiming down a weapon)
 * does not spawn a fresh entry every frame and blow the table. */
static int sig_same(float a, float b) {
    float d = a - b;
    float m = (a < 0 ? -a : a) + 1.0f;
    if (d < 0) d = -d;
    return d <= 1e-4f * m;
}

static void log_matrix(unsigned int reg, const float *m, char layout, const struct Sig *sg) {
    float wz = (layout == CAMHUNT_COL) ? m[14] : m[11];
    log_msg("PERSPECTIVE-SHAPED 4x4 at c%u, layout %c (%lu uploads seen so far):",
            reg, layout, g_uploads_total);
    if (sg) {
        char rl[128]; int o = 0, i;
        for (i = 0; i < sg->nregs && o < (int)sizeof rl - 12; ++i)
            o += snprintf(rl + o, sizeof rl - (size_t)o, "%sc%u", i ? " " : "", sg->regs[i]);
        log_msg("    signature xs=%.6f ys=%.6f   seen at: %s%s",
                sg->xs, sg->ys, rl, sg->nregs >= SIG_REGS ? " (+more)" : "");
    }
    log_msg("    [%12.6f %12.6f %12.6f %12.6f]", m[0],  m[1],  m[2],  m[3]);
    log_msg("    [%12.6f %12.6f %12.6f %12.6f]", m[4],  m[5],  m[6],  m[7]);
    log_msg("    [%12.6f %12.6f %12.6f %12.6f]", m[8],  m[9],  m[10], m[11]);
    log_msg("    [%12.6f %12.6f %12.6f %12.6f]", m[12], m[13], m[14], m[15]);
    if (wz > 0.9f && wz < 1.1f)
        log_msg("    reading: w-from-z = +1 -> clip.w = +view.z, LEFT-handed.");
    else if (wz < -0.9f && wz > -1.1f)
        log_msg("    reading: w-from-z = -1 -> clip.w = -view.z, RIGHT-handed. Any stereo "
                "derivation assuming left-handed needs every sign re-checked.");
}

void camhunt_observe(unsigned int start, const float *data, unsigned int count) {
    ULONGLONG now;
    int i;

    if (!data || count < 1) return;
    ++g_uploads_total;
    now = GetTickCount64();

    /* --- range histogram: how does THIS engine upload constants? Whole-block
     * flushes or small writes? We do not know, and it decides what `start`
     * means when we come to match a register later. */
    for (i = 0; i < g_hist_n; ++i)
        if (g_hist[i].start == start && g_hist[i].count == count) { ++g_hist[i].n; break; }
    if (i == g_hist_n) {
        if (g_hist_n < HIST_MAX) {
            g_hist[g_hist_n].start = start;
            g_hist[g_hist_n].count = count;
            g_hist[g_hist_n].n = 1;
            ++g_hist_n;
        } else ++g_hist_other;
    }

    /* --- ⭐ 1: scan every window, at every offset, in both packings. */
    if (count >= 4) {
        unsigned int k;
        for (k = 0; k + 4 <= count; ++k) {
            const float *p = data + (size_t)k * 4;
            int layout = camhunt_classify(p);
            unsigned int reg;
            float xs, ys;
            int si, lj;

            if (layout == CAMHUNT_NONE) continue;

            reg = start + k;
            xs = p[0];              /* diagonal terms: transpose-invariant, so */
            ys = p[5];              /* usable as a key before layout is settled */

            /* ⭐ 3: what is happening NOW, per register, this period. */
            for (lj = 0; lj < g_live_n; ++lj) if (g_live[lj].reg == reg) break;
            if (lj == g_live_n && g_live_n < LIVE_MAX) { g_live[g_live_n].n = 0; ++g_live_n; }
            if (lj < LIVE_MAX) {
                g_live[lj].reg = reg; g_live[lj].xs = xs;
                g_live[lj].ys = ys;   g_live[lj].layout = (char)layout;
                ++g_live[lj].n;
            }

            /* ⭐ 2: distinct projections, keyed by (xs, ys) and NOT by register. */
            for (si = 0; si < g_sig_n; ++si)
                if (sig_same(g_sig[si].xs, xs) && sig_same(g_sig[si].ys, ys)) break;
            if (si == g_sig_n) {
                if (g_sig_n < SIG_MAX) {
                    struct Sig *s = &g_sig[g_sig_n++];
                    s->xs = xs; s->ys = ys; s->n = 1;
                    s->regs[0] = reg; s->nregs = 1; s->layout = (char)layout;
                    log_matrix(reg, p, (char)layout, s);     /* announce once, in full */
                } else ++g_sig_dropped;
            } else {
                struct Sig *s = &g_sig[si];
                int r;
                ++s->n;
                for (r = 0; r < s->nregs; ++r) if (s->regs[r] == reg) break;
                if (r == s->nregs && s->nregs < SIG_REGS) s->regs[s->nregs++] = reg;
            }
        }
    }

    /* --- periodic report, off the hot path by a time gate. */
    if (g_hist_last_ms == 0) g_hist_last_ms = now;
    if (now - g_hist_last_ms >= 5000) {
        char line[1024]; int o = 0;
        o += snprintf(line + o, sizeof line - (size_t)o,
                      "VS upload ranges in the last %llu ms (start+count:n):",
                      (unsigned long long)(now - g_hist_last_ms));
        for (i = 0; i < g_hist_n && o < (int)sizeof line - 40; ++i) {
            if (g_hist[i].n == 0) continue;
            o += snprintf(line + o, sizeof line - (size_t)o, " c%u+%u:%lu",
                          g_hist[i].start, g_hist[i].count, g_hist[i].n);
            g_hist[i].n = 0;
        }
        if (g_hist_other) {
            o += snprintf(line + o, sizeof line - (size_t)o, " other:%lu", g_hist_other);
            g_hist_other = 0;
        }
        log_msg("%s", line);

        if (g_live_n) {
            char l2[1024]; int o2 = 0, j;
            o2 += snprintf(l2 + o2, sizeof l2 - (size_t)o2,
                           "LIVE perspective signatures this period (reg layout xs ys ys/xs n):");
            for (j = 0; j < g_live_n && o2 < (int)sizeof l2 - 70; ++j) {
                float ratio = g_live[j].xs != 0.0f ? g_live[j].ys / g_live[j].xs : 0.0f;
                o2 += snprintf(l2 + o2, sizeof l2 - (size_t)o2, " [c%u %c %.6f %.6f %.4f n=%lu]",
                               g_live[j].reg, g_live[j].layout, g_live[j].xs,
                               g_live[j].ys, ratio, g_live[j].n);
            }
            log_msg("%s", l2);
            log_msg("    THE CAMERA is the signature whose ys/xs equals the display aspect "
                    "ratio (1.7778 at 16:9). A shadow pass is usually square (ratio 1.0).");
            g_live_n = 0;
        }
        log_msg("    distinct-signature table: %d/%d slots used, %lu dropped%s",
                g_sig_n, SIG_MAX, g_sig_dropped,
                g_sig_dropped ? "  <-- SATURATED: new projections are being LOST. "
                                "Read the LIVE line above, not the one-shot announcements."
                              : "");
        if (g_sig_n == 0)
            log_msg("    NOTHING perspective-shaped seen yet in %lu uploads. If this persists "
                    "into gameplay, the projection is not travelling through "
                    "SetVertexShaderConstantF at all - which is itself the finding.",
                    g_uploads_total);
        g_hist_last_ms = now;
    }
}

/* --------------------------------------------------------------- the hook */

typedef HRESULT (WINAPI *PFN_SetVSConstF)(IDirect3DDevice9 *, UINT, const float *, UINT);

/* Device vtable slot for SetVertexShaderConstantF. NOT assumed — the negative
 * array below fails the build if the SDK header ever disagrees. */
#define IDX_DEV_SETVSCONSTF 94
typedef char setvsconstf_slot_check[
    (offsetof(struct IDirect3DDevice9Vtbl, SetVertexShaderConstantF)
     == IDX_DEV_SETVSCONSTF * sizeof(void *)) ? 1 : -1];

static PFN_SetVSConstF g_real_setvs;
static void          **g_hooked_vtable;
static HMODULE         g_real_d3d9;

/* Refuse to hook a slot that is not owned by the real d3d9.dll — and refuse
 * when the owner cannot be determined at all, because an unbacked pointer is a
 * trampoline, which is exactly the case to avoid. Chaining into somebody else's
 * hook is what recursed CreateDevice 1669 times on the Alan Wake project. */
static int slot_is_foreign(void *fn) {
    HMODULE owner = NULL;
    char name[MAX_PATH];
    if (fn && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 (LPCSTR)fn, &owner) && owner == g_real_d3d9)
        return 0;
    name[0] = 0;
    if (owner) GetModuleFileNameA(owner, name, MAX_PATH);
    log_msg("REFUSING to hook device slot %d: it holds %p, owned by %s, not the real d3d9.dll "
            "(%p). Something else is already hooking this. Standing down - the game still runs, "
            "only the instrument is lost.",
            IDX_DEV_SETVSCONSTF, fn, owner ? name : "NO MODULE (a trampoline or freed page)",
            (void *)g_real_d3d9);
    return 1;
}

static HRESULT WINAPI Hooked_SetVSConstF(IDirect3DDevice9 *This, UINT start,
                                         const float *data, UINT count) {
    camhunt_observe(start, data, count);          /* read-only, never edits */
    return g_real_setvs(This, start, data, count);
}

void camhunt_install(void *device, HMODULE real_d3d9) {
    DWORD oldProtect;
    void **vtable;

    if (!device || g_hooked_vtable) return;
    g_real_d3d9 = real_d3d9;
    vtable = *(void ***)device;

    if (slot_is_foreign(vtable[IDX_DEV_SETVSCONSTF])) { g_real_setvs = NULL; return; }
    g_real_setvs = (PFN_SetVSConstF)vtable[IDX_DEV_SETVSCONSTF];

    if (VirtualProtect(&vtable[IDX_DEV_SETVSCONSTF], sizeof(void *),
                       PAGE_EXECUTE_READWRITE, &oldProtect)) {
        vtable[IDX_DEV_SETVSCONSTF] = (void *)Hooked_SetVSConstF;
        VirtualProtect(&vtable[IDX_DEV_SETVSCONSTF], sizeof(void *), oldProtect, &oldProtect);
        g_hooked_vtable = vtable;                 /* published last */
        log_msg("CAMHUNT: SetVertexShaderConstantF hooked at device vtable slot %d (real=%p). "
                "Scanning EVERY register window for a projection - no register is assumed.",
                IDX_DEV_SETVSCONSTF, (void *)g_real_setvs);
    } else {
        log_msg("CAMHUNT: FATAL - VirtualProtect failed installing the hook (err=%lu)",
                GetLastError());
    }
}

/* The vtable is shared per interface class and the pointer we wrote lives
 * inside this DLL, so it MUST come back out before we can be unloaded. */
void camhunt_remove(void) {
    DWORD oldProtect;
    void **vtable = g_hooked_vtable;
    if (!vtable || !g_real_setvs) return;
    g_hooked_vtable = NULL;
    if (VirtualProtect(&vtable[IDX_DEV_SETVSCONSTF], sizeof(void *),
                       PAGE_EXECUTE_READWRITE, &oldProtect)) {
        vtable[IDX_DEV_SETVSCONSTF] = (void *)g_real_setvs;
        VirtualProtect(&vtable[IDX_DEV_SETVSCONSTF], sizeof(void *), oldProtect, &oldProtect);
    }
    log_msg("CAMHUNT: hook removed. %lu uploads observed, %d distinct projection signature(s), "
            "%lu dropped.", g_uploads_total, g_sig_n, g_sig_dropped);
}
