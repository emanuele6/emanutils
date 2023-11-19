UTILS = \
    chainif \
    creatememfd \
    openpathfd \
    openpidfd \
    pidfdgetfd \
    pollinfd \

all: $(UTILS)
.PHONY: all

clean: $(UTILS)
	rm -f -- $(UTILS)
.PHONY: clean
