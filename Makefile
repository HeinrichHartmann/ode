LIBS = raylib gsl
LDFLAGS=-all-static $(shell pkg-config --libs $(LIBS))
CFLAGS=-O0 -g $(shell pkg-config --cflags $(LIBS))

build: ray_planet.html
	mkdir -p build
	mv ray_planet{.wasm,.js,.html} build

deploy: build
	rsync -av ./build/ root@192.168.2.12:/share/hhartmann/attic/www

%.html: %.c
	emcc -o $@ $< \
	-Os \
	-I/opt/gsl-2.6/include -L/opt/gsl-2.6/lib -lgsl -lm -lc \
	$(shell pkg-config --cflags gsl) \
	./raylib/src/libraylib.a -I. -I./raylib/src/ -I./gsl/build/include \
	-L. -L./raylib/src/libraylib.a -L./gsl/build/.libs/libgsl.a \
	-s USE_GLFW=3 -s ASYNCIFY \
	-DPLATFORM_WEB
