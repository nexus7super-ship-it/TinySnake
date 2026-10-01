// End-to-end tests: runs a TinySnake binary on an X server and drives it with
// synthetic key events, checking the window, its pixels and the exit codes.
//
// Run with: make test-e2e
// That starts a private Xvfb server. Don't run this on your desktop's X
// server: the tests kill X clients.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <xcb/xcb.h>

#define BOARD 400 // size of the board in pixels
#define BAR   30  // height of the score bar

#define XK_Escape 0xff1b

enum { RED = 0xff0000, GREEN = 0x00ff00, DARK_GREEN = 0x008000,
       GRAY = 0x808080, SCORE = 0xc0c0c0 };

static xcb_connection_t *conn;
static xcb_screen_t *screen;
static xcb_get_keyboard_mapping_reply_t *keymap;
static xcb_keycode_t min_keycode;
static const char *binary;
static pid_t pid;
static int fails;

#define CHECK(c)                                                   \
    do {                                                           \
        if (c)                                                     \
            printf("  ok: %s\n", #c);                              \
        else {                                                     \
            printf("  FAIL line %d: %s\n", __LINE__, #c);          \
            fails++;                                               \
        }                                                          \
    } while (0)

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void msleep(int ms)
{
    usleep(ms * 1000);
}

static xcb_keycode_t keycode(xcb_keysym_t sym)
{
    xcb_keysym_t *syms = xcb_get_keyboard_mapping_keysyms(keymap);
    int per = keymap->keysyms_per_keycode;
    int total = xcb_get_keyboard_mapping_keysyms_length(keymap);
    for (int i = 0; i < total; i += per)
        if (syms[i] == sym)
            return min_keycode + i / per;
    return 0;
}

static void title(xcb_window_t w, char *buf, int size)
{
    xcb_get_property_reply_t *r = xcb_get_property_reply(
        conn, xcb_get_property(conn, 0, w, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 64),
        NULL);
    int n = r ? xcb_get_property_value_length(r) : 0;
    if (n >= size)
        n = size - 1;
    if (n)
        memcpy(buf, xcb_get_property_value(r), n);
    buf[n] = 0;
    free(r);
}

// Is w a TinySnake game window? Besides the title, the fixed size in the
// WM_NORMAL_HINTS must match, so window manager frames are never mistaken
// for the game.
static int is_game(xcb_window_t w)
{
    char buf[64];
    title(w, buf, sizeof buf);
    if (strcmp(buf, "TinySnake") != 0)
        return 0;
    xcb_get_property_reply_t *r = xcb_get_property_reply(
        conn, xcb_get_property(conn, 0, w, XCB_ATOM_WM_NORMAL_HINTS,
                               XCB_ATOM_WM_SIZE_HINTS, 0, 18), NULL);
    int ok = r && xcb_get_property_value_length(r) == 18 * 4 &&
             ((uint32_t *)xcb_get_property_value(r))[5] == BOARD &&
             ((uint32_t *)xcb_get_property_value(r))[6] == BOARD + BAR;
    free(r);
    return ok;
}

// Windows of games that were already running before the current one.
static xcb_window_t known[64];
static int nknown;

static int is_known(xcb_window_t w)
{
    for (int i = 0; i < nknown; i++)
        if (known[i] == w)
            return 1;
    return 0;
}

static xcb_window_t find(xcb_window_t w)
{
    if (is_game(w) && !is_known(w))
        return w;
    xcb_query_tree_reply_t *t = xcb_query_tree_reply(conn, xcb_query_tree(conn, w), NULL);
    xcb_window_t found = 0;
    if (!t)
        return 0;
    xcb_window_t *children = xcb_query_tree_children(t);
    for (int i = 0; i < xcb_query_tree_children_length(t) && !found; i++)
        found = find(children[i]);
    free(t);
    return found;
}

static xcb_window_t start(void)
{
    xcb_window_t old;
    while (nknown < 64 && (old = find(screen->root)))
        known[nknown++] = old;
    pid = fork();
    if (!pid) {
        execl(binary, binary, (char *)NULL);
        _exit(127);
    }
    for (int i = 0; i < 300; i++) {
        xcb_window_t w = find(screen->root);
        if (w)
            return w;
        msleep(10);
    }
    return 0;
}

static void stop(void)
{
    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
    pid = 0;
}

static void key(xcb_window_t w, xcb_keysym_t sym)
{
    xcb_key_press_event_t e = { 0 };
    e.response_type = XCB_KEY_PRESS;
    e.detail = keycode(sym);
    e.time = (uint32_t)(now() * 1000);
    e.root = screen->root;
    e.event = w;
    e.same_screen = 1;
    xcb_send_event(conn, 0, w, 0, (const char *)&e);
    xcb_flush(conn);
}

// Exit status of the game, -1 on timeout, 1000 + signal if it was killed.
static int wait_exit(int ms)
{
    for (int i = 0; i < ms; i += 5) {
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            pid = 0;
            return WIFEXITED(status) ? WEXITSTATUS(status)
                                     : 1000 + WTERMSIG(status);
        }
        msleep(5);
    }
    return -1;
}

// Counts the pixels of a color in an area of the window, -1 on error.
static int count(xcb_window_t w, int y, int height, uint32_t color)
{
    xcb_get_image_reply_t *r = xcb_get_image_reply(
        conn, xcb_get_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, w, 0, y, BOARD, height, ~0u),
        NULL);
    if (!r)
        return -1;
    uint32_t *p = (uint32_t *)xcb_get_image_data(r);
    int n = 0;
    for (int i = 0; i < BOARD * height; i++)
        n += (p[i] & 0xffffff) == color;
    free(r);
    return n;
}

