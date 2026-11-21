/* The Windows half of both savers. The scene only sees a pixel canvas and a clock. */
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctype.h>
#include <stdlib.h>

#include "resource.h"
#include "scene.h"

enum { FRAME_HEIGHT = 320, FRAME_MS = 33, TIMER_ID = 1 };
static const char window_class[] = "RetroCollectionSaver";
static const char settings_key[] = "Software\\RetroScreensavers";
static const double speeds[] = {0.55, 1.0, 1.50};
static char scene_name[128];
static HWND preview_parent;
static Canvas frame;
static BITMAPINFO bitmap;
static ULONGLONG start_time;
static double speed;
static unsigned seed;
static POINT first_mouse;
static BOOL have_first_mouse;

static DWORD load_speed(void) {
    DWORD choice = 1, size = sizeof(choice);
    if (RegGetValueA(HKEY_CURRENT_USER, settings_key, scene_name, RRF_RT_REG_DWORD,
                     NULL, &choice, &size) != ERROR_SUCCESS || choice > 2)
        return 1;
    return choice;
}

static INT_PTR CALLBACK settings_proc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (message == WM_INITDIALOG) {
        HWND combo = GetDlgItem(dialog, IDC_SPEED);
        SetWindowTextA(dialog, scene_name);
        SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)"Slow");
        SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)"Normal");
        SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)"Brisk");
        SendMessageA(combo, CB_SETCURSEL, load_speed(), 0);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wp) == IDOK) {
        DWORD choice = (DWORD)SendDlgItemMessage(dialog, IDC_SPEED, CB_GETCURSEL, 0, 0);
        if (choice > 2) return TRUE;
        if (RegSetKeyValueA(HKEY_CURRENT_USER, settings_key, scene_name, REG_DWORD,
                            &choice, sizeof(choice)) != ERROR_SUCCESS) {
            MessageBoxA(dialog, "Windows could not save the animation speed.",
                        scene_name, MB_OK | MB_ICONWARNING);
            return TRUE;
        }
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wp) == IDCANCEL)) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static int view_width(const RECT *bounds) {
    int width = bounds->right - bounds->left;
    int height = bounds->bottom - bounds->top;
    return height > 0 ? MulDiv(width, FRAME_HEIGHT, height) : 0;
}

static BOOL CALLBACK measure_monitor(HMONITOR monitor, HDC dc, LPRECT bounds, LPARAM data) {
    (void)monitor;
    (void)dc;
    int *widest = (int *)data;
    int width = view_width(bounds);
    if (width > *widest) *widest = width;
    return TRUE;
}

/* Render once per tick. Narrower monitors show a centred crop of the same frame. */
static BOOL fit_canvas(HWND window) {
    RECT bounds;
    int width = 0;
    if (!preview_parent)
        EnumDisplayMonitors(NULL, NULL, measure_monitor, (LPARAM)&width);
    if (width == 0) {
        GetClientRect(window, &bounds);
        width = view_width(&bounds);
    }
    if (width < 1) width = 1;
    if (width > 2048) width = 2048;
    if (width == frame.width) return TRUE;

    Color *pixels = calloc((size_t)width * FRAME_HEIGHT, sizeof(*pixels));
    if (!pixels) return FALSE;
    free(frame.pixels);
    frame = (Canvas){width, FRAME_HEIGHT, pixels};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = width;
    bitmap.bmiHeader.biHeight = -FRAME_HEIGHT; /* First row is the top of the image. */
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    return TRUE;
}

static void render_frame(void) {
    double seconds = (double)(GetTickCount64() - start_time) * 0.001;
    scene_draw(&frame, seconds * speed, seed);
}

static void draw_view(HDC dc, RECT bounds) {
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    int source_width = view_width(&bounds);
    if (!frame.pixels || width < 1 || height < 1 || source_width < 1) return;
    if (source_width > frame.width) {
        /* Preserve square pixels even on a display wider than the canvas limit. */
        int scaled_width = MulDiv(height, frame.width, FRAME_HEIGHT);
        FillRect(dc, &bounds, (HBRUSH)GetStockObject(BLACK_BRUSH));
        bounds.left += (width - scaled_width) / 2;
        width = scaled_width;
        source_width = frame.width;
    }
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, bounds.left, bounds.top, width, height,
                  (frame.width - source_width) / 2, 0, source_width, FRAME_HEIGHT,
                  frame.pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);
}

typedef struct { HWND window; HDC dc; } MonitorPaint;

