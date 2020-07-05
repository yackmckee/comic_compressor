CFLAGS = -Ofast -march=native
CFLAGS_TEST = -O0 -ggdb
all: encoder decoder
test: encoder_test decoder_test

encoder:
	clang src/encoder.c -o encoder $(CFLAGS) -I./include -L. lib/libz.a -Wall

decoder:
	clang src/decoder.c -o decoder $(CFLAGS) -I./include -L. lib/libz.a -Wall

encoder_test:
	clang src/encoder.c -o encoder $(CFLAGS_TEST) -I./include -L. lib/libz.a -Wall

decoder_test:
	clang src/decoder.c -o decoder $(CFLAGS_TEST) -I./include -L. lib/libz.a -Wall

clean:
	rm encoder decoder
