// Unit tests for the game logic in main.c, without an X server: the XCB
// functions that the logic and the drawing code call are replaced by stubs.
//
// Run with: make test

#include <stdio.h>
#include <string.h>

#define main game_main
#include "../main.c"
#undef main

xcb_void_cookie_t xcb_clear_area(xcb_connection_t *c, uint8_t exposures,
                                 xcb_window_t w, int16_t x, int16_t y,
                                 uint16_t width, uint16_t height)
{
    (void)c, (void)exposures, (void)w, (void)x, (void)y, (void)width,
        (void)height;
    return (xcb_void_cookie_t){ 0 };
}

xcb_void_cookie_t xcb_change_gc(xcb_connection_t *c, xcb_gcontext_t g,
                                uint32_t mask, const void *values)
{
    (void)c, (void)g, (void)mask, (void)values;
    return (xcb_void_cookie_t){ 0 };
}

// The rectangles of the last xcb_poly_fill_rectangle call.
static xcb_rectangle_t filled[CELLS * CELLS];
static int nfilled;

xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *c,
                                          xcb_drawable_t d, xcb_gcontext_t g,
                                          uint32_t n,
                                          const xcb_rectangle_t *rects)
{
    (void)c, (void)d, (void)g;
    memcpy(filled, rects, n * sizeof *rects);
    nfilled = n;
    return (xcb_void_cookie_t){ 0 };
}

