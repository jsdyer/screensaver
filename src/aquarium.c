/* A planted freshwater aquarium. The static hardscape is painted once;
   fish and leaves are drawn back-to-front over that image on each frame. */
#include "scene.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

static uint32_t noise(unsigned n)
{
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    return n ^ (n >> 16);
}

static double unit(unsigned n)
{
    return (noise(n) & 65535u) / 65535.0;
}

static double clamp01(double v)
{
    if (v < 0) return 0;
    if (v > 1) return 1;
    return v;
}

/* Absolute heights in the illustration are measured on a 320-line canvas. */
static int size_at(const Canvas *c, double size)
{
    return (int)lround(size * c->height / 320.0);
}

static int floor_y(const Canvas *c, int x)
{
    double u = (double)x / c->width;
    return (int)(c->height * (0.842 + 0.041 * sin(u * 7.0 - 1.0)
                                      + 0.012 * sin(u * 15.0 + 1.1)));
}

/* Light shafts fan out underwater. A tiny ordered dither and gravel specks
   give the broad areas a little of the texture of a painted VGA backdrop. */
static void water_and_sand(Canvas *c, unsigned seed)
{
    const Color surface = 0x296b74, deep = 0x062b40, glow = 0x80d6be;
    static const int bayer[4][4] = {
        {0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5}
    };
    for (int x = 0; x < c->width; ++x) {
        int floor = floor_y(c, x);
        double u = (double)x / c->width;
        for (int y = 0; y < c->height; ++y) {
            double v = (double)y / c->height;
            double dither = (bayer[y & 3][x & 3] - 7.5) * 0.00085;
            Color col;
            if (y < floor) {
                double source = (x + y * 0.47) / c->height;
                double shafts = 0.4 + 0.24 * cos(18 * source) +
                                0.18 * cos(41 * source + 1.6) +
                                0.10 * cos(89 * source + 0.8);
                double overhead = 1.0 - 1.1 * fabs(u + v * 0.19 - 0.59);
                double light = clamp01(overhead) * (1.0 - v) * (1.0 - v);
                col = mix(surface, deep, (float)clamp01(v * 1.13 + dither));
                col = mix(col, glow, (float)clamp01(light * shafts * 0.53 + dither));
            } else {
                double depth = (double)(y - floor) / (c->height - floor);
                double path = clamp01(1.0 - fabs(u - 0.53 - depth * 0.10) * (6.6 - 2.2*depth));
                double grain = unit((unsigned)x * 967u + (unsigned)y * 7727u + seed);
                double wave = cos(depth * 45 + sin(u * 30 + depth * 6) * 2.4);
                col = mix(0x3d665e, 0x172f39, (float)(depth * 0.82));
                col = mix(col, 0xd3c4a1, (float)(path * (0.45 + depth * 0.29)));
                if (y - floor < 4)
                    col = mix(col, 0x224d51, (float)((4-y+floor)*0.12));
                col = mix(col, grain > 0.47 ? 0xacc4ab : 0x173f43,
                          (float)(0.12 * grain + 0.034 * wave + 0.035));
            }
            c->pixels[(size_t)y * c->width + x] = col;
        }
    }
    int amount = c->width * c->height / 113;
    for (int i = 0; i < amount; ++i) {
        unsigned k = seed + (unsigned)i * 9;
        int x = (int)(unit(k) * (c->width - 1));
        int floor = floor_y(c, x);
        int y = floor + (int)(unit(k+1) * (c->height - floor - 1));
        double depth = (double)(y - floor) / (c->height - floor);
        int width = 1 + (int)(unit(k+3) * depth * size_at(c, 5));
        Color shade = mix(0x2b4946, 0xadb699, (float)unit(k+2));
        shade = mix(c->pixels[(size_t)y*c->width+x], shade, (float)(0.25+depth*0.65));
        rect(c, x, y, width, 1, shade);
        if (width >= 3) rect(c, x+1, y-1, width-2, 1, mix(shade,0xcbbea0,0.3f));
    }
}

/* A stone is a stack of irregular outlines. Sloping strata catch the light
   along the upper face and deepen into blue at its base. */
