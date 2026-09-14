/*
 * stereo.h — per-eye projection derived from THIS GAME'S OBSERVED c4 matrix.
 *
 * Nothing here assumes the textbook D3D perspective form, because Dead Space 2's
 * projection is not in it (m[10] = 0.990097 with m[11] = +1; see
 * modding-notes/2026-09-14c). Every formula below is derived from the matrix we
 * actually measured, and the selftest checks the derivation against the measured
 * matrix rather than against a re-typed ideal one.
 *
 * CONVENTIONS, stated because getting one of them wrong is invisible until it is
 * in a headset and sickening:
 *
 *   - Row-vector D3D convention: clip = view * M, view is a row vector.
 *   - Layout R, as camhunt reports it: register i is row i, so the array index
 *     of (row r, column c) is r*4 + c. m[11] is therefore (row 2, col 3), the
 *     w-from-z term, and it is +1 here => LEFT-handed, clip.w = +view.z.
 *   - e is an eye's displacement along the +view.x axis, in view-space units.
 *     It is NOT "half the IPD" until you know which way +view.x points; see
 *     stereo_right_eye_sign().
 *   - zc is the convergence distance: the depth at which the two eyes' images
 *     coincide exactly. Objects nearer than zc get negative parallax.
 */
#ifndef STEREO_H
#define STEREO_H

/* Array indices, spelled out so a reader never has to count.
   Row-major, layout R: index = row*4 + col. */
#define M_XS   0   /* (0,0) horizontal scale  -- observed -1.944444 (X IS MIRRORED) */
#define M_YS   5   /* (1,1) vertical scale    -- observed  3.456790 */
#define M_SHX  8   /* (2,0) z-proportional term in clip.x == THE CONVERGENCE SLOT */
#define M_ZS  10   /* (2,2) depth scale       -- observed  0.990097, NOT textbook */
#define M_ZW  11   /* (2,3) w-from-z          -- observed  1.000000 => left-handed */
#define M_TRX 12   /* (3,0) w-proportional term in clip.x == THE EYE-OFFSET SLOT */
#define M_ZB  14   /* (3,2) depth bias        -- observed -0.099010 */

/*
 * ⭐ THERE ARE TWO SLOTS, NOT ONE, AND THAT IS THE WHOLE FINDING.
 *
 * The first draft of this file used m[8] alone and called it "the shear". The
 * selftest killed it in one run: disparity came out CONSTANT at every depth, which
 * is a flat image shifted sideways, not stereo. Writing the two terms out shows why.
 *
 *   clip.x = view.x*m[0] + view.y*m[4] + view.z*m[8] + view.w*m[12]
 *   ndc.x  = clip.x / view.z                      (because clip.w = view.z here)
 *          = xs*view.x/view.z  +  m[8]  +  m[12]/view.z
 *                                 ^^^^     ^^^^^^^^^^^^
 *                              constant    falls off with depth
 *
 *   m[12] IS THE EYE OFFSET. A camera displaced by e along view.x sees
 *   xs*(view.x - e)/z, i.e. an extra -xs*e/z — exactly the shape m[12] provides.
 *   This is real parallax: near things move a lot, far things barely.
 *
 *   m[8] IS CONVERGENCE. A constant NDC offset, the same at every depth, which
 *   slides the whole eye image until the two eyes agree at one chosen distance.
 *
 * ⭐ The consequence is better news than the board assumed: BOTH terms live in the
 * projection, so **per-eye stereo needs c4 and nothing else.** It does not wait on
 * c0 being confirmed. c0 is still wanted — head ROTATION has to go there — but the
 * eye separation does not depend on it.
 */

/*
 * Fill `out` (16 floats) with the mono matrix `mono` sheared for one eye.
 *
 * Derivation, from the two lines above:
 *
 *   the eye sits at view.x = e, so a point's x in that eye's frame is (view.x - e)
 *   ndc.x = xs*(view.x - e)/z + m[8]
 *         = xs*view.x/z  -  xs*e/z  +  m[8]
 *
 *   the -xs*e/z term is supplied by m[12]/z, so     m[12] = -xs*e
 *   require the mono answer at z == zc:   -xs*e/zc + m[8] = 0
 *   =>                                       m[8] = xs*e/zc
 *
 * Note what this does NOT touch: m[10], m[11] and m[14] are copied unchanged, so
 * the game's non-textbook depth mapping is preserved exactly and identically in
 * both eyes. That was flagged as an open worry on 2026-09-14; it is now closed by
 * construction, and the selftest asserts it.
 *
 * zc must be > 0. e may be any sign; see stereo_right_eye_sign().
 */
void stereo_make_eye(const float *mono, float e, float zc, float *out);

/*
 * Which sign of e belongs to the RIGHT eye, given the observed horizontal scale.
 *
 * Returns +1 if the right eye takes a positive e, -1 if it takes a negative one.
 *
 * ⚠️ THIS IS THE TRAP IN THIS GAME AND IT IS WHY THIS FUNCTION EXISTS.
 *
 * Correct stereo requires that an object NEARER than convergence appears further
 * LEFT in the right eye than in the left eye (negative parallax). Working that
 * through with the shear above:
 *
 *   ndc_r - ndc_l = -xs*(e_r - e_l)*(1/z - 1/zc)
 *
 * For z < zc the bracket is positive, so we need -xs*(e_r - e_l) < 0, i.e.
 *
 *   sign(e_r - e_l) == sign(xs)
 *
 * Dead Space 2's xs is NEGATIVE (-1.944444, the X axis is mirrored), so the right
 * eye takes a NEGATIVE e here — the opposite of every textbook. Assuming the
 * textbook sign swaps the eyes, which does not look broken on a flat screen and is
 * deeply unpleasant in a headset.
 *
 * ⚠️ CAVEAT, and it is not a small one: this reasoning assumes the mirror lives in
 * the projection ALONE. If the view matrix at c0 also mirrors X, the two cancel and
 * the answer flips back. Nothing here can settle that — see the note beside this
 * file for the check that can.
 */
int stereo_right_eye_sign(const float *mono);

#endif /* STEREO_H */
