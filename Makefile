EXE = moov
OBJS = main.o ./imgui/imgui_impl_sdl_gl3.o
OBJS += ./imgui/imgui.o ./imgui/imgui_demo.o ./imgui/imgui_draw.o
OBJS += ./libs/gl3w/GL/gl3w.o

LIBS = -lGL -ldl `sdl2-config --libs` `pkg-config --libs --cflags mpv` -pthread

CXXFLAGS = -fPIC -g
CXXFLAGS += -I./imgui -I./libs/gl3w `sdl2-config --cflags`
CFLAGS = $(CXXFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(EXE)

$(EXE): $(OBJS)
	$(CXX)  $(CXXFLAGS) -o $(EXE) $(OBJS) $(LIBS)

clean:
	rm $(EXE) $(OBJS)
