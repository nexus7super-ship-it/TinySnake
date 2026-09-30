// MIT License

// Copyright (c) 2026 nexus7super-ship-it

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// TinySnake - a tiny Snake game for X11 (also runs on Wayland via XWayland).
//
// Controls: arrow keys steer, P pauses, R restarts, Space restarts after the
// game has ended, Esc quits. The snake turns dark green while paused, gray
// when it crashed and gold when it filled the whole board.

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#define CELLS 20             // the board has CELLS x CELLS cells
#define CELL  20             // size of one cell in pixels
#define SIZE  (CELLS * CELL) // window width and height in pixels
#define START 3              // initial length of the snake

// Keysyms from <X11/keysymdef.h>. Left, Up, Right and Down are consecutive.
#define XK_Escape 0xff1b
#define XK_Left   0xff51

enum { RUNNING, PAUSED, DEAD, WON };

// The snake color depends on the game state, the food has its own color.
#define FOOD (WON + 1)
static const uint32_t colors[] = {
    [RUNNING] = 0x00ff00,
    [PAUSED]  = 0x008000,
    [DEAD]    = 0x808080,
    [WON]     = 0xffd700,
    [FOOD]    = 0xff0000,
};

typedef struct { int x, y; } Point;

static xcb_connection_t *conn;
static xcb_window_t win;
static xcb_gcontext_t gc;
static uint32_t pixels[FOOD + 1]; // colors as pixel values of the display

static Point snake[CELLS * CELLS]; // snake[0] is the head
static int len, state;
static Point dir;   // direction chosen by the player
static Point moved; // direction of the last step that was actually made
static Point food;
static uint32_t seed;

static int random_cell(void)
{
    seed = seed * 1103515245 + 12345;
    return (uint64_t)seed * CELLS >> 32; // the high bits are the most random
}

// Is p one of the first n segments of the snake?
static int on_snake(Point p, int n)
{
    for (int i = 0; i < n; i++)
        if (snake[i].x == p.x && snake[i].y == p.y)
            return 1;
    return 0;
}

static void place_food(void)
{
    do
        food = (Point){ random_cell(), random_cell() };
    while (on_snake(food, len));
}

static void clear_cell(Point p)
{
    xcb_clear_area(conn, 0, win, p.x * CELL, p.y * CELL, CELL, CELL);
}

static void reset(void)
{
    len = START;
    for (int i = 0; i < len; i++)
        snake[i] = (Point){ CELLS / 2, CELLS / 2 + i };
    dir = moved = (Point){ 0, -1 };
    state = RUNNING;
    place_food();
    xcb_clear_area(conn, 0, win, 0, 0, 0, 0); // 0 x 0 clears the whole window
}

static void step(void)
{
    Point head = { snake[0].x + dir.x, snake[0].y + dir.y };
    moved = dir;

    // The tail moves on in this step, so running into it is allowed. When the
    // snake eats, the head lands on the food, which never lies on the snake.
    if ((unsigned)head.x >= CELLS || (unsigned)head.y >= CELLS ||
        on_snake(head, len - 1)) {
        state = DEAD;
        return;
    }

    int ate = head.x == food.x && head.y == food.y;
    if (ate)
        len++;
    else
        clear_cell(snake[len - 1]);
    for (int i = len - 1; i > 0; i--)
        snake[i] = snake[i - 1];
    snake[0] = head;

    if (ate) {
        if (len == CELLS * CELLS)
            state = WON;
        else
            place_food();
    }
}

// Returns 0 when the game should quit.
static int handle_key(xcb_keysym_t sym)
{
    static const Point dirs[] = { { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 } };
    unsigned d = sym - XK_Left;

    if (sym == XK_Escape)
        return 0;
    if (sym == 'p' && state <= PAUSED)
        state ^= 1; // RUNNING <-> PAUSED
    if (sym == 'r' || (sym == ' ' && state >= DEAD))
        reset();
    // Compare with the last real step, so that several quick key presses
    // within one tick can't turn the snake back into its own neck.
    if (d < 4 && state == RUNNING &&
        (dirs[d].x != -moved.x || dirs[d].y != -moved.y))
        dir = dirs[d];
    return 1;
}

