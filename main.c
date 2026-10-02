// MIT License

// Copyright (c) 2026 nexus7super-ship-it, Tony Valentin Barrero

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
// when it crashed and gold when it filled the whole board. The score is shown
// below the board.

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#ifdef TINY
// The tiny build has no C runtime, so it sleeps and exits with system calls.
#include <sys/syscall.h>
static long syscall3(long number, long a, long b, long c)
{
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return result;
}
#define sleep_ms(ms) syscall3(SYS_poll, 0, 0, ms)
#else
#define sleep_ms(ms) poll(NULL, 0, ms)
#endif

// Keeping these small functions out of line makes the code smaller.
#define NOINLINE __attribute__((noinline))

#define CELLS 20             // the board has CELLS x CELLS cells
#define CELL  20             // size of one cell in pixels
#define SIZE  (CELLS * CELL) // width and height of the board in pixels
#define BAR   30             // height of the score bar below the board
#define START 3              // initial length of the snake

// Keysyms from <X11/keysymdef.h>. Left, Up, Right and Down are consecutive.
#define XK_Escape 0xff1b
#define XK_Left   0xff51

enum { RUNNING, PAUSED, DEAD, WON };

// The snake color depends on the game state, the rest has fixed colors.
enum { FOOD = WON + 1, SCORE, BLACK, COLORS };
static const uint32_t colors[COLORS] = {
    [RUNNING] = 0x00ff00,
    [PAUSED]  = 0x008000,
    [DEAD]    = 0x808080,
    [WON]     = 0xffd700,
    [FOOD]    = 0xff0000,
    [SCORE]   = 0xc0c0c0,
    [BLACK]   = 0x000000,
};

// The digits 0-9 in a 3 x 5 pixel font: one bit per pixel, row by row,
// starting with the top left pixel in bit 14.
static const uint16_t digits[10] = {
    0x7b6f, 0x2c97, 0x73e7, 0x73cf, 0x5bc9,
    0x79cf, 0x79ef, 0x7249, 0x7bef, 0x7bcf,
};
#define PIXEL 4 // size of one font pixel on the screen

typedef struct { signed char x, y; } Point;

static xcb_connection_t *conn;
static xcb_window_t win;
static xcb_gcontext_t gcs[COLORS]; // one graphics context for each color

static Point snake[CELLS * CELLS]; // snake[0] is the head
static int len, state;
static Point dir;   // direction chosen by the player
static Point moved; // direction of the last step that was actually made
static Point food;
static uint32_t seed;

static NOINLINE int random_cell(void)
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

static void fill(int color, int n, const xcb_rectangle_t *rects)
{
    xcb_poly_fill_rectangle(conn, win, gcs[color], n, rects);
}

// Paints an area black, like xcb_clear_area() but without importing it.
static NOINLINE void clear(int x, int y, int width, int height)
{
    fill(BLACK, 1, &(xcb_rectangle_t){ x, y, width, height });
}

static void reset(void)
{
    len = START;
    for (int i = 0; i < len; i++)
        snake[i] = (Point){ CELLS / 2, CELLS / 2 + i };
    dir = moved = (Point){ 0, -1 };
    state = RUNNING;
    place_food();
    clear(0, 0, SIZE, SIZE + BAR);
}

static NOINLINE void step(void)
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
        clear(snake[len - 1].x * CELL, snake[len - 1].y * CELL, CELL, CELL);
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

static NOINLINE xcb_rectangle_t cell_rect(Point p)
{
    return (xcb_rectangle_t){ p.x * CELL, p.y * CELL, CELL - 1, CELL - 1 };
}

// Draws the line that marks the bottom wall and the score as 3 digits in the
// bar below it. The bar is only cleared when the score changes, so it doesn't
// flicker.
static void draw_score(void)
{
    static int shown; // reset() clears the bar too, so 0 is a safe start
    static xcb_rectangle_t rects[1 + 3 * 15]; // static: makes the code smaller
    int n = 0, score = len - START;

    if (score != shown) {
        clear(0, SIZE + 1, SIZE, BAR - 1);
        shown = score;
    }
    rects[n++] = (xcb_rectangle_t){ 0, SIZE, SIZE, 1 };
    for (int i = 0; i < 3; i++, score /= 10) { // from right to left
        int x = 46 - i * 18;
        for (int b = 0; b < 15; b++)
            if (digits[score % 10] >> (14 - b) & 1)
                rects[n++] = (xcb_rectangle_t){ x + b % 3 * PIXEL,
                                                SIZE + 5 + b / 3 * PIXEL,
                                                PIXEL, PIXEL };
    }
    fill(SCORE, n, rects);
}

