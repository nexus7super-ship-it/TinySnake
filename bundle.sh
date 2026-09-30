#!/bin/sh
# Completes the TinySnake bundle that "make bundle" builds: copies all shared
# libraries and the dynamic loader that bin/TinySnake needs into lib/, collects
# their licenses and adds a launcher script.
#
# Usage: ./bundle.sh <bundle directory>

set -e

dir=$1
exe=$dir/bin/TinySnake
mkdir -p "$dir/lib" "$dir/licenses"

# ldd prints "libxcb.so.1 => /usr/lib64/libxcb.so.1 (0x...)" for libraries
# and "/lib64/ld-linux-x86-64.so.2 (0x...)" for the dynamic loader.
libs=$(ldd "$exe" | awk '$2 == "=>" && $3 ~ /^\// { print $1 " " $3 }')
loader=$(ldd "$exe" | awk '$1 ~ /^\// { print $1 }')

# Prints the license files of the package that owns the file $1.
licenses_of() {
    file=$(readlink -f "$1")
    if command -v rpm >/dev/null 2>&1 && pkg=$(rpm -qf "$file" 2>/dev/null); then
        # Some packages ship their license as documentation instead.
        { rpm -qL "$pkg"; rpm -qd "$pkg" | grep -E '/(COPYING|LICEN[CS]E)[^/]*$'; } | sort -u
    elif command -v dpkg >/dev/null 2>&1 &&
        pkg=$(dpkg -S "$file" 2>/dev/null || dpkg -S "$1" 2>/dev/null); then
        pkg=${pkg%%:*}
        echo "/usr/share/doc/$pkg/copyright"
    elif command -v pacman >/dev/null 2>&1 && pkg=$(pacman -Qoq "$file" 2>/dev/null); then
        find "/usr/share/licenses/$pkg" -type f 2>/dev/null
    fi
}

copy_licenses() {
    name=$(basename "$1")
    licenses_of "$1" | while read -r license; do
        if [ -f "$license" ]; then
            mkdir -p "$dir/licenses/$name"
            cp "$license" "$dir/licenses/$name/"
        fi
    done
}

echo "$libs" | while read -r soname path; do
    cp -L "$path" "$dir/lib/$soname"
    copy_licenses "$path"
done
cp -L "$loader" "$dir/lib/"
copy_licenses "$loader"
cp LICENSE "$dir/licenses/TinySnake-LICENSE"

# The bundled glibc only works together with its own dynamic loader, so the
# launcher starts the game through that loader instead of the system's one.
cat > "$dir/TinySnake" <<EOF
#!/bin/sh
# Starts TinySnake with the bundled libraries instead of the system's ones.
dir=\$(dirname "\$(readlink -f "\$0")")
exec "\$dir/lib/$(basename "$loader")" --inhibit-cache --library-path "\$dir/lib" \\
    "\$dir/bin/TinySnake" "\$@"
EOF
chmod +x "$dir/TinySnake"

cat > "$dir/README.txt" <<EOF
TinySnake - self-contained bundle

Start the game with:  ./TinySnake

This folder contains everything the game needs apart from the Linux kernel
and an X server (or XWayland on Wayland desktops):

  TinySnake    launcher script, starts the game with the bundled libraries
  bin/         the game itself
  lib/         the shared libraries and the dynamic loader it uses
  licenses/    the licenses of TinySnake and of the bundled libraries

Built on $(. /etc/os-release && echo "$PRETTY_NAME") for $(uname -m).
EOF

echo "$dir: $(du -sh "$dir" | cut -f1), $(ls "$dir/lib" | wc -l) libraries"
