# TinySnake

## To compile the Snake game, you must install these dependencies immediately:
on Linux Ubuntu/Debian ($${\color{green}tested, \space its \space works on Kubuntu 25.10}$$):
`sudo apt install build-essential`

on Linux Fedora 43 & 44 ($${\color{green}tested, \space its \space works}$$):
`sudo dnf install @development-tools`

on Arch Linux ($${\color{red} not \space tested}$$):
`sudo pacman -S base-devel`

## Now that we've checked that off the list, we can start compiling:
1.`gcc -Os -fno-plt -fno-ident -fno-asynchronous-unwind-tables -fno-stack-protector -ffunction-sections -fdata-sections -fno-unwind-tables -fno-exceptions main.c -o TinySnake -lxcb -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -Wl,-z,noseparate-code -s`
  
2.`objcopy -R .note.gnu.property -R .note.ABI-tag -R .comment -R .gnu.version -R .gnu.hash -R .eh_frame TinySnake`
      
3.`strip -s TinySnake`

4.`truncate -s 4960 TinySnake` **(Warning! this can destroy the game!** If you encounter a `Segmentation fault` or other errors, you should recompile the game and omit the truncate command.)

You can also use the pre-compiled package here. However, I recommend the latest version, as the old one had a bug where the game speed increased along with the CPU speed. Back when I had a laptop from 2000 or even earlier, the game actually ran at normal speed.

## Enjoy the game:
`./TinySnake`

The game is only 4960 bytes in size.
This could be a world record,
at least on Linux as an X server window.
The prerequisites are a Linux desktop
with X Server support.

## Stack

- **Programming Language:** `C`
- **Compiler:** `GCC and all it flags`
- **Utilities:** `objcopy` | `strip` | `truncate`

## Consider about this program

This is not malware. One reason for this
is that there is practically no
malware smaller than 5 kilobytes.
While such malware does indeed exist,
it is typically written in assembly or C;
since this project is open source,
you can simply check the source code
and verify that everything is above board.
You also don't have to rely on me
using the pre-built package—you can
compile it yourself from the source code.
Furthermore, the only aspect that *might*
be considered unsafe is the use of a specific
GCC flag that disables security features.
This flag turns off stack-smashing protection.
This protection serves to make the file
more resistant to exploits—specifically,
it is intended to prevent malware from
injecting itself into the file and manipulating it.
However, since this is neither a system file
nor a file essential to the system, this does not matter.
Additionally, the program disables
RELRO (Read-Only Relocations). This has
the disadvantage that the program starts more slowly
and the file is slightly larger. The advantages are
that the program starts faster and the file
is slightly smaller. Incidentally, RELRO ensures that
the relocation tables are write-protected
after the program loads; essentially, this also means
that malware cannot infiltrate the program
and modify it during runtime. But again,
it doesn't matter, because firstly, another GCC flag
causes the relocation tables to be discarded, and
the Snake game here is not a critical system file
or something essential to the system.