static void stone(Canvas *c, int x, int base, int rx, int height, unsigned key)
{
    if (rx < 1 || height < 2) return;
    double phase = unit(key) * 6;
    double lean = (unit(key+1) - 0.5) * 0.56;
    Color face = mix(0x476861, 0x6d7166, (float)unit(key+2));
    for (int dx = -rx; dx <= rx; ++dx) {
        double u = (double)dx/rx;
        double cap = pow(fmax(0, 1.0-u*u), 0.55) *
                     (0.85 + 0.07*sin(5*u+phase) + 0.04*cos(11*u+phase) + lean*u);
        int top = base - (int)(height * cap);
        int bottom = base + (int)(height * (1-u*u) * 0.13);
        double ridge = 1.2 + 2.5 * (1.0 + sin(4*u + phase));
        for (int y = top; y <= bottom; ++y) {
            double v = (double)(y-top) / (bottom-top+1);
            double band = sin((y - base) * 0.39 + u * 4.2 + sin(7*u + phase)*0.65);
            double fleck = unit(key + (unsigned)(dx+rx)*503u + (unsigned)y*367u);
            Color col = mix(face, 0x122e35, (float)clamp01(v * 0.86 + 0.15*u));
            if (band > 0.25 && v < 0.88)
                col = mix(col, 0x9a9e83, (float)(0.08 + 0.09*band));
            if (band < -0.89) col = mix(col, 0x14353b, 0.35f);
            if (y-top < ridge * height/40 && fleck > 0.23)
                col = mix(col, 0x8d9a56, (float)(0.20 + fleck * 0.42));
            else if (fleck > 0.85)
                col = mix(col, 0xa7aa91, 0.14f);
            px(c, x+dx, y, col);
        }
    }
}

/* A leaf has an asymmetric arch, a tapering tip, two planes of colour
   and a thin central vein. The root stays fixed while the current moves it. */
static void leaves(Canvas *c, int x, int y, int height, int count,
                   double spread, unsigned key, int layer, double seconds)
{
    static const Color shades[][3] = {
        {0x204b53,0x285a5c,0x346663},  /* distant leaves */
        {0x19514c,0x36755a,0x599263},  /* behind the wood */
        {0x163e39,0x377052,0x779956},  /* broad foreground leaves */
        {0x133638,0x295c48,0x4e8154}
    };
    for (int leaf = 0; leaf < count; ++leaf) {
        unsigned k = key + (unsigned)leaf * 17;
        double fan = (leaf + 0.35 + 0.28 * unit(k)) / count * 2.0 - 1.0;
        double tall = height * (0.55 + 0.43 * unit(k+1)) * (1 - 0.22*fabs(fan));
        double breadth = height * (layer == 2 ? 0.105 : 0.037) * (0.8+unit(k+2));
        double reach = height * spread * fan;
        double phase = unit(k+3) * 6.28;
        double tip = 0.08 * height * sin(seconds * 0.55 + phase);
        int root = x + (int)(fan * height * 0.13);
        Color dark = shades[layer][0];
        Color light = mix(shades[layer][1], shades[layer][2], (float)(unit(k+4)*0.56));
        for (int row = 0; row < (int)tall; ++row) {
            double t = (double)row / tall;
            double bend = reach*t*t + tip*t*t*t + sin(t*6 + phase)*height*0.035*t;
            int center = root + (int)bend;
            double fullness = 4*t*(1-t);
            if (layer == 2) fullness *= 2.1*t*t; /* a broad blade on a narrow petiole */
            else fullness *= 0.77 + t*0.23;
            int w = (int)(breadth * fullness);
            int vein = center + (int)(w * sin(phase+t*4.6) * 0.58);
            rect(c, center-w, y-row, vein-center+w+1, 1, light);
            rect(c, vein+1, y-row, center+w-vein, 1, dark);
            if (w >= 3 && row % 7 != 1)
                px(c, vein, y-row, shades[layer][2]);
            if (w >= 5 && layer == 2) {
                int rib = row % 8;
                if (rib < vein-center+w)
                    px(c, vein-rib, y-row, mix(light,shades[layer][2],0.35f));
                if (rib < center+w-vein)
                    px(c, vein+rib, y-row, mix(dark,light,0.34f));
            }
        }
    }
}

/* Joined cubic curves give the driftwood its knuckles and taper. Points and
   radii are in illustration pixels, relative to the base of the root. */
