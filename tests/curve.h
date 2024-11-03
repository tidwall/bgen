#ifndef CURVE_H
#define CURVE_H

#include <stdint.h>

//------------------------------------------------------------------------------
// Hilbert Curve Encoding
// From (Public Domain): https://github.com/rawrunprotected/hilbert_curves
//------------------------------------------------------------------------------
static uint32_t curve_hilbert(double x, double y, double window[4]) {
    // Prepare inputs by converting doubles to ints
    x = (x - window[0]) / (window[2]-window[0]);
    y = (y - window[1]) / (window[3]-window[1]);
    uint16_t xint = (x < 0.0 ? 0.0 : x > 1.0 ? 1.0 : x) * 0xFFFF;
    uint16_t yint = (y < 0.0 ? 0.0 : y > 1.0 ? 1.0 : y) * 0xFFFF;
    // Initial prefix scan round, prime with x and y
    uint16_t a = xint ^ yint;
    uint16_t b = 0xFFFF ^ a;
    uint16_t c = 0xFFFF ^ (xint | yint);
    uint16_t d = xint & (yint ^ 0xFFFF);
    uint16_t A = a | (b >> 1);
    uint16_t B = (a >> 1) ^ a;
    uint16_t C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
    uint16_t D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;
    a = A, b = B, c = C, d = D;
    A = ((a & (a >> 2)) ^ (b & (b >> 2)));
    B = ((a & (b >> 2)) ^ (b & ((a ^ b) >> 2)));
    C ^= ((a & (c >> 2)) ^ (b & (d >> 2)));
    D ^= ((b & (c >> 2)) ^ ((a ^ b) & (d >> 2)));
    a = A, b = B, c = C, d = D;
    A = ((a & (a >> 4)) ^ (b & (b >> 4)));
    B = ((a & (b >> 4)) ^ (b & ((a ^ b) >> 4)));
    C ^= ((a & (c >> 4)) ^ (b & (d >> 4)));
    D ^= ((b & (c >> 4)) ^ ((a ^ b) & (d >> 4)));
    // Final round and projection
    a = A, b = B, c = C, d = D;
    C ^= ((a & (c >> 8)) ^ (b & (d >> 8)));
    D ^= ((b & (c >> 8)) ^ ((a ^ b) & (d >> 8)));
    // Undo transformation prefix scan
    a = C ^ (C >> 1);
    b = D ^ (D >> 1);
    // Recover index bits
    uint32_t i0 = xint ^ yint;
    uint32_t i1 = b | (0xFFFF ^ (i0 | a));
    // Interleave the index bits into a fully formed curve
    i0 = (i0 | (i0 << 8)) & 0x00ff00ff;
    i0 = (i0 | (i0 << 4)) & 0x0f0f0f0f;
    i0 = (i0 | (i0 << 2)) & 0x33333333;
    i0 = (i0 | (i0 << 1)) & 0x55555555;
    i1 = (i1 | (i1 << 8)) & 0x00ff00ff;
    i1 = (i1 | (i1 << 4)) & 0x0f0f0f0f;
    i1 = (i1 | (i1 << 2)) & 0x33333333;
    i1 = (i1 | (i1 << 1)) & 0x55555555;
    return (i1 << 1) | i0;
}

static uint32_t curve_z(double x, double y, double window[4]) {
    // Prepare inputs by converting doubles to ints
    x = (x - window[0]) / (window[2]-window[0]);
    y = (y - window[1]) / (window[3]-window[1]);
    uint16_t xint = (x < 0.0 ? 0.0 : x > 1.0 ? 1.0 : x) * 0xFFFF;
    uint16_t yint = (y < 0.0 ? 0.0 : y > 1.0 ? 1.0 : y) * 0xFFFF;
    uint32_t i0 = xint;
    uint32_t i1 = yint;
    // Interleave the index bits into a fully formed curve
    i0 = (i0 | (i0 << 8)) & 0x00ff00ff;
    i0 = (i0 | (i0 << 4)) & 0x0f0f0f0f;
    i0 = (i0 | (i0 << 2)) & 0x33333333;
    i0 = (i0 | (i0 << 1)) & 0x55555555;
    i1 = (i1 | (i1 << 8)) & 0x00ff00ff;
    i1 = (i1 | (i1 << 4)) & 0x0f0f0f0f;
    i1 = (i1 | (i1 << 2)) & 0x33333333;
    i1 = (i1 | (i1 << 1)) & 0x55555555;
    return (i1 << 1) | i0;
}

#endif
