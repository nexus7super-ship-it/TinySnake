# make           builds the ultra-trimmed TinySnake binary (the default)
# make bundle    builds TinySnake-bundle/, a self-contained folder that ships
#                all shared libraries it needs, for maximum compatibility
# make clean     removes everything that was built

CC = gcc

# -Oz optimizes for size. The other flags turn off features that some
# distributions (e.g. Ubuntu) enable by default and that would add code and
# imports: position independent code, stack protection and CET.
CFLAGS = -Oz -fno-pie -fno-plt -fno-stack-protector -fcf-protection=none \
         -DTINY

# -nostdlib leaves out the C runtime and libc; main.c has its own _start.
# tiny.ld lays out the ELF file without padding and without anything that
# isn't needed to run it.
LDFLAGS = -no-pie -nostdlib -Wl,--build-id=none -Wl,-T,tiny.ld

# The bundle is built for robustness instead of size, with the usual
# hardening features turned on.
BUNDLE_CFLAGS = -O2 -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=2
BUNDLE_LDFLAGS = -pie -s -Wl,-z,relro,-z,now

# The tiny build links against a stub libxcb.so.1 that is generated from the
# functions main.c calls (plus free). The stub doesn't depend on libc, so the
# binary doesn't either and needs no symbol versions. At runtime the real
# libxcb.so.1 is loaded, and it brings libc along, where free is found.
#
# After linking, the section headers are stripped and the trailing zero bytes
# are cut off. The kernel fills the rest of the last page with zeros anyway,
# so the file only must not end before that page.
TinySnake: main.c tiny.ld
	tmp=$$(mktemp -d) && \
	$(CC) $(CFLAGS) -c main.c -o $$tmp/main.o && \
	nm -u $$tmp/main.o | awk '$$2 != "_GLOBAL_OFFSET_TABLE_" { \
	    print "void " $$2 "(void) {}" }' > $$tmp/stub.c && \
	$(CC) -shared -nostdlib -fno-builtin $$tmp/stub.c -o $$tmp/libxcb.so \
	    -Wl,-soname,libxcb.so.1 && \
	$(CC) $$tmp/main.o -o $@ -L$$tmp -lxcb $(LDFLAGS); \
	status=$$?; rm -rf $$tmp; exit $$status
	strip --strip-section-headers $@
	truncate -s $$(od -An -v -tu1 -w1 $@ | awk '$$1 { n = NR } END { \
	    p = int((NR - 1) / 4096) * 4096; print (n > p ? n : p + 1) }') $@
	@echo "$@: $$(stat -c %s $@) bytes"

bundle: main.c bundle.sh
	rm -rf TinySnake-bundle
	mkdir -p TinySnake-bundle/bin
	$(CC) $(BUNDLE_CFLAGS) main.c -o TinySnake-bundle/bin/TinySnake -lxcb \
	    $(BUNDLE_LDFLAGS)
	./bundle.sh TinySnake-bundle

clean:
	rm -rf TinySnake TinySnake-bundle

.PHONY: bundle clean
