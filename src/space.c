/* Cassini Drift. The sky is painted once. The planet and its rings share a
   small transparent sprite, so even the shadow across the rings stays aligned. */
#include "scene.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TAU 6.283185307179586
static const double ring_cos = 0.949, ring_sin = 0.315; /* 18-degree tilt */
static const double opening = 0.275; /* apparent minor / major axis */
static const int dither[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}
};

static double clamp01(double v)
{
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

static uint32_t hash(uint32_t v)
{
    v ^= v >> 16;
    v *= UINT32_C(0x7feb352d);
    v ^= v >> 15;
    v *= UINT32_C(0x846ca68b);
    return v ^ (v >> 16);
}

static double fraction(uint32_t v)
{
    return (double)(hash(v) >> 8) / 16777216.0;
}

static double lattice(int x, int y, int period, unsigned seed)
{
    x %= period;
    if (x < 0) x += period;
    return fraction((unsigned)x * 1237u + (unsigned)y * 7349u + seed);
}

/* Smooth value noise. Repeating the horizontal lattice lets the sky scroll
   through a complete revolution without a seam. */
static double noise(double x, double y, int period, unsigned seed)
{
    int ix = (int)floor(x), iy = (int)floor(y);
    double fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    double a = lattice(ix, iy, period, seed), b = lattice(ix + 1, iy, period, seed);
    double c = lattice(ix, iy + 1, period, seed), d = lattice(ix + 1, iy + 1, period, seed);
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}

static double quantize(double value, int levels, int x, int y)
{
    double bias = (dither[y & 3][x & 3] + 0.5) / 16.0;
    return floor(clamp01(value) * levels + bias) / levels;
}

static Color nebula(double nx, double ny, int x, int y, unsigned seed)
{
    double warp = noise(5 * nx, 3 * ny, 5, seed);
    double large = noise(9 * nx, 6 * ny, 9, seed + 1);
    double small = noise(32 * nx, 19 * ny, 32, seed + 2);
    double grain = noise(88 * nx, 47 * ny, 88, seed + 3);
    double dust = 0.55 * large + 0.31 * small + 0.14 * grain;
    double spine = 0.45 + 0.29 * sin(TAU * (nx + 0.12)) + 0.32 * (warp - 0.5);
    double distance = ny - spine;
    double cloud = exp(-distance * distance * 27) * (0.18 + 1.46 * dust);
    double lane = distance + (large - 0.5) * 0.15 + (small - 0.5) * 0.036;
    cloud *= 1 - 0.75 * exp(-lane * lane * 620);
    double strength = quantize(cloud * cloud * 0.97, 28, x, y);
    Color shade = mix(0x060a1b, 0x22314e, (float)clamp01(strength * 2));
    Color cloud_color = mix(0x4b5574, 0x88617c, (float)clamp01(0.55 + distance * 3));
    return mix(shade, cloud_color, (float)clamp01((strength - 0.20) * 0.96));
}

static void draw_sky(Canvas *c, double seconds, unsigned seed)
{
    static Color *sky;
    static int width, height;
    static unsigned sky_seed;
    if (!sky || width != c->width || height != c->height || sky_seed != seed) {
        Color *next = realloc(sky, (size_t)c->width * c->height * sizeof(*sky));
        if (!next) {
            rect(c, 0, 0, c->width, c->height, 0x090e21);
            return;
        }
        sky = next;
        width = c->width;
        height = c->height;
        sky_seed = seed;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                sky[(size_t)y * width + x] = nebula((double)x / width, (double)y / height, x, y, seed);
            }
        }
        for (int i = 0; i < width * height / 360; ++i) {
            unsigned key = seed + (unsigned)i * 7u;
            int x = (int)(width * fraction(key)), y = (int)(height * fraction(key + 1));
            Color *pixel = &sky[(size_t)y * width + x];
            *pixel = mix(*pixel, 0xa6b5ca, (float)(0.16 + 0.38 * fraction(key + 2)));
        }
    }
    int shift = (int)fmod(seconds * 0.54, width);
    for (int y = 0; y < height; ++y) {
        Color *dst = c->pixels + (size_t)y * width;
        const Color *src = sky + (size_t)y * width;
        memcpy(dst, src + shift, (size_t)(width - shift) * sizeof(*dst));
        memcpy(dst + width - shift, src, (size_t)shift * sizeof(*dst));
    }
}

