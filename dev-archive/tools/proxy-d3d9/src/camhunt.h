/* camhunt.h — the camera-matrix instrument for Dead Space 2.
 *
 * READ-ONLY. Nothing here ever modifies an upload. It answers one question:
 * which vertex-shader constant register receives this game's projection matrix,
 * and in which packing. Everything about stereo comes after that.
 */
#ifndef CAMHUNT_H
#define CAMHUNT_H

#include <windows.h>

/* Layout codes returned by camhunt_classify(). */
#define CAMHUNT_NONE 0
#define CAMHUNT_ROW  'R'   /* register i = row i    (D3D row-vector convention) */
#define CAMHUNT_COL  'C'   /* register i = column i (HLSL default column-major)  */

/* Pure function: is this 4-register window shaped like a perspective matrix?
 * Returns CAMHUNT_NONE, CAMHUNT_ROW or CAMHUNT_COL. Separated out and kept free
 * of state precisely so it can be tested against constructed matrices with no
 * game running — see test/camhunt_selftest.c. */
int camhunt_classify(const float *p);

/* Feed one SetVertexShaderConstantF upload to the instrument. */
void camhunt_observe(unsigned int start, const float *data, unsigned int count);

/* Hook / unhook the device's SetVertexShaderConstantF. */
void camhunt_install(void *device, HMODULE real_d3d9);
void camhunt_remove(void);

#endif
