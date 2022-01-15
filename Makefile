LDFLAGS = -lgsl -lgslcblas -lm

all: main gsl_ode ray ray.wasm

ray: ray.c
	cc ray.c $$(pkg-config --libs --cflags raylib) -o ray

ray.wasm: ray.c
	emcc -o ray.html ray.c \
	-Os -Wall \
	./raylib/src/libraylib.a -I. -I./raylib/src/ -L. -L./raylib/src/libraylib.a \
	-s USE_GLFW=3 -s ASYNCIFY \
	-DPLATFORM_WEB