static int board(xcb_window_t w, uint32_t color)
{
    return count(w, 0, BOARD, color);
}

static int dead(xcb_window_t w)
{
    return board(w, GRAY) > 0;
}

static int paused(xcb_window_t w)
{
    return board(w, DARK_GREEN) > 0 && board(w, GREEN) == 0;
}

static int wait_for(int (*condition)(xcb_window_t), xcb_window_t w, int ms)
{
    for (int i = 0; i < ms; i += 5) {
        if (condition(w))
            return 1;
        msleep(5);
    }
    return 0;
}

#define CELL_PIXELS (19 * 19)
#define ZERO_PIXELS (12 * 16) // the digit 0 has 12 font pixels of 4 x 4

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 2) {
        fprintf(stderr, "usage: %s <TinySnake binary>\n", argv[0]);
        return 2;
    }
    binary = argv[1];
    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) {
        fprintf(stderr, "can't connect to the X server\n");
        return 2;
    }
    const xcb_setup_t *setup = xcb_get_setup(conn);
    screen = xcb_setup_roots_iterator(setup).data;
    min_keycode = setup->min_keycode;
    keymap = xcb_get_keyboard_mapping_reply(
        conn,
        xcb_get_keyboard_mapping(conn, min_keycode,
                                 setup->max_keycode - min_keycode + 1),
        NULL);
    printf("Testing %s\n", binary);

    // The snake starts in the middle moving up and crashes into the wall
    // after 11 steps (about 1.3 s). A flood of key presses must not make
    // it faster.
    printf("speed with a flood of key presses:\n");
    xcb_window_t w = start();
    CHECK(w != 0);
    double t0 = now(), t = 0;
    while (w && now() - t0 < 5) {
        key(w, 'x');
        if (dead(w)) {
            t = now() - t0;
            break;
        }
        msleep(1);
    }
    printf("  the snake survived %.2f s\n", t);
    CHECK(t > 1.1 && t < 2.0);
    stop();

    printf("window, drawing, pause, restart and Esc:\n");
    w = start();
    CHECK(w != 0);
    if (!w)
        return 1;
    msleep(300);
    char buf[64];
    title(w, buf, sizeof buf);
    CHECK(strcmp(buf, "TinySnake") == 0);
    CHECK(board(w, RED) == CELL_PIXELS);            // the food
    CHECK(board(w, GREEN) == 3 * CELL_PIXELS);      // the snake
    CHECK(count(w, BOARD, 1, SCORE) == BOARD);                 // the wall line
    CHECK(count(w, BOARD + 1, BAR - 1, SCORE) == 3 * ZERO_PIXELS); // "000"
    key(w, 'p');
    CHECK(wait_for(paused, w, 500));
    msleep(2000); // without the pause, the snake would have crashed by now
    CHECK(paused(w) && !dead(w));
    key(w, 'p');
    CHECK(wait_for(dead, w, 2000));
    msleep(200);
    CHECK(board(w, GRAY) == 3 * CELL_PIXELS && board(w, GREEN) == 0);
    key(w, ' ');
    msleep(300);
    CHECK(board(w, RED) == CELL_PIXELS && board(w, GREEN) == 3 * CELL_PIXELS &&
          board(w, GRAY) == 0);
    key(w, XK_Escape);
    CHECK(wait_exit(1000) == 0);
    stop();

    // A window manager kills clients without WM_DELETE_WINDOW support.
    printf("closing the window:\n");
    w = start();
    CHECK(w != 0);
    if (w) {
        msleep(300);
        xcb_kill_client(conn, w);
        xcb_flush(conn);
        CHECK(wait_exit(1000) == 1);
    }
    stop();

    printf("no X server:\n");
    pid = fork();
    if (!pid) {
        unsetenv("DISPLAY");
        execl(binary, binary, (char *)NULL);
        _exit(127);
    }
    CHECK(wait_exit(1000) == 1);
    stop();

    if (fails)
        printf("%d end-to-end test(s) FAILED\n", fails);
    else
        printf("All end-to-end tests passed.\n");
    return fails != 0;
}
