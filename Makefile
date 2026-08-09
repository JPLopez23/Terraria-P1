# ============================================================
# Terraria Forge — Proyecto #1, Computación Paralela y Distribuida
# Compila con: make        (Linux/macOS)
#              mingw32-make (Windows + MinGW-W64)
# ============================================================

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fopenmp -march=native
LDFLAGS  = -fopenmp

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)

# Todo .o se recompila si cambia cualquier header: son pocos archivos y
# evita el error clásico de medir un binario que no incluye tu último cambio.
HDR = $(wildcard src/*.hpp)

ifeq ($(OS),Windows_NT)
    SDL_DIR  = third_party/SDL2/x86_64-w64-mingw32
    # GCC 8.1 de MinGW no ensambla los registros AVX-512 (bug .seh_savexmm):
    # se desactiva esa extensión; el resto de -march=native queda activo.
    CXXFLAGS += -mno-avx512f -I$(SDL_DIR)/include -DSDL_MAIN_HANDLED
    # Runtime de GCC estático: evita mezclar los DLL de otro MinGW que
    # esté antes en el PATH (p. ej. el de Git, con ABI SEH incompatible).
    LDFLAGS  += -static-libgcc -static-libstdc++ -Wl,-Bstatic,-lgomp,-lwinpthread -Wl,-Bdynamic
    LDLIBS   = -L$(SDL_DIR)/lib -lSDL2
    EXE      = terraria-forge.exe
    DLL      = SDL2.dll
all: $(EXE) $(DLL)
# Funciona tanto si make usa sh (Git Bash) como cmd.exe:
$(DLL): $(SDL_DIR)/bin/SDL2.dll
	cp -f $< $@ || copy /Y "$(subst /,\,$<)" $@
else
    CXXFLAGS += $(shell sdl2-config --cflags) -DSDL_MAIN_HANDLED
    LDLIBS   = $(shell sdl2-config --libs)
    EXE      = terraria-forge
all: $(EXE)
endif

$(EXE): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp $(HDR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(EXE)

# rm -f funciona con el sh de MSYS/Git Bash y en Linux/macOS por igual;
# el "|| true" evita que make falle si algún archivo ya no existe.
clean:
	-rm -f $(OBJ) $(EXE) || true

.PHONY: all run clean
