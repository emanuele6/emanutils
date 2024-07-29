PREFIX = /usr/local

UTILS = \
    chainif \
    creatememfd \
    fdcmp \
    fdseal \
    fdtruncate \
    mergeeet \
    openpathfd \
    openpidfd \
    pidfdgetfd \
    pollinfd \
    psendfd \
    ptytty \
    secretmemfd \

all: $(UTILS)
.PHONY: all

clean:
	rm -f -- $(UTILS)
.PHONY: clean

install:
	install -d -- $(DESTDIR)$(PREFIX)/bin
	install -p -- $(UTILS) $(DESTDIR)$(PREFIX)/bin
.PHONY: install
