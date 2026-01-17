# TinySnake

## To compile the Snake game, you must install these dependencies immediately:
on Linux Ubuntu/Debian:
`sudo apt install build-essential`

on Linux Fedora:
`sudo dnf install @development-tools`

on Arch Linux:
`sudo pacman -S base-devel`

## Now that we've checked that off the list, we can start compiling:
1.`gcc -Os -fno-plt -fno-ident -fno-asynchronous-unwind-tables \
  -fno-stack-protector -ffunction-sections -fdata-sections \
  -fno-unwind-tables -fno-exceptions \
  main.c -o TinySnake -lxcb \
  -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -Wl,-z,noseparate-code -s`
  
2.`objcopy --remove-section=.note.gnu.property \
      --remove-section=.note.ABI-tag \
      --remove-section=.comment \
      --remove-section=.gnu.version \
      --remove-section=.gnu.hash \
      --remove-section=.eh_frame \
      TinySnake`
      
3.`strip -s TinySnake`

4.`truncate -s 4393 TinySnake`

## Enjoy the game:
`./TinySnake`

The game is only 4393 bytes in size.
It's possible this could be a world record,
but I think only if it's done very specifically.
