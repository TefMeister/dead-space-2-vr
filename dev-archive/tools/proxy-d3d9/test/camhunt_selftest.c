/*
 * camhunt_selftest.c — test the projection detector against CONSTRUCTED matrices.
 *
 * The detector is the whole instrument. If it is too loose it will report a view
 * matrix or a shadow frustum as the camera; if it is too tight it will report
 * nothing and we will wrongly conclude the projection does not travel through
 * SetVertexShaderConstantF. Either way we would be reading a live log through a
 * broken lens and would not know it.
 *
 * So the matrices here are built from the documented D3D formulae rather than
 * captured, which means the expected answer is known independently of the code
 * under test. This is the standing rule on this account: test against
 * independently constructed ground truth, not against a transcription of the
 * thing you are testing.
 *
 * Runs in a process of our own. No game, no device, no d3d9 at all.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include "../src/camhunt.h"

/* camhunt.c logs through proxy.c's log_msg; give it one that prints. */
void log_msg(const char *fmt, ...) {
    va_list ap;
    printf("      | ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

static int failures = 0;

static void check(const char *what, int got, int want) {
    const char *g = got == CAMHUNT_ROW ? "ROW" : got == CAMHUNT_COL ? "COL" : "NONE";
    const char *w = want == CAMHUNT_ROW ? "ROW" : want == CAMHUNT_COL ? "COL" : "NONE";
    if (got == want) {
        printf("  ok    %-46s -> %s\n", what, g);
    } else {
        printf("  FAIL  %-46s -> %s (expected %s)\n", what, g, w);
        failures++;
    }
}

static void transpose(const float *in, float *out) {
    int r, c;
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            out[c * 4 + r] = in[r * 4 + c];
}

/* D3DXMatrixPerspectiveFovLH, straight from the documented formula. */
static void persp_lh(float *m, float fovY, float aspect, float zn, float zf) {
    float ys = 1.0f / tanf(fovY * 0.5f);
    float xs = ys / aspect;
    memset(m, 0, 16 * sizeof(float));
    m[0]  = xs;
    m[5]  = ys;
    m[10] = zf / (zf - zn);
    m[11] = 1.0f;
    m[14] = -zn * zf / (zf - zn);
    m[15] = 0.0f;
}

/* The right-handed variant: w = -z, and the z terms change sign accordingly. */
static void persp_rh(float *m, float fovY, float aspect, float zn, float zf) {
    float ys = 1.0f / tanf(fovY * 0.5f);
    float xs = ys / aspect;
    memset(m, 0, 16 * sizeof(float));
    m[0]  = xs;
    m[5]  = ys;
    m[10] = zf / (zn - zf);
    m[11] = -1.0f;
    m[14] = zn * zf / (zn - zf);
    m[15] = 0.0f;
}

static void ortho_lh(float *m, float w, float h, float zn, float zf) {
    memset(m, 0, 16 * sizeof(float));
    m[0]  = 2.0f / w;
    m[5]  = 2.0f / h;
    m[10] = 1.0f / (zf - zn);
    m[14] = -zn / (zf - zn);
    m[15] = 1.0f;
}

static void identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* A plausible view matrix: a rotation about Y plus a translation, row-vector
 * convention. Affine, so the last column is (0,0,0,1). */
static void view_matrix(float *m, float yaw) {
    float c = cosf(yaw), s = sinf(yaw);
    memset(m, 0, 16 * sizeof(float));
    m[0] = c;    m[2]  = -s;
    m[5] = 1.0f;
    m[8] = s;    m[10] = c;
    m[12] = 13.5f; m[13] = 1.8f; m[14] = -42.0f; m[15] = 1.0f;
}

int main(void) {
    float m[16], t[16];

    printf("camhunt detector self-test - constructed matrices, no game\n\n");

    printf("POSITIVES (these must be found):\n");
    persp_lh(m, 1.0471976f, 16.0f / 9.0f, 0.1f, 1000.0f);      /* 60 deg, 16:9 */
    check("left-handed perspective, row layout", camhunt_classify(m), CAMHUNT_ROW);
    transpose(m, t);
    check("the same matrix transposed, column layout", camhunt_classify(t), CAMHUNT_COL);

    persp_rh(m, 1.0471976f, 16.0f / 9.0f, 0.1f, 1000.0f);
    check("right-handed perspective, row layout", camhunt_classify(m), CAMHUNT_ROW);
    transpose(m, t);
    check("right-handed transposed, column layout", camhunt_classify(t), CAMHUNT_COL);

    persp_lh(m, 0.5235988f, 1.0f, 0.5f, 200.0f);               /* square, 30 deg */
    check("a square frustum (shadow-pass shaped)", camhunt_classify(m), CAMHUNT_ROW);

    printf("\nNEGATIVES (these must NOT be mistaken for a projection):\n");
    identity(m);
    check("identity", camhunt_classify(m), CAMHUNT_NONE);
    ortho_lh(m, 1280.0f, 720.0f, 0.0f, 1.0f);
    check("orthographic (a HUD/2D projection)", camhunt_classify(m), CAMHUNT_NONE);
    transpose(m, t);
    check("orthographic transposed", camhunt_classify(t), CAMHUNT_NONE);
    view_matrix(m, 0.7f);
    check("a view matrix (rotation + translation)", camhunt_classify(m), CAMHUNT_NONE);
    transpose(m, t);
    check("a view matrix transposed", camhunt_classify(t), CAMHUNT_NONE);
    memset(m, 0, sizeof m);
    check("all zeros", camhunt_classify(m), CAMHUNT_NONE);
    {
        /* The exact block that fooled the detector in Dead Space 2 gameplay at
         * c18 on 2026-09-14. Kept verbatim: a regression test written from a
         * real false positive is worth more than an invented one. */
        static const float c18[16] = {
            0.005f, 0.0f,  -0.05f, 200.0f,
            0.0f,   0.0f, 128.0f,    0.0f,
            1.0f,   1.0f,   1.0f,    1.0f,
            0.0f,   0.0f,   1.0f,    0.0f
        };
        float tmp[16];
        memcpy(tmp, c18, sizeof tmp);
        check("the real c18 false positive from DS2 gameplay", camhunt_classify(tmp), CAMHUNT_NONE);
    }
    {
        /* A projection with an absurdly wide field must still be ACCEPTED - the
         * new gate must reject junk without narrowing what counts as a camera. */
        float wide[16];
        persp_lh(wide, 3.1241f, 16.0f/9.0f, 0.1f, 1000.0f);   /* 179 degrees */
        check("a 179-degree field is still a projection", camhunt_classify(wide), CAMHUNT_ROW);
        persp_lh(wide, 0.0174533f, 16.0f/9.0f, 0.1f, 1000.0f); /* 1 degree */
        check("a 1-degree field is still a projection", camhunt_classify(wide), CAMHUNT_ROW);
    }

    printf("\nWINDOW SCAN (the projection is found at an offset inside a block):\n");
    {
        /* A 128-register block flush with the projection sitting at offset 8
         * registers in. This is the case that matters: engines flush whole
         * blocks, so `start` is almost never the matrix's own register. */
        static float block[128 * 4];
        memset(block, 0, sizeof block);
        persp_lh(m, 1.0471976f, 16.0f / 9.0f, 0.1f, 1000.0f);
        memcpy(block + 8 * 4, m, sizeof m);
        printf("    feeding a c0+128 block with the projection at register c8:\n");
        camhunt_observe(0, block, 128);
        printf("    (an announcement above naming c8 means the offset scan works)\n");
    }

    printf("\nROBUSTNESS (must not crash):\n");
    camhunt_observe(0, NULL, 4);
    camhunt_observe(0, m, 0);
    camhunt_observe(0, m, 1);
    camhunt_observe(4000, m, 4);
    printf("  ok    null data, zero count, short count, high register\n");

    printf(failures ? "\nSELFTEST FAILED (%d)\n" : "\nSELFTEST PASSED\n", failures);
    return failures ? 1 : 0;
}
