#include "canvas.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void px(Canvas *c, int x, int y, Color color)
{
    if (x >= 0 && x < c->width && y >= 0 && y < c->height)
        c->pixels[(size_t)y * c->width + x] = color;
}

/* Clamp a span edge to [0, limit]. The right and bottom edges are exclusive.
   Wide arithmetic lets x + width fall safely beyond the canvas. */
static int clip_edge(int64_t edge, int limit)
{
    if (edge < 0) return 0;
    if (edge > limit) return limit;
    return (int)edge;
}

static void fill_row(Canvas *c, int y, int left, int right, Color color)
{
    Color *row = c->pixels + (size_t)y * c->width;
    for (int x = left; x < right; ++x)
        row[x] = color;
}

void rect(Canvas *c, int x, int y, int w, int h, Color color)
{
    if (w <= 0 || h <= 0) return;
    int left = clip_edge(x, c->width);
    int right = clip_edge((int64_t)x + w, c->width);
    int top = clip_edge(y, c->height);
    int bottom = clip_edge((int64_t)y + h, c->height);
    for (int row = top; row < bottom; ++row)
        fill_row(c, row, left, right, color);
}

static void swap_int(int *a, int *b)
{
    int copy = *a;
    *a = *b;
    *b = copy;
}

void line(Canvas *c, int x0, int y0, int x1, int y1, Color color)
{
    if (x0 == x1 && y0 == y1) {
        px(c, x0, y0, color);
        return;
    }

    /* Step along the longer axis so consecutive pixels always touch.
       For steep lines we exchange x and y, then exchange them back to draw. */
    bool steep = llabs((long long)y1 - y0) > llabs((long long)x1 - x0);
    if (steep) {
        swap_int(&x0, &y0);
        swap_int(&x1, &y1);
    }
    if (x0 > x1) {
        swap_int(&x0, &x1);
        swap_int(&y0, &y1);
    }

    int limit = steep ? c->height : c->width;
    int first = clip_edge(x0, limit);
    int end = clip_edge((int64_t)x1 + 1, limit);
    double slope = ((double)y1 - y0) / ((double)x1 - x0);
    double y = y0 + slope * ((double)first - x0);
    for (int x = first; x < end; ++x, y += slope) {
        int nearest = (int)floor(y + 0.5);
        if (steep) px(c, nearest, x, color);
        else       px(c, x, nearest, color);
    }
}

void oval(Canvas *c, int cx, int cy, int rx, int ry, Color color)
{
    if (rx < 0 || ry < 0) return;
    int top = clip_edge((int64_t)cy - ry, c->height);
    int bottom = clip_edge((int64_t)cy + ry + 1, c->height);

    /* Solve the ellipse equation for the horizontal reach of each row. */
    for (int y = top; y < bottom; ++y) {
        double dy = ry ? ((double)y - cy) / ry : 0.0;
        int reach = (int)(rx * sqrt(1.0 - dy * dy));
        int left = clip_edge((int64_t)cx - reach, c->width);
        int right = clip_edge((int64_t)cx + reach + 1, c->width);
        fill_row(c, y, left, right, color);
    }
}

static Color mix_channel(int a, int b, float amount)
{
    return (Color)(a + (b - a) * amount + 0.5f);
}

Color mix(Color a, Color b, float amount)
{
    if (amount <= 0.0f) return a;
    if (amount >= 1.0f) return b;
    Color r = mix_channel((a >> 16) & 255, (b >> 16) & 255, amount);
    Color g = mix_channel((a >> 8) & 255, (b >> 8) & 255, amount);
    Color blue = mix_channel(a & 255, b & 255, amount);
    return (r << 16) | (g << 8) | blue;
}
