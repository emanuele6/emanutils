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
    psendfd \
    secretmemfd \

all: $(UTILS)
.PHONY: all

clean:
	rm -f -- $(UTILS)
.PHONY: clean
