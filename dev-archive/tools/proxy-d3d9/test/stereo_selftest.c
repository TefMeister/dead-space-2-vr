/*
 * stereo_selftest.c — check the per-eye shear against THE MEASURED MATRIX.
 *
 * The standing rule on this account is to test against independently constructed
 * ground truth rather than a transcription of the code under test. Here that means
 * two things, and both matter:
 *
 *   1. The input is the matrix Dead Space 2 actually uploaded on 2026-09-14, typed
 *      from the recon note, NOT a textbook perspective built from a FOV. If the
 *      shear only worked on well-behaved matrices we would never find out.
 *
 *   2. Every expectation below is computed from the closed-form algebra written out
 *      in the comments, in this file, by hand — never by calling stereo_make_eye()
 *      and comparing it to itself.
 *
 * Runs in a process of our own. No game, no device, no d3d9 at all.
 * ⚠️ NOTHING HERE HAS BEEN RUN AGAINST THE GAME. This proves the arithmetic is what
 * we say it is; it does not prove Dead Space 2 will accept a sheared c4.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/stereo.h"

static int failures = 0;
static int checks = 0;

static void ok(int cond, const char *what) {
    ++checks;
    if (cond) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        ++failures;
    }
}

static void okf(double got, double want, double tol, const char *what) {
    ++checks;
    if (fabs(got - want) <= tol) {
        printf("  ok    %-52s (%.9f)\n", what, got);
    } else {
        printf("  FAIL  %-52s got %.9f want %.9f (tol %.1e)\n", what, got, want, tol);
        ++failures;
    }
}

/*
 * THE MEASURED MATRIX. Dead Space 2, c4, layout R, 2026-09-14 play session.
 * Source: modding-notes/2026-09-14c-the-camera-is-found.md
 *
 *   [ -1.944444   0.000000   0.000000   0.000000 ]
 *   [  0.000000   3.456790   0.000000   0.000000 ]
 *   [  0.000000   0.000000   0.990097   1.000000 ]
 *   [  0.000000   0.000000  -0.099010   0.000000 ]
 */
static const float DS2_C4[16] = {
    -1.944444f, 0.0f,       0.0f,       0.0f,
     0.0f,      3.456790f,  0.0f,       0.0f,
     0.0f,      0.0f,       0.990097f,  1.0f,
     0.0f,      0.0f,      -0.099010f,  0.0f,
};

/* Row-vector transform, written out longhand so it cannot quietly agree with a
   bug in the code under test. clip = view * M, index = row*4 + col. */
static void project(const float *m, double vx, double vy, double vz,
                    double *ndc_x, double *ndc_y, double *ndc_z) {
    double cx = vx*m[0] + vy*m[4] + vz*m[8]  + 1.0*m[12];
    double cy = vx*m[1] + vy*m[5] + vz*m[9]  + 1.0*m[13];
    double cz = vx*m[2] + vy*m[6] + vz*m[10] + 1.0*m[14];
    double cw = vx*m[3] + vy*m[7] + vz*m[11] + 1.0*m[15];
    *ndc_x = cx / cw;
    *ndc_y = cy / cw;
    *ndc_z = cz / cw;
}

