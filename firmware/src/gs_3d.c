/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* A small software 3D for the screen: flat-shaded convex meshes (a cube, an octahedron, a pyramid, an icosahedron) that
 * turn, orbit and stretch, seen from above at a pitch, with a perspective floor behind them. Integer only. The shading
 * walks a ramp of the palette (orange, silver, red) by how a face turns to a light fixed to the camera; the faces of one
 * mesh are painted far to near; a caller paints its meshes far to near too.
 *
 * Each triangle carries a mask of the edges worth outlining (a cube's square sides, not their diagonals). */

#define G3_MAXF 20

typedef struct {
    uint8_t nv, nf;
    const int8_t (*v)[3];
    const uint8_t (*f)[4];          /* a b c, and the edge mask: bit 0 a-b, bit 1 b-c, bit 2 c-a */
} g3_mesh_t;

static const int8_t G3_CUBE_V[8][3] = {{-32, -32, -32}, {32, -32, -32}, {32, 32, -32}, {-32, 32, -32},
                                       {-32, -32, 32},  {32, -32, 32},  {32, 32, 32},  {-32, 32, 32}};
static const uint8_t G3_CUBE_F[12][4] = {{0, 1, 2, 3}, {0, 2, 3, 6}, {4, 6, 5, 6}, {4, 7, 6, 3}, {0, 4, 5, 3}, {0, 5, 1, 6},
                                         {3, 2, 6, 3}, {3, 6, 7, 6}, {0, 3, 7, 3}, {0, 7, 4, 6}, {1, 5, 6, 3}, {1, 6, 2, 6}};
static const int8_t G3_OCT_V[6][3] = {{32, 0, 0}, {-32, 0, 0}, {0, 32, 0}, {0, -32, 0}, {0, 0, 32}, {0, 0, -32}};
static const uint8_t G3_OCT_F[8][4] = {{0, 2, 4, 7}, {2, 1, 4, 7}, {1, 3, 4, 7}, {3, 0, 4, 7},
                                       {2, 0, 5, 7}, {1, 2, 5, 7}, {3, 1, 5, 7}, {0, 3, 5, 7}};
static const int8_t G3_PYR_V[5][3] = {{-32, -32, -32}, {32, -32, -32}, {32, -32, 32}, {-32, -32, 32}, {0, 32, 0}};
static const uint8_t G3_PYR_F[6][4] = {{0, 1, 2, 3}, {0, 2, 3, 6}, {0, 4, 1, 7}, {1, 4, 2, 7}, {2, 4, 3, 7}, {3, 4, 0, 7}};
static const int8_t G3_ICO_V[12][3] = {{-19, 31, 0}, {19, 31, 0}, {-19, -31, 0}, {19, -31, 0}, {0, -19, 31}, {0, 19, 31},
                                       {0, -19, -31}, {0, 19, -31}, {31, 0, -19}, {31, 0, 19}, {-31, 0, -19}, {-31, 0, 19}};
static const uint8_t G3_ICO_F[20][4] = {{0, 11, 5, 7}, {0, 5, 1, 7}, {0, 1, 7, 7}, {0, 7, 10, 7}, {0, 10, 11, 7},
                                        {1, 5, 9, 7}, {5, 11, 4, 7}, {11, 10, 2, 7}, {10, 7, 6, 7}, {7, 1, 8, 7},
                                        {3, 9, 4, 7}, {3, 4, 2, 7}, {3, 2, 6, 7}, {3, 6, 8, 7}, {3, 8, 9, 7},
                                        {4, 9, 5, 7}, {2, 4, 11, 7}, {6, 2, 10, 7}, {8, 6, 7, 7}, {9, 8, 1, 7}};
static const g3_mesh_t G3_CUBE = {8, 12, G3_CUBE_V, G3_CUBE_F};
static const g3_mesh_t G3_OCT = {6, 8, G3_OCT_V, G3_OCT_F};
static const g3_mesh_t G3_PYR = {5, 6, G3_PYR_V, G3_PYR_F};
static const g3_mesh_t G3_ICO = {12, 20, G3_ICO_V, G3_ICO_F};

/* the camera: where the world's origin lands on the screen, the focal length, the distance, the pitch (0 = level) */
static int g3_cx = 120, g3_cy = 100, g3_f = 220, g3_d = 220, g3_cp = 32767, g3_sp;

static int g3_sin(int a) { return SK_SIN[a & 1023]; }
static int g3_cos(int a) { return SK_SIN[(a + 256) & 1023]; }

static void g3_cam(int cx, int cy, int f, int d, int pitch)
{
    g3_cx = cx;
    g3_cy = cy;
    g3_f = f;
    g3_d = d;
    g3_cp = g3_cos(pitch);
    g3_sp = g3_sin(pitch);
}

static uint32_t g3_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}

