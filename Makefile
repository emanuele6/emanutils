UTILS = \
    chainif \
    creatememfd \
    fdcmp \
    fdtruncate \
    fdseal \
    openpathfd \
    openpidfd \
    pidfdgetfd \
    pollinfd \

all: $(UTILS)
.PHONY: all

clean:
	rm -f -- $(UTILS)
.PHONY: clean
