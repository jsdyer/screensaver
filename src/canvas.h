#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

/* Top-down rows of packed 0xRRGGBB pixels. (0, 0) is the upper left. */
typedef uint32_t Color;
typedef struct {
    int width, height;
    Color *pixels;
} Canvas;

/* Drawing outside the canvas is clipped. Rectangles use width and height,
   ovals use radii, and a line includes both of its endpoints. */
void px(Canvas *c, int x, int y, Color color);
void line(Canvas *c, int x0, int y0, int x1, int y1, Color color);
void rect(Canvas *c, int x, int y, int w, int h, Color color);
void oval(Canvas *c, int cx, int cy, int rx, int ry, Color color);

/* Linear blend from a (amount = 0) to b (amount = 1). */
Color mix(Color a, Color b, float amount);

#endif