/* A separate star layer moves at a different speed from the cloud bank. */
static void draw_stars(Canvas *c, double seconds, unsigned seed)
{
    int count = c->width * c->height / 1150 + 8;
    for (int i = 0; i < count; ++i) {
        unsigned key = seed + 3499u + (unsigned)i * 11u;
        double along = fraction(key) * c->width - seconds * (i % 4 == 0 ? 1.85 : 1.18);
        int x = (int)floor(along - c->width * floor(along / c->width));
        int y = (int)(fraction(key + 1) * c->height);
        double glow = 0.55 + 0.40 * sin(0.64 * seconds + 17.0 * fraction(key + 2));
        Color tint = i % 5 ? 0xbed3e1 : 0xffc4a0;
        if (i % 19 == 0) {
            Color ray = mix(0x1e2947, tint, (float)(0.18 + glow * 0.25));
            line(c, x - 2, y, x + 2, y, ray);
            line(c, x, y - 2, x, y + 2, ray);
        }
        px(c, x, y, mix(0x59637c, tint, (float)glow));
    }
}

/* Ring coordinates u,v are measured along the apparent long and short axes.
   z points towards the viewer. The same tilted plane supplies latitude on
   the sphere, so the ring crosses the planet's equator in perspective. */
static Color atmosphere(double u, double v, double z, double x, double y,
                        double spin, int ix, int iy)
{
    double tilt = sqrt(1 - opening * opening);
    double latitude = -v * tilt + z * opening;
    double longitude = atan2(u, v * opening + z * tilt) + spin;
    double cloud = latitude + 0.018 * sin(3 * longitude + latitude * 10)
                          + 0.011 * sin(7 * longitude - latitude * 19);
    double belts = 0.48 + 0.20 * sin(cloud * 23) + 0.12 * sin(cloud * 50 + 1.1)
                        + 0.08 * sin(cloud * 93) + 0.07 * sin(cloud * 157 + 1.7);
    /* A small oval storm rolls into view as the planet rotates. */
    double slon = sin((longitude - 0.25) * 0.5) * 2;
    double storm = slon * slon * 12 + (latitude + 0.23) * (latitude + 0.23) * 300;
    belts += 0.22 * exp(-fabs(storm - 1) * 3) - 0.30 * exp(-storm * 1.5);
    Color daylight = mix(0xaf6864, 0xffdfae, (float)quantize(belts, 20, ix, iy));

    /* Upper-left sunlight; on the night side the ambient light is violet. */
    double sun = -0.77 * x - 0.49 * y + 0.40 * z;
    double diffuse = clamp01(0.20 + 0.86 * sun);
    /* Intersection of a sunward ray from this point with the ring plane. */
    const double light_u = -0.77 * 0.949 + 0.49 * 0.315;
    const double light_v = -0.77 * 0.315 - 0.49 * 0.949;
    double crossing = -latitude / (-light_v * tilt + 0.40 * opening);
    if (crossing > 0) {
        double ru = u + crossing * light_u, rv = (v + crossing * light_v) / opening;
        double r = sqrt(ru * ru + rv * rv);
        if (r > 1.23 && r < 2.05 && (r < 1.79 || r > 1.85))
            diffuse *= 0.67;
    }
    Color night = mix(0x231e39, 0x55415b, (float)clamp01(belts * 0.70 + 0.18));
    Color result = mix(night, daylight, (float)quantize(diffuse, 28, ix, iy));
    if (z < 0.14 && sun > 0.06)
        result = mix(result, 0xffdfbb, (float)((1 - z / 0.14) * 0.68));
    return result;
}

static Color ring_pixel(double r, double x, double y, double z, int ix, int iy)
{
    if (r < 1.21 || r > 2.08) return 0;
    double density = 0.94;
    if (r < 1.39) density = 0.20 + (r - 1.21) * 1.8;   /* gauzy inner ring */
    if (r > 1.78 && r < 1.85) density = 0.16;            /* broad division */
    if (r > 1.99 && r < 2.015) density = 0.39;
    if (r > 2.06) density *= (2.08 - r) * 50;

    double grooves = 0.55 + 0.19 * sin(r * 79) + 0.15 * sin(r * 134) + 0.11 * sin(r * 228);
    Color lit = mix(0x996b71, 0xffdcaa, (float)quantize(grooves, 20, ix, iy));
    /* Distance between the planet's centre and a sunward ray from the ring. */
    double along = -(-0.77 * x - 0.49 * y + 0.40 * z);
    if (along > 0) {
        double distance2 = x * x + y * y + z * z - along * along;
        lit = mix(0x4b3f5b, lit, (float)clamp01((distance2 - 0.87) * 6.5));
    }
    return lit | ((Color)(density * 255) << 24);
}