static void fill(uint32_t color, int n, const xcb_rectangle_t *rects)
{
    xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &color);
    xcb_poly_fill_rectangle(conn, win, gc, n, rects);
}

static xcb_rectangle_t cell_rect(Point p)
{
    return (xcb_rectangle_t){ p.x * CELL, p.y * CELL, CELL - 1, CELL - 1 };
}

// Only vacated cells are cleared (in step and reset), so nothing flickers.
static void draw(void)
{
    xcb_rectangle_t rects[CELLS * CELLS];

    rects[0] = cell_rect(food);
    fill(pixels[FOOD], 1, rects);
    for (int i = 0; i < len; i++)
        rects[i] = cell_rect(snake[i]);
    fill(pixels[state], len, rects);
    xcb_flush(conn);
}

int main(void)
{
    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn))
        return 1;
    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;

    // Keys are recognized by keysym, because keycodes depend on the hardware
    // and the X server.
    xcb_keycode_t min_keycode = setup->min_keycode;
    xcb_get_keyboard_mapping_reply_t *keymap = xcb_get_keyboard_mapping_reply(
        conn,
        xcb_get_keyboard_mapping(conn, min_keycode,
                                 setup->max_keycode - min_keycode + 1),
        NULL);
    if (!keymap)
        return 1;
    // The keysyms directly follow the reply. This is exactly what
    // xcb_get_keyboard_mapping_keysyms() returns, minus one import.
    xcb_keysym_t *keysyms = (xcb_keysym_t *)(keymap + 1);

    win = xcb_generate_id(conn);
    uint32_t values[] = { screen->black_pixel, XCB_EVENT_MASK_KEY_PRESS };
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root, 0, 0,
                      SIZE, SIZE, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING, 8, 9, "TinySnake");

    // WM_NORMAL_HINTS with the xcb_size_hints_t layout from ICCCM: flags,
    // x, y, width, height, min_width, min_height, max_width, max_height, ...
    // Setting the min and max size to the same value makes the window fixed.
    uint32_t hints[18] = { [0] = 1 << 4 | 1 << 5, // PMinSize | PMaxSize
                           [5] = SIZE, SIZE, SIZE, SIZE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
                        XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32,
                        18, hints);

    gc = xcb_generate_id(conn);
    xcb_create_gc(conn, gc, win, 0, NULL);

    // Let the X server translate the colors, so they are also right on
    // displays that aren't 24-bit TrueColor.
    for (int i = 0; i <= FOOD; i++) {
        uint32_t c = colors[i];
        xcb_alloc_color_reply_t *color = xcb_alloc_color_reply(
            conn,
            xcb_alloc_color(conn, screen->default_colormap,
                            (c >> 16 & 0xff) * 0x101, (c >> 8 & 0xff) * 0x101,
                            (c & 0xff) * 0x101),
            NULL);
        pixels[i] = color ? color->pixel : screen->white_pixel;
        free(color);
    }

    seed = (uintptr_t)&setup; // a stack address, randomized by ASLR
    reset();
    xcb_map_window(conn, win);

    for (;;) {
        xcb_generic_event_t *event;
        while ((event = xcb_poll_for_event(conn))) {
            if ((event->response_type & 0x7f) == XCB_KEY_PRESS) {
                xcb_key_press_event_t *key = (xcb_key_press_event_t *)event;
                seed += key->time;
                int index = (key->detail - min_keycode) *
                            keymap->keysyms_per_keycode;
                if (!handle_key(keysyms[index]))
                    return 0;
            }
            free(event);
        }
        // The window was closed or the X server went away.
        if (xcb_connection_has_error(conn))
            return 1;

        if (state == RUNNING)
            step();
        draw();

        // A plain sleep: key presses are handled once per tick, so they
        // can't speed the game up. The snake gets faster as it grows.
        int delay = 120 - len / 2;
        poll(NULL, 0, delay < 50 ? 50 : delay);
    }
}

#ifdef NOSTARTFILES
// Entry point for the tiny build, which links with -nostartfiles to leave out
// the C runtime startup code. The dynamic loader initializes glibc on its own.
__attribute__((force_align_arg_pointer, noreturn)) void _start(void)
{
    exit(main());
}
#endif
