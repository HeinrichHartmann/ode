all: ray_planet

.PHONY: prep
prep:
	sudo apt-get install -y wget build-essential libdrm-dev libegl1-mesa-dev libgles2-mesa-dev libgbm-dev

src/gsl:
	mkdir -p src
	wget https://ftp.gnu.org/gnu/gsl/gsl-2.7.tar.gz
	tar -xzvf gsl-2.7.tar.gz
	mv gsl-2.7 src/gsl
	rm -rf gsl-2.7.tar.gz

build/gsl: src/gsl
	mkdir -p build
	DST=$$(pwd)/build; cd src/gsl && ./configure --prefix=$$DST/gsl && make -j && make install

src/raylib:
	mkdir -p src
	wget https://github.com/raysan5/raylib/archive/refs/tags/4.0.0.tar.gz
	tar -xzvf 4.0.0.tar.gz
	mv raylib-4.0.0 src/raylib
	rm -rf 4.0.0.tar.gz

build/libraylib.a: src/raylib
	mkdir -p build
	cd src/raylib/src; make -j PLATFORM=PLATFORM_DRM
	cp src/raylib/src/libraylib.a $@

ray_planet: ray_planet.c build/gsl build/libraylib.a
	gcc -o ray_planet ray_planet.c build/libraylib.a \
		 -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces -Wunused-result -s -O1 -std=gnu99 -DEGL_NO_X11 \
		-I. -I/usr/include/libdrm -I./src/raylib/src -I./build/gsl/include \
		-L. -L./build/gsl/lib -L./build -L \
		-lraylib -lGLESv2 -lEGL -lpthread -lrt -lm -lgbm -ldrm -ldl -lgsl -lgslcblas -lm \
		-DPLATFORM_DRM

run: ray_planet
	LD_LIBRARY_PATH=./build/gsl/lib ./ray_planet