static int fails;
#define CHECK(c)                                                   \
    do {                                                           \
        if (!(c)) {                                                \
            printf("FAIL line %d: %s\n", __LINE__, #c);            \
            fails++;                                               \
        }                                                          \
    } while (0)

#define XK_Up    (XK_Left + 1)
#define XK_Right (XK_Left + 2)
#define XK_Down  (XK_Left + 3)

// Puts the food somewhere out of the way of the tests.
static void restart(void)
{
    reset();
    food = (Point){ 0, CELLS - 1 };
}

// Number of lit font pixels of the score digit at position i (0 = ones).
static int score_pixels(int i)
{
    int n = 0, x = 46 - i * 18;
    for (int r = 0; r < nfilled; r++)
        if (filled[r].x >= x && filled[r].x < x + 3 * PIXEL &&
            filled[r].y >= SIZE)
            n++;
    return n;
}

static int popcount(int v)
{
    return __builtin_popcount(v);
}

int main(void)
{
    // Wall: starting at y=10 moving up, 10 steps are fine, the 11th crashes.
    seed = 1;
    restart();
    for (int i = 0; i < 10; i++)
        step();
    CHECK(state == RUNNING && snake[0].y == 0);
    step();
    CHECK(state == DEAD);

    // Space restarts only after the game ended, R always.
    handle_key(' ');
    CHECK(state == RUNNING && len == START && snake[0].y == 10);
    food = (Point){ 0, CELLS - 1 };
    step();
    handle_key(' ');
    CHECK(snake[0].y == 9);
    handle_key('r');
    CHECK(snake[0].y == 10);

    // Pause toggles, arrows are ignored while paused, no pause when dead.
    handle_key('p');
    CHECK(state == PAUSED);
    handle_key(XK_Left);
    CHECK(dir.x == 0 && dir.y == -1);
    handle_key('p');
    CHECK(state == RUNNING);
    state = DEAD;
    handle_key('p');
    CHECK(state == DEAD);

    // Esc quits, other keys don't.
    CHECK(handle_key(XK_Escape) == 0);
    CHECK(handle_key('x') == 1);

    // Moving up, Left and then Down within one tick must not reverse the snake.
    restart();
    handle_key(XK_Left);
    handle_key(XK_Down);
    CHECK(dir.x == -1 && dir.y == 0);
    step();
    CHECK(state == RUNNING && snake[0].x == 9 && snake[0].y == 10);
    handle_key(XK_Right); // a direct reversal is ignored too
    CHECK(dir.x == -1);

    // A snake may move into the cell its tail is leaving...
    restart();
    len = 4;
    snake[0] = (Point){ 5, 5 };
    snake[1] = (Point){ 6, 5 };
    snake[2] = (Point){ 6, 6 };
    snake[3] = (Point){ 5, 6 };
    dir = moved = (Point){ 0, 1 };
    step();
    CHECK(state == RUNNING && snake[0].x == 5 && snake[0].y == 6);
    // ...but running into its body is still deadly.
    restart();
    len = 5;
    snake[0] = (Point){ 5, 5 };
    snake[1] = (Point){ 6, 5 };
    snake[2] = (Point){ 6, 6 };
    snake[3] = (Point){ 5, 6 };
    snake[4] = (Point){ 4, 6 };
    dir = moved = (Point){ 0, 1 };
    step();
    CHECK(state == DEAD);

    // Eating grows the snake and places new food off the snake.
    restart();
    food = (Point){ 10, 9 };
    step();
    CHECK(len == START + 1);
    CHECK(!on_snake(food, len));

    // Food is never placed on the snake, even when the board is nearly full.
    for (int t = 0; t < 200; t++) {
        len = CELLS * CELLS - 1 - t % 5;
        for (int i = 0; i < len; i++)
            snake[i] = (Point){ (i + t) % CELLS, (i + t) / CELLS % CELLS };
        place_food();
        CHECK(!on_snake(food, len));
        CHECK(food.x >= 0 && food.x < CELLS && food.y >= 0 && food.y < CELLS);
    }

    // Win: eating the last free cell (0,0) with the head at (1,0).
    restart();
    len = CELLS * CELLS - 1;
    snake[0] = (Point){ 1, 0 };
    int n = 1;
    for (int y = 0; y < CELLS; y++)
        for (int x = 0; x < CELLS; x++)
            if (y || x > 1)
                snake[n++] = (Point){ x, y };
    CHECK(n == len);
    food = (Point){ 0, 0 };
    dir = moved = (Point){ -1, 0 };
    step();
    CHECK(state == WON && len == CELLS * CELLS);

    // The score is drawn as 3 digits below the board.
    len = START + 105;
    draw_score();
    CHECK(score_pixels(0) == popcount(digits[5]));
    CHECK(score_pixels(1) == popcount(digits[0]));
    CHECK(score_pixels(2) == popcount(digits[1]));
    CHECK(nfilled == 1 + popcount(digits[1]) + popcount(digits[0]) +
                             popcount(digits[5]));
    // The first rectangle is the line that marks the bottom wall.
    CHECK(filled[0].x == 0 && filled[0].y == SIZE && filled[0].width == SIZE &&
          filled[0].height == 1);
    len = CELLS * CELLS; // the highest possible score: 397
    draw_score();
    CHECK(score_pixels(0) == popcount(digits[7]));
    CHECK(score_pixels(1) == popcount(digits[9]));
    CHECK(score_pixels(2) == popcount(digits[3]));
    // The 1 (rows .#. ##. .#. .#. ###) has a single pixel in the middle of
    // its top row, two on the left in its second row and a full bottom row.
    len = START + 1;
    draw_score();
    int top = 0, second = 0, bottom = 0;
    for (int r = 0; r < nfilled; r++) {
        if (filled[r].x < 46) // only look at the ones digit
            continue;
        if (filled[r].y == SIZE + 5) {
            top++;
            CHECK(filled[r].x == 46 + PIXEL);
        }
        if (filled[r].y == SIZE + 5 + PIXEL) {
            second++;
            CHECK(filled[r].x < 46 + 2 * PIXEL);
        }
        bottom += filled[r].y == SIZE + 5 + 4 * PIXEL;
    }
    CHECK(top == 1 && second == 2 && bottom == 3);

    // Random cells cover the whole board evenly.
    int seen[CELLS] = { 0 };
    for (int i = 0; i < 10000; i++) {
        int c = random_cell();
        CHECK(c >= 0 && c < CELLS);
        seen[c]++;
    }
    for (int i = 0; i < CELLS; i++)
        CHECK(seen[i] > 300);

    if (fails)
        printf("%d unit test(s) FAILED\n", fails);
    else
        printf("All unit tests passed.\n");
    return fails != 0;
}
