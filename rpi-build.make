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
	cd src/gsl; ./configure --prefix=$$(pwd)/build/gsl; make; make install

src/raylib:
	mkdir -p src
	wget https://github.com/raysan5/raylib/archive/refs/tags/4.0.0.tar.gz
	tar -xzvf 4.0.0.tar.gz
	mv raylib-4.0.0 src/raylib
	rm -rf 4.0.0.tar.gz

build/libraylib.a: src/raylib
	mkdir -p build
	cd src/raylib/src; make PLATFORM=PLATFORM_DRM
	cp src/raylib/src/libraylib.a $@

# DEBIAN_FRONTEND=noninteractive sudo apt-get install -y --no-install-recommends lxterminal gvfs
# sudo apt-get install -y libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev libasound2-dev mesa-common-dev libgl1-mesa-dev
# cd raylib/src; make PLATFORM=PLATFORM_DRM

ray_planet: ray_planet.c build/gsl build/libraylib.a
	gcc -o ray_planet ray_planet.c build/libraylib.a \
		 -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces -Wunused-result -s -O1 -std=gnu99 -DEGL_NO_X11 \
		-I. -I/usr/include/libdrm -I/opt/gsl/include -I./raylib/src \
		-L. -L./raylib/src -L/opt/gsl/lib -L \
		-lraylib -lGLESv2 -lEGL -lpthread -lrt -lm -lgbm -ldrm -ldl -lgsl -lgslcblas -lm \
		-DPLATFORM_DRM