// Only vacated cells are cleared (in step and reset), so nothing flickers.
static void draw(void)
{
    xcb_rectangle_t rects[CELLS * CELLS];

    rects[0] = cell_rect(food);
    fill(FOOD, 1, rects);
    for (int i = 0; i < len; i++)
        rects[i] = cell_rect(snake[i]);
    fill(state, len, rects);
    draw_score();
    xcb_flush(conn);
}

int main(void)
{
    // Local copies of conn and win stay in registers, which makes the many
    // calls below shorter.
    xcb_connection_t *connection = conn = xcb_connect(NULL, NULL);
    const xcb_setup_t *setup = xcb_get_setup(connection);
    // The first screen follows the vendor string (padded to 4 bytes) and the
    // pixmap formats. This is what xcb_setup_roots_iterator() computes.
    xcb_screen_t *screen =
        (xcb_screen_t *)((char *)(setup + 1) + ((setup->vendor_len + 3) & ~3) +
                         setup->pixmap_formats_len * sizeof(xcb_format_t));

    // xcb_generate_id() hands out the resource IDs base | inc, base | 2 * inc
    // and so on. TinySnake numbers its window and graphics contexts the same
    // way and never calls it.
    uint32_t inc = setup->resource_id_mask & -setup->resource_id_mask;
    xcb_window_t window = win = setup->resource_id_base | inc;

    // Keys are recognized by keysym, because keycodes depend on the hardware
    // and the X server.
    xcb_keycode_t min_keycode = setup->min_keycode;
    // xcb_wait_for_reply() is what all the xcb_*_reply() functions call. Using
    // it directly saves importing them.
    xcb_get_keyboard_mapping_reply_t *keymap = xcb_wait_for_reply(
        connection,
        xcb_get_keyboard_mapping(connection, min_keycode,
                                 setup->max_keycode - min_keycode + 1)
            .sequence,
        NULL);
    if (!keymap) // also when there is no X server: then all requests fail
        return 1;
    // The keysyms directly follow the reply. This is exactly what
    // xcb_get_keyboard_mapping_keysyms() returns, minus one import.
    xcb_keysym_t *keysyms = (xcb_keysym_t *)(keymap + 1);

    uint32_t values[] = { screen->black_pixel, XCB_EVENT_MASK_KEY_PRESS };
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root,
                      0, 0, SIZE, SIZE + BAR, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 9, "TinySnake");

    // WM_NORMAL_HINTS with the xcb_size_hints_t layout from ICCCM: flags,
    // x, y, width, height, min_width, min_height, max_width, max_height, ...
    // Setting the min and max size to the same value makes the window fixed.
    static uint32_t hints[18];        // static: all 0, and smaller code
    hints[0] = 1 << 4 | 1 << 5;       // PMinSize | PMaxSize
    hints[5] = hints[7] = SIZE;       // min_width, max_width
    hints[6] = hints[8] = SIZE + BAR; // min_height, max_height
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
                        XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32,
                        18, hints);

    // Let the X server translate the colors, so they are also right on
    // displays that aren't 24-bit TrueColor. Each color gets its own
    // graphics context.
    for (int i = 0; i < COLORS; i++) {
        uint32_t c = colors[i];
        xcb_alloc_color_reply_t *color = xcb_wait_for_reply(
            connection,
            xcb_alloc_color(connection, screen->default_colormap,
                            (c >> 16 & 0xff) * 0x101, (c >> 8 & 0xff) * 0x101,
                            (c & 0xff) * 0x101)
                .sequence,
            NULL);
        uint32_t pixel = color ? color->pixel : screen->white_pixel;
        free(color);
        gcs[i] = window + (i + 1) * inc;
        xcb_create_gc(connection, gcs[i], window, XCB_GC_FOREGROUND, &pixel);
    }

    seed = (uintptr_t)&setup; // a stack address, randomized by ASLR
    reset();
    xcb_map_window(connection, window);

    for (;;) {
        xcb_generic_event_t *event;
        while ((event = xcb_poll_for_event(connection))) {
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
        // The window was closed or the X server went away. The error state is
        // the first field of every connection; libxcb relies on that itself,
        // and it's all that xcb_connection_has_error() returns.
        if (*(int *)connection)
            return 1;

        if (state == RUNNING)
            step();
        draw();

        // A plain sleep: key presses are handled once per tick, so they
        // can't speed the game up. The snake gets faster as it grows.
        int delay = 120 - len / 2;
        sleep_ms(delay < 50 ? 50 : delay);
    }
}

#ifdef TINY
// Entry point of the tiny build, which leaves out the C runtime startup code.
// The dynamic loader initializes glibc on its own, as libxcb needs it.
__attribute__((force_align_arg_pointer, noreturn)) void _start(void)
{
    syscall3(SYS_exit_group, main(), 0, 0);
    __builtin_unreachable();
}
#endif
