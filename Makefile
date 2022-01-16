LIBS = raylib gsl
LDFLAGS=-all-static $(shell pkg-config --libs $(LIBS))
CFLAGS=-O0 -g $(shell pkg-config --cflags $(LIBS))

all: main gsl_ode ray ray.html ray_ode

%.html: %.c
	# clang gsl_ex.c -o example -O2 --target=wasm32-wasi
	emcc -o $@ $< \
	-Os \
	-I/opt/gsl-2.6/include -L/opt/gsl-2.6/lib -lgsl -lm -lc \
	$(shell pkg-config --cflags gsl) \
	./raylib/src/libraylib.a -I. -I./raylib/src/ -I./gsl/build/include \
	-L. -L./raylib/src/libraylib.a -L./gsl/build/.libs/libgsl.a \
	-s USE_GLFW=3 -s ASYNCIFY \
	-DPLATFORM_WEB
