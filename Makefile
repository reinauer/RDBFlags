AMIGADATE = $(shell date +"%-d.%-m.%Y")

all: RDBFlags.gcc

RDBFlags.gcc:
	m68k-amigaos-gcc -Wno-int-conversion RDBFlags.c -o RDBFlags.gcc -D__far__= -D__AMIGADATE__=\"$(AMIGADATE)\"
	m68k-amigaos-strip -s RDBFlags.gcc

clean:
	rm RDBFlags.gcc
