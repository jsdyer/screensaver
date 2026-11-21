#ifndef SCENE_H
#define SCENE_H

#include "canvas.h"

/* Each screensaver supplies one scene. Time is elapsed animation time in
   seconds, and the seed stays constant for the lifetime of a run. */
void scene_draw(Canvas *c, double seconds, unsigned seed);

#endif