static void bough(Canvas *c, int x, int y, double s, const int p[8],
                  int r0, int r1, unsigned key)
{
    int steps = (int)(s * (abs(p[6]-p[0]) + abs(p[7]-p[1]) + 48)) + 1;
    for (int i = 0; i <= steps; ++i) {
        double t = (double)i/steps, a=1-t;
        double bx = a*a*a*p[0] + 3*a*a*t*p[2] + 3*a*t*t*p[4] + t*t*t*p[6];
        double by = a*a*a*p[1] + 3*a*a*t*p[3] + 3*a*t*t*p[5] + t*t*t*p[7];
        int cx = x + (int)(bx*s), cy = y + (int)(by*s);
        int r = (int)(s * (a*r0 + t*r1) * (0.97 + 0.10*cos(t*15)));
        /* Paint across the curve's normal so even upright limbs retain width. */
        double tx = a*a*(p[2]-p[0]) + 2*a*t*(p[4]-p[2]) + t*t*(p[6]-p[4]);
        double ty = a*a*(p[3]-p[1]) + 2*a*t*(p[5]-p[3]) + t*t*(p[7]-p[5]);
        double length = hypot(tx,ty);
        if (length < 0.01) continue;
        double nx = -ty/length, ny = tx/length;
        if (ny < 0) { nx = -nx; ny = -ny; }
        for (int offset = -r; offset <= r; ++offset) {
            double v = (double)(offset+r)/(2*r+1);
            Color bark = mix(0x977758, 0x302e32, (float)(0.1 + 0.84*v));
            double grain = cos(v*34 + sin(t*14)*1.55 + key);
            if (grain > 0.55) bark = mix(bark, 0x352f32, 0.58f);
            if (grain < -0.76 && v < 0.73) bark = mix(bark, 0xb09872, 0.36f);
            if (offset == -r && r > 2) bark = 0x9c8b64;
            rect(c, cx+(int)lround(nx*offset), cy+(int)lround(ny*offset), 2,1,bark);
        }
        if (r > 3 && noise(key+(unsigned)i)%9 < 5)
            px(c, cx-(int)(nx*r), cy-(int)(ny*r)-1, 0x76935d);
    }
}

static void driftwood(Canvas *c)
{
    /* The left root reaches above the rocks and forks into the open water. */
    static const int branches[][10] = {
        {-36,-4,  -6,-19, 25,-21, 60,-47, 12,8},
        { 58,-46, 75,-53, 81,-70, 100,-77, 8,4},
        { 98,-76,113,-87,126,-77, 153,-98, 4,0},
        { 79,-63,100,-63,113,-56, 135,-67, 5,0},
        { 41,-35, 37,-50, 48,-59,  43,-75, 6,1},
        { 43,-74, 44,-84, 37,-85,  42,-96, 1,0},
        { 15,-19, 43,-18, 58, -8,  82,-17, 7,0},
        { 67,-52, 73,-65, 65,-78,  76,-91, 4,0}
    };
    double scale = fmin(c->height / 320.0, c->width / 395.0);
    int x = (int)(c->width * 0.082), y = floor_y(c,x) + size_at(c,10);
    for (unsigned i = 0; i < sizeof(branches)/sizeof(branches[0]); ++i)
        bough(c, x,y,scale,branches[i],branches[i][8],branches[i][9], i*19);
}

static void paint_tank(Canvas *c, unsigned seed)
{
    water_and_sand(c, seed);
    int clumps = 4 + c->width * 9 / c->height;
    for (int i = 0; i < clumps; ++i) {
        unsigned k = seed + (unsigned)i * 31;
        int x = (int)((i + unit(k)*0.5) * c->width / clumps);
        double banks = 1.0 - sin(PI*x/c->width);
        /* Keep the distant sand channel open between the two planted banks. */
        if (x > c->width*0.44 && x < c->width*0.61) continue;
        int h = size_at(c, (30 + 52*unit(k+1)) * (0.63 + banks));
        leaves(c, x, floor_y(c,x)+1,h, 5 + (int)(unit(k+2)*5), 0.56,k,0,0);
    }
    /* Two unequal stone banks leave a sandy clearing through the middle. */
    for (int bank = 0; bank < 2; ++bank) {
        double edge = bank ? 1 : 0;
        int direction = bank ? -1 : 1;
        double extent = bank ? 0.31 : 0.42;
        for (int i = 0; i < 11; ++i) {
            unsigned k = seed + (unsigned)(i*61+bank*881);
            double distance = (i + unit(k)*0.42)/11.0;
            int x = (int)((edge + direction*extent*distance)*c->width);
            int h = size_at(c, (18 + 26*unit(k+1)) * (1.23 - distance*0.68));
            int rx = size_at(c, 19 + 21*unit(k+3));
            int y = floor_y(c,x) + size_at(c, 3 + 12*unit(k+2));
            stone(c,x,y,rx,h,k);
        }
    }
    int ledge = (int)(c->width * 0.155);
    stone(c,ledge,floor_y(c,ledge)+size_at(c,10),size_at(c,47),size_at(c,53),seed+907);
    driftwood(c);
    for (int bank = 0; bank < 2; ++bank) {
        for (int i = 0; i < 6; ++i) {
            unsigned k = seed+ (unsigned)(bank*943+i*191);
            double u = (i+unit(k)*0.3)*0.051;
            int x = (int)((bank ? 1-u : u)*c->width);
            int y = floor_y(c,x) + size_at(c, 14+9*unit(k+1));
            stone(c,x,y,size_at(c, 11+14*unit(k+2)),
                  size_at(c, (13+17*unit(k+3))*(1-u)), k);
        }
    }
}

