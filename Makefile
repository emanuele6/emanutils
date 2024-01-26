UTILS = \
    chainif \
    creatememfd \
    fdcmp \
    fdseal \
    fdtruncate \
    openpathfd \
    openpidfd \
    pidfdgetfd \
    pollinfd \
    secretmemfd \

all: $(UTILS)
.PHONY: all

clean:
	rm -f -- $(UTILS)
.PHONY: clean
