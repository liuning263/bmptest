CC = gcc
CFLAGS = -Wall

all: my_bmp2raw

bmp_parser: my_bmp2raw.c
	$(CC) $(CFLAGS) -o my_bmp2raw my_bmp2raw.c

clean:
	rm -f my_bmp2raw *.raw 
