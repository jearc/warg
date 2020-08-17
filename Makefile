OBJS = main.o mpvh.o util.o
OBJS += ./imgui/imgui_impl_sdl.o ./imgui/imgui.o ./imgui/imgui_draw.o
OBJS += ./imgui/imgui_impl_opengl3.o ./imgui/imgui_widgets.o
CFLAGS = -fPIC -pedantic -Wall -Wextra -Ofast -ffast-math
LIBS = -lGL -ldl -lSDL2 -lmpv -lGLEW -lGLU

all: moov

moov: $(OBJS) moov.h
	$(CXX) $(CFLAGS) -o moov $(OBJS) $(LIBS)

clean:
	rm moov $(OBJS)
	
test: all
	@./test.py

install: all
	@mkdir -p /usr/local/bin
	@echo 'Installing executable (moov) to /usr/local/bin.'
	@cp -f moov /usr/local/bin
	@chmod 755 /usr/local/bin/moov
	@echo 'Installing pidgin adapter script (moovpidgin) to /usr/local/bin.'
	@cp -f moovpidgin.py /usr/local/bin/moovpidgin
	@chmod 755 /usr/local/bin/moovpidgin
	@mkdir -p /usr/local/share/moov
	@echo 'Installing systemd user unit for pidgin adapter script.'
	@cp -f moovpidgin.service /etc/systemd/user

uninstall: all
	@echo 'Removing executable (moov) from /usr/local/bin.'
	@rm -f /usr/local/bin/moov
	@echo 'Removing pidgin adapter script (moovpidgin) from /usr/local/bin.'
	@rm -f /usr/local/bin/moovpidgin
	@echo 'Removing moov directory from /usr/local/share.'
	@rm -rf /usr/local/share/moov
	@echo 'Removing systemd user unit for pidgin adapter script.'
	@rm -f /etc/systemd/user/moovpidgin.service
