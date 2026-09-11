# Makefile del simulador de scheduler MLFQ.
#
# Portable a macOS, Linux y Windows (MSYS2/MinGW-w64 o WSL): usa solo GNU make
# y $(wildcard), sin depender de `find` ni de utilidades específicas del shell.
#
# Objetivos:
#   make             compila el simulador en build/scheduler
#   make run         ejecuta el escenario del enunciado (genera results.csv)
#   make trace       ejecuta el escenario mostrando la traza ciclo a ciclo
#   make test        compila y ejecuta las pruebas unitarias
#   make sanitize    compila y corre las pruebas con AddressSanitizer + UBSan
#   make experiments corre las variantes usadas en el análisis
#   make clean       borra artefactos
#   make help        lista los objetivos

CC       ?= cc
CSTD     ?= -std=c11
WARNINGS ?= -Wall -Wextra -Werror -pedantic
OPTIMIZE ?= -O2 -g
CFLAGS   ?= $(CSTD) $(WARNINGS) $(OPTIMIZE)
CPPFLAGS  = -Iinclude -Isrc
BUILD_DIR = build

# En Windows los ejecutables necesitan la extensión .exe.
ifeq ($(OS),Windows_NT)
  EXE = .exe
else
  EXE =
endif

# El árbol de fuentes tiene exactamente dos niveles (src/<capa>/*.c), así que
# $(wildcard) basta y evita depender de `find`.
LIB_SOURCES  = $(filter-out src/cli/main.c,$(wildcard src/*/*.c))
MAIN_SOURCE  = src/cli/main.c
TEST_SOURCES = $(wildcard tests/*.c)

LIB_OBJECTS  = $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_SOURCES))
MAIN_OBJECT  = $(patsubst %.c,$(BUILD_DIR)/%.o,$(MAIN_SOURCE))
TEST_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SOURCES))

TARGET      = $(BUILD_DIR)/scheduler$(EXE)
TEST_TARGET = $(BUILD_DIR)/run_tests$(EXE)

.PHONY: all run trace test sanitize experiments clean help

all: $(TARGET)

$(TARGET): $(LIB_OBJECTS) $(MAIN_OBJECT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(LIB_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

trace: $(TARGET)
	./$(TARGET) --trace

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Detecta errores de memoria y comportamiento indefinido en tiempo de ejecución.
# Disponible con clang y gcc (no con MSVC ni con MinGW clásico).
# Compila en su PROPIO directorio para no mezclar objetos instrumentados con los
# de la compilación normal, que es una fuente clásica de fallos difusos.
sanitize:
	$(MAKE) test BUILD_DIR=$(BUILD_DIR)/asan \
		CFLAGS="$(CSTD) $(WARNINGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"

experiments: $(TARGET)
	./scripts/experimentos.sh

clean:
	rm -rf $(BUILD_DIR) results.csv docs/resultados

help:
	@echo "make            compila build/scheduler$(EXE)"
	@echo "make run        ejecuta el escenario del enunciado"
	@echo "make trace      ejecuta mostrando la traza ciclo a ciclo"
	@echo "make test       ejecuta las pruebas unitarias"
	@echo "make sanitize   pruebas con AddressSanitizer + UndefinedBehaviorSanitizer"
	@echo "make experiments regenera la tabla comparativa del analisis"
	@echo "make clean      borra artefactos"
