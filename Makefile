SRC = main.cpp mpvh.cpp cmd.cpp chat.cpp util.cpp ui.cpp
SRC += ./imgui/imgui_impl_sdl_gl3.cpp ./imgui/imgui.cpp ./imgui/imgui_draw.cpp
CFLAGS = -fPIC -pedantic -Wall -Wextra -Ofast -ffast-math
LIBS = -lGL -ldl -lSDL2 -lmpv -lGLEW -lGLU -lfreetype

all: moov

moov: $(SRC) moov.h ui.h data.s meme.vert meme.frag
	$(CXX) $(CFLAGS) -o moov data.s $(SRC) $(LIBS) -I/usr/include/freetype2

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
	@cp -f pidgin_adapter.py /usr/local/bin/moovpidgin
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
