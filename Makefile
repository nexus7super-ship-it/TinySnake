# make           builds the ultra-trimmed TinySnake binary (the default)
# make bundle    builds TinySnake-bundle/, a self-contained folder that ships
#                all shared libraries it needs, for maximum compatibility
# make clean     removes everything that was built

CC = gcc

# -Oz optimizes for size. The other flags remove unwind tables and turn off
# hardening features that some distributions (e.g. Ubuntu) enable by default
# and that would add extra code and imports. -mx86-used-note=no drops the
# x86 ISA note that some assemblers (e.g. on Fedora) add to every object.
CFLAGS = -Oz -fno-pie -fno-plt -fno-asynchronous-unwind-tables \
         -fno-stack-protector -fcf-protection=none -U_FORTIFY_SOURCE \
         -Wa,-mx86-used-note=no -DNOSTARTFILES

# -nostartfiles leaves out the C runtime startup code (main.c has its own
# _start). The -z options pack the ELF segments tightly without padding.
LDFLAGS = -no-pie -nostartfiles -s -Wl,--build-id=none -Wl,-z,norelro \
          -Wl,-z,noseparate-code -Wl,-z,common-page-size=8

# The bundle is built for robustness instead of size, with the usual
# hardening features turned on.
BUNDLE_CFLAGS = -O2 -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=2
BUNDLE_LDFLAGS = -pie -s -Wl,-z,relro,-z,now

# After linking, the section headers are stripped and the trailing zero bytes
# are cut off. The kernel fills the rest of the last page with zeros anyway,
# so the file only must not end before that page.
TinySnake: main.c
	$(CC) $(CFLAGS) $< -o $@ -lxcb $(LDFLAGS)
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
