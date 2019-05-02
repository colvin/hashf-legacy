CC	= clang

USER	= colvin
GROUP	= wheel
MODE	= 775

PREFIX	?= /usr/local

all: hashf.c
	${CC} -lmd -o ./hashf hashf.c

install: all
	install -o ${USER} -g ${GROUP} -m ${MODE} ./hashf ${PREFIX}/bin/hashf

clean:
	-rm ./hashf

.PHONY: clean