/* a flat triangle: scanlines between its edges */
static void g3_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    int t, y;
    int32_t s02, s01 = 0, s12 = 0;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y2 == y0) {
        int l = x0 < x1 ? x0 : x1, r = x0 > x1 ? x0 : x1;
        if (x2 < l) l = x2;
        if (x2 > r) r = x2;
        fb_hline(l, y0, r - l + 1, c);
        return;
    }
    s02 = ((x2 - x0) * 65536) / (y2 - y0);
    if (y1 > y0)
        s01 = ((x1 - x0) * 65536) / (y1 - y0);
    if (y2 > y1)
        s12 = ((x2 - x1) * 65536) / (y2 - y1);
    for (y = y0; y <= y2; y++) {
        int32_t xa = x0 * 65536 + s02 * (y - y0), xb;
        int l, r;
        if (y < fb_cy0 || y >= fb_cy1)
            continue;
        if (y < y1)
            xb = x0 * 65536 + s01 * (y - y0);
        else
            xb = x1 * 65536 + s12 * (y - y1);
        l = (int)(xa >> 16);
        r = (int)(xb >> 16);
        if (l > r) { t = l; l = r; r = t; }
        fb_hline(l, y, r - l + 1, c);
    }
}

static struct {
    int32_t z;
    int16_t x[3], y[3];
    uint8_t c, e;
} g3_fs[G3_MAXF];

/* one mesh. (ox, oy, oz): where its centre is; (sx, sy, sz): how it is stretched, 16 = the mesh's own size (half-width 32
 * units); (ax, ay, az): its turning, 0..1023 a circle. ramp, rlen: the palette run its faces take, bias shifts the light;
 * ramp 255: no fill (wire only). edge: the colour of its outlines, 255: none. */