static void paint_planet(Canvas *sprite, int radius, double spin)
{
    for (int iy = 0; iy < sprite->height; ++iy) {
        double y = (double)(iy - sprite->height / 2) / radius;
        for (int ix = 0; ix < sprite->width; ++ix) {
            double x = (double)(ix - sprite->width / 2) / radius;
            double u = x * ring_cos - y * ring_sin, v = x * ring_sin + y * ring_cos;
            double orbit = sqrt(u * u + (v / opening) * (v / opening));
            double disc = x * x + y * y;
            Color result = 0;
            if (disc <= 1) {
                double z = sqrt(1 - disc);
                result = atmosphere(u, v, z, x, y, spin, ix, iy) | 0xff000000u;
            }
            if (disc > 1 || v > 0) {
                Color ring = ring_pixel(orbit, x, y, v * sqrt(1 - opening * opening) / opening, ix, iy);
                if (ring >> 24) {
                    if (result >> 24)
                        result = mix(result & 0xffffffu, ring & 0xffffffu, (float)(ring >> 24) / 255) | 0xff000000u;
                    else result = ring;
                }
            }
            sprite->pixels[(size_t)iy * sprite->width + ix] = result;
        }
    }
}

static void draw_planet(Canvas *c, int cx, int cy, int radius, double seconds)
{
    static Canvas sprite;
    static int last_radius;
    static double last_tick = -1;
    double tick = floor(seconds * 0.7);
    if (radius != last_radius) {
        int width = radius * 4 + 5, height = radius * 2 + 5;
        Color *next = realloc(sprite.pixels, (size_t)width * height * sizeof(*next));
        if (!next) return;
        sprite = (Canvas){width, height, next};
        last_radius = radius;
        last_tick = -1;
    }
    if (tick != last_tick) {
        paint_planet(&sprite, radius, tick * 0.013);
        last_tick = tick;
    }
    int left = cx - sprite.width / 2, top = cy - sprite.height / 2;
    for (int sy = 0; sy < sprite.height; ++sy) {
        int y = sy + top;
        if (y < 0 || y >= c->height) continue;
        for (int sx = 0; sx < sprite.width; ++sx) {
            int x = sx + left;
            if (x < 0 || x >= c->width) continue;
            Color paint = sprite.pixels[(size_t)sy * sprite.width + sx];
            unsigned alpha = paint >> 24;
            Color *dst = &c->pixels[(size_t)y * c->width + x];
            if (alpha == 255) *dst = paint & 0xffffffu;
            else if (alpha) *dst = mix(*dst, paint & 0xffffffu, (float)alpha / 255);
        }
    }
}

/* Sample crater bowls in surface coordinates so each one follows the
   lighting round the curvature of the moon. */
static void draw_moon(Canvas *c, int cx, int cy, int r)
{
    static const double craters[][3] = {
        {-0.29, -0.39, 0.23}, {0.25, -0.48, 0.13}, {-0.50, 0.05, 0.16},
        {0.11, 0.15, 0.31}, {-0.20, 0.63, 0.15}, {0.48, 0.52, 0.10}
    };
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            double nx = (double)x / r, ny = (double)y / r, radial = nx * nx + ny * ny;
            if (radial > 1) continue;
            double sun = 0.34 - 0.64 * nx - 0.49 * ny + 0.22 * sqrt(1 - radial);
            for (size_t k = 0; k < sizeof(craters) / sizeof(*craters); ++k) {
                double dx = (nx - craters[k][0]) / craters[k][2];
                double dy = (ny - craters[k][1]) / craters[k][2];
                if (dx * dx + dy * dy < 1.0) sun += 0.25 * (dx + dy) - 0.11;
            }
            px(c, cx + x, cy + y, mix(0x302c47, 0xf0d9bb, (float)quantize(sun, 15, x, y)));
        }
    }
}

