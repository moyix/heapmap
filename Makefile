malloc.so: malloc.c
	clang -O3 -g -W -Wall -Wextra -shared -fPIC $< -o $@

all: malloc.so