/* Silt migrates slowly across the shafts; a few trapped bubbles escape
   from two fixed points and wobble as they rise. */
static void currents(Canvas *c, double time, unsigned seed)
{
    double scale = c->height/320.0;
    for (int i = 0; i < 52; ++i) {
        unsigned k = seed+(unsigned)i*71;
        double drift = (1.0+unit(k+1)*2.1)*scale;
        int x = (int)fmod(unit(k)*c->width+time*drift, c->width);
        double height = unit(k+2)*0.76+0.03;
        int y = (int)(height*c->height + scale*4*sin(time*0.15+i));
        Color col = mix(0x4d8a84, 0x76ada0, (float)(unit(k+4)*0.68));
        px(c,x,y,col);
    }
    for (int i = 0; i < 21; ++i) {
        unsigned k = seed + (unsigned)i*117;
        int source = (int)((i < 12 ? 0.227 : 0.851)*c->width);
        int base = floor_y(c,source)-size_at(c,7);
        if (base < 1) continue;
        double cycle = fmod(unit(k)*base + time * scale*(8.0+unit(k+1)*5.2), base+6);
        double phase = cycle/(base+6);
        int y = base-(int)cycle;
        int x = source + (int)(scale * ((unit(k+2)-0.5)*9.5 + phase*7.5 +
                 (1.4 + phase*1.3)*sin(cycle/(10*scale) + i)));
        int r = size_at(c, 0.50+1.85*unit(k+3)+phase*0.70);
        if (r <= 1) {
            px(c,x,y,0x7ab2ac);
            if (r == 1) px(c,x,y+1,0x4b888c);
        } else {
            /* Leave the centre transparent. */
            line(c,x-r+1,y-r,x+r-1,y-r,0x8cc3b9);
            line(c,x-r,y-r+1,x-r,y+r-1,0x5f9ca0);
            line(c,x+r,y-r+1,x+r,y+r-1,0x438089);
            line(c,x-r+1,y+r,x+r-1,y+r,0x69a6a7);
            px(c,x-r+1,y-r,0xc8e6d3);
        }
    }
}
/* Each letter is a palette entry in a hand-drawn sprite. A dot lets water
   show through. The source art faces right; swimming left mirrors it. */
static const char *const angelfish_rows[] = {
    ".........bee...........................",
    "..........bceee........................",
    "...........bccbee......................",
    "............bcdcceee...................",
    ".............ccdccdceeb................",
    "..............ccdccdcccb...............",
    "..............bccdccdcecbbb............",
    "...............ccdcccdcecbb............",
    "...............cccdccbcceabb...........",
    "...............bccdbbbbaggbbba.........",
    "...............bccdbccbbdbbbgcab.......",
    "b..............bcaabbcbeebcbhgga.......",
    "bdbb...........caacbcbbfbbcghhhga......",
    ".dfdb.........babccbcbgfbcbbhhhcca.....",
    ".bcffbb......bbcbdebbbffbcbfhhhhgha....",
    ".bcccffb....babcbgfbbbgfbcbffhhgaiha...",
    "..beeccffb.bacbcbffbbbffbcbfgfhiaihd...",
    "..bcceeccfdacdbcbffbbbffbcgffdfhichfa..",
    "..bcccceeacfebbbgffbbgffbbbffdffceeefga",
    "..bdddcccacdebbbbfbbbffbbbfffdfcceebca.",
    "..bdddeeeacdebbbffbabffbbbddfdeccedaaa.",
    "..beeeddeacccbbbefbabebbdfffdecceca....",
    "..bdddeedbbaccbbdebabedffbedfccccca....",
    "..bdeedbb....abbbdbabebabbeeedccc......",
    "..deddbb.....bacbcbafbbabbfdddcc.......",
    "..ddbb........baccbbabdabbdddca........",
    ".bdb..........bcaccbabbbabbccaa........",
    ".b............bccaabbabbabbaaab........",
    "..............bccccababbbbaabb.........",
    "..............bccccdbbbbbbcbb..........",
    ".............bbcccdcccceebbb...........",
    ".............bcccdccccecbbb............",
    ".............bcddccceecbb..............",
    "............bcdccceebbb................",
    "............cdcceebbb..................",
    "...........bcceebb.....................",
    "..........bcbbb........................",
    "..........bb...........................",
    ".........b.............................",
    ".......................................",
};

