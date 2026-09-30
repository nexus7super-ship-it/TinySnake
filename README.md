# TinySnake

A complete Snake game for Linux in **4259 bytes**: a real X11 window, keyboard
control, pause, restart and a win condition. It is written in plain C against
XCB and squeezed with nothing more than GCC and binutils.

There are two ways to build it:

- **Ultra-trimmed** (`make`, the default): the tiny 4259-byte binary. It uses
  the libraries installed on your system.
- **Bundle** (`make bundle`): a self-contained folder for maximum
  compatibility, which ships every shared library the game needs.

It runs on any Linux desktop with X11, and on Wayland desktops (GNOME, KDE
Plasma, Sway, ...) through **XWayland**, which those desktops ship by default.

## Controls

| Key        | Action                                 |
|------------|----------------------------------------|
| Arrow keys | Steer the snake                        |
| `P`        | Pause / resume                         |
| `R`        | Restart                                |
| `Space`    | Restart after a game over or a win     |
| `Esc`      | Quit                                   |

The snake gets faster as it grows. It turns dark green while the game is
paused, gray when you lose and gold when you fill the whole 20 × 20 board.

## Building

### 1. Install the dependencies

You need GCC, binutils, make and the XCB development files:

| Distribution         | Command                                             | Tested |
|----------------------|-----------------------------------------------------|--------|
| Ubuntu / Debian      | `sudo apt install build-essential libxcb1-dev`      | ✅ Ubuntu 25.10 |
| Fedora               | `sudo dnf install @development-tools libxcb-devel`  | ✅ Fedora 44 |
| Arch Linux           | `sudo pacman -S base-devel libxcb`                  | ❌ not tested |

The build needs GCC 12 or newer (for `-Oz`) and binutils 2.41 or newer (for
`strip --strip-section-headers`). Current releases of all the distributions
above have both.

### 2. Compile

```sh
make
```

That's it. The Makefile compiles, strips and trims the binary and prints its
size at the end:

```
TinySnake: 4259 bytes
```

The exact size can differ by a few bytes between compiler versions.

### 3. Play

```sh
./TinySnake
```

## The bundle version

The ultra-trimmed binary is as small as possible, but it depends on the
libraries of the system it runs on. For maximum compatibility, build the
bundle instead:

```sh
make bundle
```

This creates the folder `TinySnake-bundle/`:

```
TinySnake-bundle/
├── TinySnake     launcher script, start the game with this
├── bin/          the game, built with the usual hardening features turned on
├── lib/          all shared libraries it needs, including glibc and its loader
├── licenses/     the licenses of TinySnake and of the bundled libraries
└── README.txt
```

Start it with:

```sh
./TinySnake-bundle/TinySnake
```

The launcher runs the game through the bundled dynamic loader, so it doesn't
use any library of the target system. You can copy the folder to another
x86-64 Linux machine and run it there, even on distributions without glibc
or without XCB (tested on Alpine Linux). The only requirements are the Linux
kernel and an X server or XWayland. The bundle is about 4 MB, most of it glibc.

## How it gets so small

A normal build of the same code is over 10 KB. These are the tricks that save
most of the bytes:

- **`-Oz`**: optimize for size above everything else.
- **`-nostartfiles`**: leaves out the C runtime startup code. `main.c` has its
  own tiny `_start` function, and the dynamic loader still initializes glibc.
- **`-fno-plt`**: calls library functions directly through the GOT, without
  PLT stubs.
- **`-Wl,-z,norelro -Wl,-z,noseparate-code -Wl,-z,common-page-size=8`**: packs
  the code and data segments of the ELF file tightly instead of padding them
  to 4 KB page boundaries.
- **`-Wa,-mx86-used-note=no`, `-Wl,--build-id=none`,
  `-fno-asynchronous-unwind-tables`**: drop notes and unwind tables that the
  program doesn't need.
- **`strip --strip-section-headers`**: removes everything the program loader
  doesn't need, including the section headers.
- **Trimming trailing zero bytes**: the kernel fills the rest of the last
  memory page with zeros anyway. The Makefile computes exactly how many bytes
  can be cut off, so there is no longer a hand-picked `truncate` size that
  could break the binary.
- **Few library imports**: every imported function costs 60 to 90 bytes of ELF
  metadata, so the code avoids unnecessary library calls.

## Security notes

TinySnake is open source and small enough to read in a few minutes. Building
it yourself from `main.c` is the best way to know what you run.

To save bytes, the build turns off some hardening features that many
distributions enable by default:

- **Stack protector** (`-fno-stack-protector`): detects some buffer overflows
  at runtime.
- **RELRO** (`-z norelro`): makes the relocation tables read-only after
  loading.
- **Control-flow protection** (`-fcf-protection=none`) and
  **`_FORTIFY_SOURCE`**: additional checks against exploitation.
- **PIE** (`-no-pie`): the program is always loaded at the same address.

These features make it harder to exploit bugs in a program. TinySnake doesn't
read files, doesn't use the network and doesn't handle untrusted input; it
only talks to your X server. The stack stays non-executable.

## License

MIT, see [LICENSE](LICENSE).
