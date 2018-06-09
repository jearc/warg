EXE = moov
OBJS = main.o ./imgui/imgui_impl_sdl_gl3.o
OBJS += ./imgui/imgui.o ./imgui/imgui_demo.o ./imgui/imgui_draw.o
OBJS += ./libs/gl3w/GL/gl3w.o

LIBS = -lGL -ldl `sdl2-config --libs` `pkg-config --libs --cflags mpv` -pthread

CXXFLAGS = -fPIC -pedantic -Wall -Wextra -O3 -g
CXXFLAGS += -I./imgui -I./libs/gl3w `sdl2-config --cflags`
CFLAGS = $(CXXFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(EXE)

$(EXE): $(OBJS)
	$(CXX)  $(CXXFLAGS) -o $(EXE) $(OBJS) $(LIBS)

clean:
	rm $(EXE) $(OBJS)

install: all
	@mkdir -p /usr/local/bin
	@echo 'Installing executable (moov) to /usr/local/bin.'
	@cp -f moov /usr/local/bin
	@chmod 755 /usr/local/bin/moov
	@echo 'Installing pidgin adapter script (moovpidgin) to /usr/local/bin.'
	@cp -f pidgin_adapter.py /usr/local/bin/moovpidgin
	@chmod 755 /usr/local/bin/moovpidgin

uninstall: all
	@echo 'Removing executable (moov) from /usr/local/bin.'
	@rm -f /usr/local/bin/moov
	@echo 'Removing pidgin adapter script (moovpidgin) from /usr/local/bin.'
	@rm -f /usr/local/bin/moovpidgin