/* Draw a solar wing as parallel rows with a copper frame and blue cells.
   The changing width is the panel slowly turning about its long axis. */
static void panel(Canvas *c, int x, int y, int side, int half_width)
{
    for (int i = 0; i <= 20; ++i) {
        int cx = x + side * i, cy = y + side * i / 3;
        for (int cross = -half_width; cross <= half_width; ++cross) {
            Color paint = cross < 0 ? 0x344766 : 0x28405c;
            if (i % 4 == 0 || cross == 0) paint = 0x648399;
            if (i == 0 || i == 20 || cross == -half_width) paint = 0xc19b77;
            if (cross == half_width) paint = 0x4c4656;
            px(c, cx - cross / 2, cy + cross, paint);
        }
    }
}

static void draw_probe(Canvas *c, double seconds, unsigned seed)
{
    /* One pass lasts a few minutes. Start in clear sky at the left. */
    double route = fmod(seconds / 196.0 + 0.23, 1.0);
    int x = (int)((c->width + 86) * route) - 43;
    int y = (int)(c->height * (0.78 - 0.11 * sin(TAU * route + 0.8))
                  + 3 * sin(seconds * 0.037));
    int width = 2 + (int)(2.8 * (0.5 + 0.5 * sin(seconds * 0.084)));
    line(c, x - 10, y - 4, x + 11, y + 4, 0xb49b85);
    panel(c, x - 10, y - 4, -1, width);
    panel(c, x + 11, y + 4, 1, width);

    /* Foil blanket, equipment shelf and optical head. */
    oval(c, x, y, 6, 5, 0x624a4c);
    rect(c, x - 5, y - 3, 8, 6, 0xbd8554);
    line(c, x - 5, y - 3, x + 1, y - 4, 0xffda90);
    line(c, x - 4, y, x - 1, y + 2, 0xe6ac6e);
    rect(c, x + 2, y - 2, 4, 6, 0x73515a);
    rect(c, x + 4, y, 4, 3, 0xb0b8bb);
    px(c, x + 7, y, 0x3b5266);
    line(c, x - 3, y + 3, x - 13, y + 15, 0x9b8d8c);
    rect(c, x - 15, y + 14, 4, 2, 0xddd5b6);
    if (fmod(seconds + fraction(seed), 3.7) < 0.29) px(c, x + 2, y + 3, 0xff694c);

    /* High-gain dish points up and right. The cream lip catches sunlight. */
    line(c, x, y - 3, x + 3, y - 9, 0xc6b9a6);
    for (int i = -6; i <= 6; ++i) {
        int lip = y - 11 + i / 2;
        int depth = (36 - i * i) / 8;
        line(c, x + 3 + i, lip, x + 3 + i, lip + depth, 0x86858c);
        px(c, x + 3 + i, lip, 0xfce5c4);
    }
    line(c, x + 1, y - 12, x + 6, y - 16, 0xa9adb2);
    line(c, x + 9, y - 8, x + 6, y - 16, 0xcac4b7);
    px(c, x + 6, y - 17, 0xffd59e);
}

void scene_draw(Canvas *c, double seconds, unsigned seed)
{
    draw_sky(c, seconds, seed);
    draw_stars(c, seconds, seed);
    int radius = (int)fmax(4, fmin(c->height * 0.235, c->width * 0.210));
    int cx = (int)(c->width * (0.61 + 0.040 * sin(seconds * 0.009)));
    int cy = (int)(c->height * (0.44 + 0.045 * sin(seconds * 0.014)));
    double orbit = seconds * 0.021 + 2.51;
    int mx = cx + (int)(radius * (2.39 * cos(orbit) + 0.12 * sin(orbit)));
    int my = cy + (int)(radius * (-0.78 * cos(orbit) + 0.95 * sin(orbit)));
    int mr = (int)fmax(2, radius * (0.13 + 0.035 * sin(orbit)));
    if (sin(orbit) < 0) draw_moon(c, mx, my, mr);
    draw_planet(c, cx, cy, radius, seconds);
    if (sin(orbit) >= 0) draw_moon(c, mx, my, mr);
    draw_probe(c, seconds, seed);
}