static const Color angelfish_palette[] = {
    0x182d43, /* a */
    0x354359, /* b */
    0x53647b, /* c */
    0x819399, /* d */
    0xadbfaf, /* e */
    0xd3d6ab, /* f */
    0xf5e1a5, /* g */
    0xd9954e, /* h */
    0xa15d44, /* i */
    0x773e39, /* j */
    0xfff2bf, /* k */
};

static const char *const discus_rows[] = {
    ".........................................",
    "........................aaaaa............",
    "......................aaaffffc...........",
    "..................aaaacffccjccca.........",
    ".................aacffjcccicacjca........",
    ".............aaaacffcccaaaacaaaaaa.......",
    ".............aacffjaaaccdddfffccaa.......",
    "............aaffccaaccdddeeeehffcaa......",
    "...........afjccaacdefeejjjjjhhdddca.....",
    ".........aafcccacddeehhjhhjjjjjhhdda.....",
    ".........afcccacddfjjjjhhjhheeeehedca....",
    "b.......afccacddehjhhjjjjhhhiheeeebda....",
    ".hbbb...fccacddehjhejhheiiiihheekfebca...",
    ".hiibbbaccacddhjjeejheeihhhhhhcebafbca...",
    "..hhiihbaacdejjhejjhehihhhjjjecbeabeda...",
    "..hhhhiiacdeehhjjhhiiihhhjhhhecebfbeeeaa.",
    "..bggghhacdehhhhhiieeeehjhhiieceeeefffdc.",
    "..bhhhggacdeiiiiihehjjjjeeihhcefeeeeeedca",
    "..biiiiiacdeeeehhhjjhhhehihhhcefeeeeddaa.",
    "..bhhhhiaacdeejjjjhhhiiiiiiiieefeeeedca..",
    "..bhhiihaaccdeeeeeehihhgiggghefeeeeeda...",
    "..hiihbbacaacdeeiiiihhiiiihihfeeeeedca...",
    ".bhhbbb..acaacdeeehhiiheeehieeeeeedda....",
    ".bhbb....aacaacddfiihhhhhiiheeeeeddca....",
    ".b........accaccddeeeehiiheeefddddca.....",
    "..........aaiaacccdddeeeheeedddddaa......",
    "...........accaaaccddddedfdddddcaaa......",
    "...........aacccaaccccdddddddcaaaa.......",
    "............accccaaaacccdccccaacaa.......",
    "............aaaeeccccaaacaaaaacaa........",
    "..............aaaeeecccccacceeaa.........",
    ".................aaaeeeeeeeeaaa..........",
    "....................aaaaaa...............",
    ".........................................",
};

static const Color discus_palette[] = {
    0x312d45, /* a */
    0x624156, /* b */
    0x934958, /* c */
    0xba5860, /* d */
    0xeb785d, /* e */
    0xf4b16a, /* f */
    0x405b60, /* g */
    0x458687, /* h */
    0x62b4a4, /* i */
    0xa1dcba, /* j */
    0xfae2a1, /* k */
};