int main(void) {
    const double XS = -1.944444, YS = 3.456790;
    const double ZC = 2.5;        /* convergence distance, view-space units */
    const double E  = 0.032;      /* an eye offset; ~32 mm if 1 unit == 1 m */

    float left[16], right[16], mono[16];

    printf("stereo_selftest — per-eye shear, against the MEASURED c4 matrix\n\n");

    /* ---- 1. the sign question, which is the whole point of this file ---- */
    printf("The sign of the right eye's offset\n");
    {
        int s = stereo_right_eye_sign(DS2_C4);
        ok(s == -1, "DS2's mirrored X axis puts the RIGHT eye at NEGATIVE view.x");

        /* An ordinary, unmirrored projection must give the opposite answer, or the
           rule is not a rule, it is this one matrix memorised. */
        float textbook[16];
        memcpy(textbook, DS2_C4, sizeof textbook);
        textbook[M_XS] = 1.944444f;
        ok(stereo_right_eye_sign(textbook) == +1,
           "an unmirrored projection puts the right eye at POSITIVE view.x");
    }

    /* ---- 2. mono passthrough ---- */
    printf("\nA zero offset must change nothing at all\n");
    stereo_make_eye(DS2_C4, 0.0f, (float)ZC, mono);
    {
        int same = 1;
        for (int i = 0; i < 16; ++i)
            if (mono[i] != DS2_C4[i]) same = 0;
        ok(same, "e = 0 reproduces the measured matrix bit for bit");
    }

    /* ---- 3. the two eyes ---- */
    int s = stereo_right_eye_sign(DS2_C4);
    stereo_make_eye(DS2_C4, (float)(-s * E), (float)ZC, left);
    stereo_make_eye(DS2_C4, (float)(+s * E), (float)ZC, right);

    printf("\nBoth slots land, with the values the algebra predicts\n");
    okf(right[M_SHX], XS * (s * E) / ZC, 1e-7, "right eye m[8]  == xs*e/zc   (convergence)");
    okf(left[M_SHX],  XS * (-s * E) / ZC, 1e-7, "left  eye m[8]  == xs*e/zc   (convergence)");
    okf(right[M_TRX], -XS * (s * E), 1e-7, "right eye m[12] == -xs*e     (eye offset)");
    okf(left[M_TRX],  -XS * (-s * E), 1e-7, "left  eye m[12] == -xs*e     (eye offset)");
    ok(right[M_SHX] != 0.0f && right[M_TRX] != 0.0f,
       "both are non-zero (a no-op would pass everything below)");

    printf("\nEverything except m[8] and m[12] is untouched — the depth triplet especially\n");
    {
        int intact = 1;
        for (int i = 0; i < 16; ++i)
            if (i != M_SHX && i != M_TRX
                && (left[i] != DS2_C4[i] || right[i] != DS2_C4[i])) intact = 0;
        ok(intact, "all 14 other elements identical in both eyes");
        ok(right[M_ZS] == DS2_C4[M_ZS] && right[M_ZW] == DS2_C4[M_ZW]
           && right[M_ZB] == DS2_C4[M_ZB],
           "m[10]/m[11]/m[14] carried over — the non-textbook depth map survives");
    }

    /* ---- 4. behaviour at, nearer than, and beyond convergence ---- */
    printf("\nAt the convergence distance the two eyes must agree with mono exactly\n");
    {
        double lx, ly, lz, rx, ry, rz, mx, my, mz;
        for (double vx = -1.5; vx <= 1.5; vx += 0.75) {
            project(left,   vx, 0.4, ZC, &lx, &ly, &lz);
            project(right,  vx, 0.4, ZC, &rx, &ry, &rz);
            project(DS2_C4, vx, 0.4, ZC, &mx, &my, &mz);
            char lbl[80];
            snprintf(lbl, sizeof lbl, "x=%+.2f at z=zc: left==right==mono", vx);
            okf(lx, mx, 1e-6, lbl);
            okf(rx, mx, 1e-6, lbl);
        }
    }

    printf("\nParallax has the right SIGN either side of convergence\n");
    {
        double lx, ly, lz, rx, ry, rz;
        project(left,  0.2, 0.0, 1.0, &lx, &ly, &lz);   /* nearer than zc=2.5 */
        project(right, 0.2, 0.0, 1.0, &rx, &ry, &rz);
        ok(rx < lx, "nearer than convergence: right-eye image is LEFT of the left-eye image");

        project(left,  0.2, 0.0, 60.0, &lx, &ly, &lz);  /* far beyond zc */
        project(right, 0.2, 0.0, 60.0, &rx, &ry, &rz);
        ok(rx > lx, "beyond convergence: the disparity reverses, as it must");
    }

    printf("\nDisparity matches the closed form  -xs*(e_r - e_l)*(1/z - 1/zc)\n");
    {
        const double zs[] = { 0.15, 0.5, 1.0, 2.5, 7.0, 40.0 };
        for (unsigned i = 0; i < sizeof zs / sizeof zs[0]; ++i) {
            double z = zs[i], lx, ly, lz, rx, ry, rz;
            project(left,  0.3, -0.2, z, &lx, &ly, &lz);
            project(right, 0.3, -0.2, z, &rx, &ry, &rz);
            double want = -XS * ((s*E) - (-s*E)) * (1.0/z - 1.0/ZC);
            char lbl[80];
            snprintf(lbl, sizeof lbl, "disparity at z=%.2f", z);
            okf(rx - lx, want, 1e-6, lbl);
        }
    }

    printf("\nThe shear touches x only — y and depth are identical in both eyes\n");
    {
        const double zs[] = { 0.1, 0.3, 1.0, 4.0, 25.0, 500.0 };
        for (unsigned i = 0; i < sizeof zs / sizeof zs[0]; ++i) {
            double z = zs[i], lx, ly, lz, rx, ry, rz, mx, my, mz;
            project(left,   0.7, 0.9, z, &lx, &ly, &lz);
            project(right,  0.7, 0.9, z, &rx, &ry, &rz);
            project(DS2_C4, 0.7, 0.9, z, &mx, &my, &mz);
            char lbl[80];
            snprintf(lbl, sizeof lbl, "z=%.2f: ndc.y unchanged", z);
            okf(ly, my, 1e-9, lbl);
            okf(ry, my, 1e-9, lbl);
            snprintf(lbl, sizeof lbl, "z=%.2f: ndc.z unchanged", z);
            okf(lz, mz, 1e-9, lbl);
            okf(rz, mz, 1e-9, lbl);
        }
    }

    /* ---- 5. the measured depth map, re-derived here rather than assumed ---- */
    printf("\nThe measured depth map behaves as the note describes\n");
    {
        double mx, my, mz;
        project(DS2_C4, 0.0, 0.0, 0.1, &mx, &my, &mz);
        /* 3e-6 rather than 1e-6: the matrix above is typed from the log's six
           printed decimals, so -m[14]/m[10] is 0.1000003, not 0.1 exactly. The
           slack is the note's rounding, not slack in the maths. */
        okf(mz, 0.0, 3e-6, "near plane 0.1 maps to ndc.z == 0");
        project(DS2_C4, 0.0, 0.0, 1e9, &mx, &my, &mz);
        okf(mz, 0.990097, 1e-6, "z -> infinity approaches 0.990097, never 1.0");
    }

    /* ---- 6. refusing nonsense ---- */
    printf("\nA meaningless convergence is refused rather than acted on\n");
    {
        float bad[16];
        stereo_make_eye(DS2_C4, (float)E, 0.0f, bad);
        int same = 1;
        for (int i = 0; i < 16; ++i) if (bad[i] != DS2_C4[i]) same = 0;
        ok(same, "zc = 0 leaves the matrix mono instead of dividing by zero");

        stereo_make_eye(DS2_C4, (float)E, -3.0f, bad);
        same = 1;
        for (int i = 0; i < 16; ++i) if (bad[i] != DS2_C4[i]) same = 0;
        ok(same, "zc < 0 likewise");
    }

    /* ---- 7. the aspect and FOV the note claims, checked not copied ---- */
    printf("\nThe note's readings of the measured matrix reproduce\n");
    okf(YS / XS, -16.0/9.0, 1e-5, "ys/xs == -16/9");
    okf(2.0 * atan(1.0 / fabs(XS)) * 180.0 / M_PI, 54.43, 0.01, "horizontal FOV 54.43 deg");
    okf(-(-0.099010) / 0.990097, 0.1, 1e-6, "near plane 0.1 from -m[14]/m[10]");

    printf(failures ? "\nSELFTEST FAILED (%d of %d)\n" : "\nSELFTEST PASSED (%d failures, %d checks)\n",
           failures, checks);
    return failures ? 1 : 0;
}
