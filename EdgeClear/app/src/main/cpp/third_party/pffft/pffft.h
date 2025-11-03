/*
 * PFFFT - Pretty Fast FFT Header
 *
 * This is a placeholder header for pffft.
 * Download actual library from: https://bitbucket.org/jpommier/pffft
 *
 * Replace with actual pffft.h before production build.
 */

#ifndef PFFFT_H
#define PFFFT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { PFFFT_REAL, PFFFT_COMPLEX } pffft_transform_t;
typedef enum { PFFFT_FORWARD, PFFFT_BACKWARD } pffft_direction_t;

typedef struct PFFFT_Setup PFFFT_Setup;

/* Setup FFT for given size and type */
PFFFT_Setup* pffft_new_setup(int N, pffft_transform_t transform);

/* Destroy FFT setup */
void pffft_destroy_setup(PFFFT_Setup* setup);

/* Perform FFT (ordered output) */
void pffft_transform_ordered(PFFFT_Setup* setup, const float* input,
                             float* output, float* work,
                             pffft_direction_t direction);

/* Reorder FFT output */
void pffft_zreorder(PFFFT_Setup* setup, const float* input, float* output,
                    pffft_direction_t direction);

#ifdef __cplusplus
}
#endif

#endif /* PFFFT_H */