static void feature_fish(Canvas *c, int species, int cx, int cy,
                         int direction, double phase, int scale)
{
    const int angel = species == 0;
    const char *const *rows = angel ? angelfish_rows : discus_rows;
    const Color *palette = angel ? angelfish_palette : discus_palette;
    const int row_count = angel ? (int)(sizeof angelfish_rows / sizeof *angelfish_rows)
                                : (int)(sizeof discus_rows / sizeof *discus_rows);
    const int middle_x = angel ? 19 : 20;
    const int middle_y = angel ? 19 : 17;
    const double tail_fold = 0.28 + 0.25 * sin(phase);

    /* The angelfish's two threadlike pelvic fins trail behind its belly. */
    if (angel) {
        for (int fin = 0; fin < 2; ++fin) {
            int root_x = cx + direction * (9 - 3 * fin) * scale;
            int bend_x = root_x - direction * (2 + fin) * scale;
            int bend_y = cy + (14 - fin) * scale;
            int tip_x = bend_x - direction * (3 + (int)(2 * sin(phase * 0.45 + fin))) * scale;
            Color thread = fin ? 0xc5b382 : 0x819798;
            line(c, root_x, cy + 5 * scale, bend_x, bend_y, thread);
            line(c, bend_x, bend_y, tip_x, cy + (21 - 2 * fin) * scale, thread);
        }
    }

    for (int y = 0; y < row_count; ++y) {
        const int top = cy + (y - middle_y) * scale;
        for (int x = 0; rows[y][x]; ++x) {
            char ink = rows[y][x];
            if (ink == '.') continue;

            /* Fold the fan toward its hinge. The body stays the same length. */
            int u = x < 9 ? x + (int)((9 - x) * tail_fold) : x;
            int left = cx + direction * (u - middle_x) * scale;
            rect(c, left, top, scale, scale, palette[ink - 'a']);
        }
    }

    /* The nearer pectoral fin gently rows over the flank. */
    int gill_x = cx + direction * 9 * scale;
    int gill_y = cy + (angel ? 1 : 2) * scale;
    int span = (int)((3.5 + 2.5 * sin(phase * 1.25)) * scale);
    int rise = (int)(1.5 * cos(phase * 1.25) * scale);
    line(c, gill_x, gill_y, gill_x - direction * span,
         gill_y + rise, angel ? 0xd3d6ab : 0x85bf9f);
}

/* A dozen neon tetras swim as a loose group. Each fish keeps its own
   offset, but follows the group's broad vertical turns a little later. */
static void tetra(Canvas *c, int x, int y, int dir, double beat, int distant)
{
    Color back = distant ? 0x4d7278 : 0x4a747a;
    Color blue = distant ? 0x64aaa8 : 0x66d4d1;
    Color belly = distant ? 0x6d7c74 : 0xc6caa1;
    Color red = distant ? 0x946d75 : 0xe06b67;
    int tail = x - dir * (sin(beat) > 0.2 ? 8 : 7);
    if (!distant) {
        /* Blue lateral stripe, red rear half and a silvery, fuller head. */
        int rear = x-5*dir, nose = x+6*dir;
        line(c,tail-dir,y-3,rear,y,0x66928e);
        line(c,tail-dir,y+3,rear,y,0x648581);
        line(c,rear,y-1,nose-dir,y-1,back);
        line(c,x-2*dir,y-2,x+3*dir,y-2,0x587e86);
        line(c,x-dir,y-3,x+dir,y-2,back);
        line(c,rear,y,nose,y,blue);
        line(c,x-dir,y,nose-dir*2,y,0x9bede1);
        line(c,rear,y+1,x,y+1,red);
        line(c,x-3*dir,y+2,x,y+2,0xb6535e);
        line(c,x+dir,y+1,nose,y+1,belly);
        line(c,x+dir,y+2,x+4*dir,y+2,0x97a89a);
        px(c,x+5*dir,y,0x234451);
        return;
    }
    line(c,tail,y-2,x-5*dir,y,back);
    line(c,tail,y+2,x-5*dir,y,back);
    line(c,x-4*dir,y-1,x+3*dir,y-1,back);
    line(c,x-5*dir,y,x+4*dir,y,blue);
    line(c,x-4*dir,y+1,x,y+1,red);
    line(c,x+dir,y+1,x+4*dir,y+1,belly);
    px(c,x+4*dir,y,0x1c3d4b);
    px(c,x+5*dir,y,belly);
    px(c,x-dir,y-2,back);
}