static BOOL CALLBACK paint_monitor(HMONITOR monitor, HDC unused, LPRECT bounds, LPARAM data) {
    (void)monitor;
    (void)unused;
    MonitorPaint *paint = (MonitorPaint *)data;
    RECT local = *bounds;
    MapWindowPoints(NULL, paint->window, (POINT *)&local, 2);
    draw_view(paint->dc, local);
    return TRUE;
}

/* A preview lives inside another process's window. Track that parent's client area. */
static void resize_to_parent(HWND window) {
    RECT desired, current;
    if (!GetClientRect(preview_parent, &desired)) {
        PostMessage(window, WM_CLOSE, 0, 0);
        return;
    }
    GetClientRect(window, &current);
    if (desired.right != current.right || desired.bottom != current.bottom)
        SetWindowPos(window, NULL, 0, 0, desired.right, desired.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
}

static LRESULT CALLBACK saver_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE:
        if (!SetTimer(window, TIMER_ID, FRAME_MS, NULL)) return -1;
        return 0;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            if (!fit_canvas(window)) {
                PostMessage(window, WM_CLOSE, 0, 0);
                return 0;
            }
            render_frame();
            InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_ID) {
            if (preview_parent) resize_to_parent(window);
            if (frame.pixels && IsWindowVisible(window)) {
                render_frame();
                InvalidateRect(window, NULL, FALSE);
            }
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);
        MonitorPaint target = {window, dc};
        if (preview_parent || !EnumDisplayMonitors(NULL, NULL, paint_monitor, (LPARAM)&target))
            draw_view(dc, client);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_SETCURSOR:
        if (!preview_parent) {
            SetCursor(NULL);
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
        if (!preview_parent) {
            POINT mouse;
            if (GetCursorPos(&mouse)) {
                if (have_first_mouse && (labs(mouse.x - first_mouse.x) > 6 ||
                                         labs(mouse.y - first_mouse.y) > 6))
                    DestroyWindow(window);
                if (!have_first_mouse) { first_mouse = mouse; have_first_mouse = TRUE; }
            }
        }
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (!preview_parent) DestroyWindow(window);
        return 0;
    case WM_DISPLAYCHANGE:
        /* Let Windows start the next full-screen run with the new desktop geometry. */
        if (!preview_parent) PostMessage(window, WM_CLOSE, 0, 0);
        return 0;
    case WM_DESTROY:
        KillTimer(window, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wp, lp);
}

static HWND argument_window(const char *arg) {
    while (*arg == ':' || isspace((unsigned char)*arg)) ++arg;
    if (!isdigit((unsigned char)*arg)) return NULL;
    char *end;
    UINT_PTR value = (UINT_PTR)strtoull(arg, &end, 10);
    while (isspace((unsigned char)*end)) ++end;
    return *end == '\0' && IsWindow((HWND)value) ? (HWND)value : NULL;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR arg, int show) {
    (void)previous;
    (void)show;
    SetProcessDPIAware();
    if (!LoadStringA(instance, IDS_SCENE_NAME, scene_name, sizeof(scene_name))) return 1;
    while (isspace((unsigned char)*arg)) ++arg;
    if (*arg == '/' || *arg == '-') ++arg;
    int mode = tolower((unsigned char)*arg);
    if (mode == 'c' || mode == '\0')
        return DialogBoxParamA(instance, MAKEINTRESOURCEA(IDD_SETTINGS),
                               mode ? argument_window(arg + 1) : NULL,
                               settings_proc, 0) == -1;
    if (mode != 's' && mode != 'p') return 0;
    if (mode == 'p' && !(preview_parent = argument_window(arg + 1))) return 0;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = saver_proc;
    wc.hInstance = instance;
    wc.lpszClassName = window_class;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!RegisterClassA(&wc)) return 1;
    start_time = GetTickCount64();
    seed = (unsigned)start_time ^ (unsigned)GetCurrentProcessId();
    speed = speeds[load_speed()];
    have_first_mouse = GetCursorPos(&first_mouse);

    int x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (preview_parent) {
        RECT bounds;
        if (!GetClientRect(preview_parent, &bounds)) return 1;
        x = y = 0;
        width = bounds.right;
        height = bounds.bottom;
    }
    HWND window = CreateWindowExA(preview_parent ? 0 : WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        window_class, scene_name, preview_parent ? WS_CHILD : WS_POPUP,
        x, y, width, height, preview_parent, NULL, instance, NULL);
    if (!window) return 1;
    ShowWindow(window, preview_parent ? SW_SHOWNOACTIVATE : SW_SHOW);
    if (!preview_parent) SetForegroundWindow(window);
    UpdateWindow(window);
    MSG message;
    BOOL result;
    while ((result = GetMessageA(&message, NULL, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    free(frame.pixels);
    return result == -1;
}
