# TinySnake

## To compile the Snake game, you must install these dependencies immediately:
on Linux Ubuntu/Debian:
`sudo apt install build-essential`

on Linux Fedora:
`sudo dnf install @development-tools`

on Arch Linux:
`sudo pacman -S base-devel`

## Now that we've checked that off the list, we can start compiling:
1.`gcc -Os -fno-plt -fno-ident -fno-asynchronous-unwind-tables -fno-stack-protector -ffunction-sections -fdata-sections -fno-unwind-tables -fno-exceptions main.c -o TinySnake -lxcb -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -Wl,-z,noseparate-code -s`
  
2.`objcopy -R .note.gnu.property -R .note.ABI-tag -R .comment -R .gnu.version -R .gnu.hash -R .eh_frame TinySnake`
      
3.`strip -s TinySnake`

4.`truncate -s 4960 TinySnake` **(Warning! this can destroy the game!** If you encounter a `Segmentation fault` or other errors, you should recompile the game and omit the truncate command.)

## Enjoy the game:
`./TinySnake`

The game is only 4960 bytes in size.
This could be a world record,
at least on Linux as an X server window.
I recently tested whether
it also runs on platforms other than
Kubuntu 25.10, and apparently
it now runs on Fedora 43, and I think it
most likely also runs on
other platforms since the dependencies are
just system stuff anyway.

## Stack

- **Programming Language:** `C`
- **Compiler:** `GCC`
- **Utilities:** `objcopy` | `strip` | `truncate`