static void shoal(Canvas *c, double time, unsigned seed, int distant)
{
    double spacing = size_at(c, distant ? 10 : 14);
    double span = 7*spacing;
    double course = c->width + 2*span + 20;
    double speed = size_at(c, distant ? 6 : 12);
    int dir = distant ? -1 : 1;
    double progress = fmod(time*speed + c->width*(distant ? 0.41 : 0.63) + span, course) - span;
    double lead = dir > 0 ? progress : c->width - progress;
    int count = distant ? 9 : 13;
    for (int i = 0; i < count; ++i) {
        unsigned k = seed + (unsigned)i*83 + (unsigned)distant*307;
        int pair = i/2;
        double phase = i*1.31;
        double offset = spacing*(pair + 0.46*unit(k));
        double x = lead-dir*(offset + 2.5*sin(time*0.9+phase));
        if (x < -10 || x > c->width+10) continue;
        double vertical = ((i%2) ? 1:-1) * (2.0+13.0*unit(k+1));
        double y = c->height*(distant ? 0.62 : 0.28) +
                   size_at(c,15) * sin(time*0.13 - offset*0.016 + distant) +
                   size_at(c,7)*sin(time*0.31 + phase) + size_at(c,vertical);
        tetra(c, (int)x, (int)y, dir, time*5.1+phase, distant);
    }
}

/* The copper stems put small opposite leaves along a flexible stalk, giving
   the planted banks a different silhouette from the wide ribbon plants. */
static void stem_plant(Canvas *c, int x, int y, int height, int lean,
                       double seconds, unsigned key)
{
    int last_x = x, last_y = y;
    double bend = 0.12*sin(seconds*0.42+unit(key)*6);
    for (int node = 1; node <= 11; ++node) {
        double t = node/11.0;
        int cx = x + (int)((lean + height*bend)*t*t);
        int cy = y - (int)(height*t);
        line(c,last_x,last_y,cx,cy,0x674e43);
        for (int side = -1; side <= 1; side+=2) {
            double reach = height * (0.11 + 0.06*sin(PI*t))*(1-0.75*t*t);
            int tx = cx + side*(int)(reach*(0.74+unit(key+(unsigned)(node*3+side))*0.44));
            int ty = cy-(int)(reach*0.63);
            int mid_x = (cx+tx)/2, mid_y = (cy+ty)/2;
            int w = size_at(c,2.2*(1-t)+1);
            for (int j = -w; j <= w; ++j) {
                Color color = j <= 0 ? mix(0x716646,0xac7850,(float)t) : 0x655041;
                line(c,cx,cy,mid_x,mid_y+j,color);
                line(c,mid_x,mid_y+j,tx,ty,color);
            }
        }
        last_x = cx;
        last_y = cy;
    }
}

static void swimming_fish(Canvas *c, double time, unsigned seed)
{
    /* Feature fish patrol different depths. Entire sprites pass the edge
       before the next circuit starts, including the long pelvic fins. */
    const double start[] = {0.67, 0.24, 0.66};
    const double depth[] = {0.22, 0.48, 0.57};
    const double speed[] = {5.5, 8.0, 6.1};
    const int heading[] = {-1, 1, -1};
    for (int i = 0; i < 3; ++i) {
        int dir = heading[i];
        double phase = unit(seed + (unsigned)i*67) * 6.28;
        double swim = time*speed[i] + 9*sin(time*0.31+phase);
        double route = c->width + size_at(c,104);
        double origin = (dir > 0 ? start[i] : 1-start[i]) * c->width;
        double progress = fmod(origin + size_at(c,52) + swim*c->height/320.0, route);
        int x = (int)progress-size_at(c,52);
        if (dir < 0) x = c->width-x;
        int y = (int)(depth[i]*c->height) +
                size_at(c,9*sin(time*0.29+phase) + 8*sin(time*0.16+2*i));
        int scale = i == 0 || c->height < 240 || c->width < 280 ? 1 : 2;
        feature_fish(c, i == 2, x,y,dir, time*(2.8+i*0.35)+phase,scale);
    }
}

/* A small panda cory stops periodically to sift the sand. Its sweep follows
   the floor contour, with each turn hidden behind a bank of plants. */
