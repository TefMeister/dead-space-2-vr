/*
 * stereo.c — see stereo.h for the derivation and for the sign trap.
 *
 * Deliberately tiny and free of D3D types: it is pure arithmetic on 16 floats, so
 * the selftest can exercise it in a process of our own with no game, no device and
 * no d3d9.dll. Nothing in here has been run against Dead Space 2.
 */
#include "stereo.h"

void stereo_make_eye(const float *mono, float e, float zc, float *out) {
    for (int i = 0; i < 16; ++i)
        out[i] = mono[i];

    if (zc <= 0.0f)          /* a non-positive convergence is meaningless; leave */
        return;              /* the matrix mono rather than produce nonsense.    */

    /* Two elements, and it has to be two: m[12] moves the eye (parallax that falls
       off with depth) and m[8] sets convergence (a constant offset). m[8] on its own
       is a flat sideways shift of the whole image, which the selftest proved by
       failing on it. Everything else — and in particular the depth triplet
       m[10]/m[11]/m[14] — is carried over untouched, so this game's non-standard
       depth mapping survives exactly as measured. */
    out[M_TRX] = -mono[M_XS] * e;
    out[M_SHX] =  mono[M_XS] * e / zc;
}

int stereo_right_eye_sign(const float *mono) {
    /* sign(e_right - e_left) must equal sign(xs); with the left eye at -e and the
       right at +e*s, that makes s the sign of xs itself. */
    return (mono[M_XS] < 0.0f) ? -1 : +1;
}