static void g3_draw(const g3_mesh_t *m, int ox, int oy, int oz, int sx, int sy, int sz, int ax, int ay, int az,
                    uint8_t ramp, int rlen, int bias, uint8_t edge)
{
    int32_t vx[12], vy[12], vz[12];
    int16_t px[12], py[12];
    int i, k, nf = 0, cyw = g3_cos(ay), syw = g3_sin(ay), cxw = g3_cos(ax), sxw = g3_sin(ax), czw = g3_cos(az), szw = g3_sin(az);
    int32_t ccx, ccy, ccz;
    for (i = 0; i < m->nv; i++) {
        int32_t x = m->v[i][0] * sx / 16, y = m->v[i][1] * sy / 16, z = m->v[i][2] * sz / 16, x1, z1, y2, z2, x3, y3, yy, zz;
        x1 = (x * cyw + z * syw) >> 15;
        z1 = (z * cyw - x * syw) >> 15;
        y2 = (y * cxw - z1 * sxw) >> 15;
        z2 = (y * sxw + z1 * cxw) >> 15;
        x3 = (x1 * czw - y2 * szw) >> 15;
        y3 = (x1 * szw + y2 * czw) >> 15;
        x3 += ox;
        y3 += oy;
        z2 += oz;
        yy = (y3 * g3_cp + z2 * g3_sp) >> 15;                /* the pitch: far goes up the screen */
        zz = ((z2 * g3_cp - y3 * g3_sp) >> 15) + g3_d;
        if (zz < 40)
            zz = 40;
        vx[i] = x3;
        vy[i] = yy;
        vz[i] = zz;
        px[i] = (int16_t)(g3_cx + x3 * g3_f / zz);
        py[i] = (int16_t)(g3_cy - yy * g3_f / zz);
    }
    ccx = ox;
    ccy = (oy * g3_cp + oz * g3_sp) >> 15;
    ccz = ((oz * g3_cp - oy * g3_sp) >> 15) + g3_d;
    for (k = 0; k < m->nf && nf < G3_MAXF; k++) {
        int a = m->f[k][0], b = m->f[k][1], c = m->f[k][2];
        int32_t ux = vx[b] - vx[a], uy = vy[b] - vy[a], uz = vz[b] - vz[a];
        int32_t wx = vx[c] - vx[a], wy = vy[c] - vy[a], wz = vz[c] - vz[a];
        int32_t nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx, m_, mag;
        int32_t gx = (vx[a] + vx[b] + vx[c]) / 3 - ccx, gy = (vy[a] + vy[b] + vy[c]) / 3 - ccy, gz = (vz[a] + vz[b] + vz[c]) / 3 - ccz;
        int lit, idx;
        m_ = nx < 0 ? -nx : nx;
        if ((ny < 0 ? -ny : ny) > m_) m_ = ny < 0 ? -ny : ny;
        if ((nz < 0 ? -nz : nz) > m_) m_ = nz < 0 ? -nz : nz;
        while (m_ > 1023) {
            nx >>= 1; ny >>= 1; nz >>= 1; m_ >>= 1;
        }
        if (nx * gx + ny * gy + nz * gz < 0) {               /* facing out of the mesh's centre */
            nx = -nx; ny = -ny; nz = -nz;
        }
        if (nx * vx[a] + ny * vy[a] + nz * vz[a] >= 0 && ramp != 255)      /* a back face */
            continue;
        mag = (int32_t)g3_isqrt((uint32_t)(nx * nx + ny * ny + nz * nz));
        if (mag < 1)
            mag = 1;
        lit = (int)((nx * 1024 / mag * -410 + ny * 1024 / mag * 614 + nz * 1024 / mag * -717) >> 10);
        idx = ((lit + 350) * rlen) / 1400 + bias;
        idx = idx < 0 ? 0 : idx >= rlen ? rlen - 1 : idx;
        g3_fs[nf].z = vz[a] + vz[b] + vz[c];
        g3_fs[nf].x[0] = px[a]; g3_fs[nf].x[1] = px[b]; g3_fs[nf].x[2] = px[c];
        g3_fs[nf].y[0] = py[a]; g3_fs[nf].y[1] = py[b]; g3_fs[nf].y[2] = py[c];
        g3_fs[nf].c = (uint8_t)(ramp + idx);
        g3_fs[nf].e = m->f[k][3];
        nf++;
    }
    for (i = 1; i < nf; i++) {                              /* far to near */
        int j = i;
        typeof(g3_fs[0]) t = g3_fs[i];
        for (; j > 0 && g3_fs[j - 1].z < t.z; j--)
            g3_fs[j] = g3_fs[j - 1];
        g3_fs[j] = t;
    }
    for (i = 0; i < nf; i++) {
        if (ramp != 255)
            g3_tri(g3_fs[i].x[0], g3_fs[i].y[0], g3_fs[i].x[1], g3_fs[i].y[1], g3_fs[i].x[2], g3_fs[i].y[2], g3_fs[i].c);
        if (edge != 255) {
            if (g3_fs[i].e & 1) fb_line(g3_fs[i].x[0], g3_fs[i].y[0], g3_fs[i].x[1], g3_fs[i].y[1], edge);
            if (g3_fs[i].e & 2) fb_line(g3_fs[i].x[1], g3_fs[i].y[1], g3_fs[i].x[2], g3_fs[i].y[2], edge);
            if (g3_fs[i].e & 4) fb_line(g3_fs[i].x[2], g3_fs[i].y[2], g3_fs[i].x[0], g3_fs[i].y[0], edge);
        }
    }
}