static void bottom_feeder(Canvas *c, double time)
{
    static const char *const rows[] = {
        "..........bb............",
        "..........bcb...........",
        "..........bccb..........",
        "b........bbcbbbb........",
        "bc.....bbbcdeeeebb......",
        ".bcb.bbbcceedeeccebb....",
        "..cbbcebbdddeedcbceebb..",
        "..bcbeebbceeeedbbabddeb.",
        ".bcb..beeccddddebbededdc",
        ".bb....bbddddddddddecccb",
        "........bcbddddbbbbbb...",
        "........bb.bccb........."
    };
    static const Color palette[] = {
        0x172f39,0x38464a,0x6a756b,0xbead83,0xd8c69c
    };
    double leg = fmod(time+13, 110.0);
    int dir = leg < 55.0 ? 1 : -1;
    double t = fmod(leg, 55.0)/55.0;
    /* The slow and fast portions mimic little bursts between mouthfuls. */
    double progress = t - sin(14*PI*t)/(14*PI);
    int x = (int)((dir > 0 ? progress : 1-progress)*(c->width+70))-35;
    int y = floor_y(c,x) + size_at(c,21);
    int nudge = (int)lround(sin(2.8*time));
    for (int row = 0; row < (int)(sizeof(rows)/sizeof(rows[0])); ++row) {
        for (int col = 0; rows[row][col]; ++col) {
            char ink = rows[row][col];
            if (ink == '.') continue;
            int swim = col < 5 && nudge > 0 ? 1 : 0;
            px(c,x+dir*(col-11+swim),y+row-10,palette[ink-'a']);
        }
    }
    line(c,x+dir*11,y-1,x+dir*14,y+1+nudge,0xccbb93);
    line(c,x+dir*10,y,x+dir*12,y+2,0x8a8d75);
}

static void surface_glints(Canvas *c, double time, unsigned seed)
{
    for (int i = 0; i < 77; ++i) {
        unsigned k = seed + (unsigned)i*27;
        double phase = unit(k) * 6.28;
        int x = (int)(unit(k+1)*c->width + size_at(c, 4*sin(time*0.39+phase)));
        int y = size_at(c, 1 + pow(unit(k+2), 2.4)*10 + sin(time*0.45+phase));
        int width = size_at(c, 3 + 13*unit(k+3)*(0.7+0.3*cos(time*0.5+phase)));
        Color col = mix(0x447f84,0x86bbb0,(float)(unit(k+4)*0.58));
        rect(c,x,y,width,1,col);
    }
}

void scene_draw(Canvas *c, double seconds, unsigned seed)
{
    static Canvas background;
    static unsigned saved_seed;
    size_t bytes = (size_t)c->width*c->height*sizeof(Color);
    if (background.width != c->width || background.height != c->height ||
        saved_seed != seed || !background.pixels) {
        Color *pixels = realloc(background.pixels, bytes);
        if (!pixels) {
            paint_tank(c,seed);
        } else {
            background = (Canvas){c->width,c->height,pixels};
            saved_seed = seed;
            paint_tank(&background,seed);
        }
    }
    if (background.pixels && background.width == c->width &&
        background.height == c->height && saved_seed == seed)
        memcpy(c->pixels,background.pixels,bytes);

    surface_glints(c,seconds,seed);
    currents(c,seconds,seed);
    shoal(c,seconds,seed,1);

    int left = (int)(0.16*c->width), right = (int)(0.92*c->width);
    leaves(c,left,floor_y(c,left)+size_at(c,8),size_at(c,94),9,0.48,seed+7,1,seconds);
    leaves(c,right,floor_y(c,right)+size_at(c,8),size_at(c,114),12,0.46,seed+45,1,seconds);
    for (int i = 0; i < 6; ++i) {
        int root = (int)((0.77+0.017*i)*c->width);
        stem_plant(c,root,floor_y(c,root)+size_at(c,9),size_at(c,39+16*sin(i)+7*i),
                   size_at(c,(i-3)*8),seconds,seed+(unsigned)i*19);
    }
    shoal(c,seconds,seed,0);
    swimming_fish(c,seconds,seed);
    bottom_feeder(c,seconds);

    /* Closer leaves occasionally conceal part of a passing fish. */
    leaves(c,size_at(c,5),c->height+size_at(c,5), size_at(c,132),14,0.54,seed+85,3,seconds);
    int left_mid = (int)(0.075*c->width);
    leaves(c,left_mid,floor_y(c,left_mid)+size_at(c,24),size_at(c,83),9,0.67,seed+27,2,seconds);
    int mid = (int)(0.30*c->width);
    leaves(c,mid,floor_y(c,mid)+size_at(c,16), size_at(c,38),7,0.65,seed+61,2,seconds);
    int right_mid = (int)(0.86*c->width);
    leaves(c,right_mid,floor_y(c,right_mid)+size_at(c,23),size_at(c,57),7,0.77,seed+53,2,seconds);
    leaves(c,c->width-size_at(c,6),c->height+size_at(c,6),size_at(c,105),12,0.6,seed+31,3,seconds);
}
