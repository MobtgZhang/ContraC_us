.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

BUILD_DIR ?= build-make
SDL ?= 2
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2

ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must not be empty)
endif
ifeq ($(abspath $(BUILD_DIR)),$(CURDIR))
$(error BUILD_DIR must not be the source directory)
endif

ifeq ($(SDL),2)
OUTPUT_DIR := $(BUILD_DIR)
SDL_PACKAGES := 'sdl2 >= 2.0.10' 'SDL2_mixer >= 2.0.2'
SDL_DUMMY_DRIVERS := SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
else ifeq ($(SDL),3)
OUTPUT_DIR := $(BUILD_DIR)/sdl3
SDL_PACKAGES := 'sdl3 >= 3.2.0'
SDL_DUMMY_DRIVERS := SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
else
$(error SDL must be 2 or 3)
endif

SDL_CFLAGS = $(shell $(PKG_CONFIG) --cflags $(SDL_PACKAGES))
SDL_LIBS = $(shell $(PKG_CONFIG) --libs $(SDL_PACKAGES))

GAME := $(OUTPUT_DIR)/contra
SMOKE := $(OUTPUT_DIR)/contra_smoke
OBJ_DIR := $(OUTPUT_DIR)/obj
ASSET_DIR := $(OUTPUT_DIR)/resource

GAME_SOURCES := $(filter-out src/main.c,$(wildcard src/*.c))
GAME_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(GAME_SOURCES))
MAIN_OBJECT := $(OBJ_DIR)/src/main.o
SMOKE_OBJECT := $(OBJ_DIR)/tests/smoke.o
OBJECTS := $(GAME_OBJECTS) $(MAIN_OBJECT) $(SMOKE_OBJECT)
DEPFILES := $(OBJECTS:.o=.d)

SFX := $(patsubst resource/%,$(ASSET_DIR)/%,$(wildcard resource/sfx/*.wav))
BGM_TRACKS := Contra_00.raw Contra_01.raw Contra_02.raw Contra_04.raw Contra_05.raw \
              Contra_07.raw Contra_boss1.raw Contra_boss2.raw Contra_boss3.raw newboss.raw
BGM := $(addprefix $(ASSET_DIR)/bgm/,$(BGM_TRACKS))
ASSETS := $(ASSET_DIR)/contra.ini $(ASSET_DIR)/gfx.dat $(SFX) $(BGM)

.PHONY: all run test clean check-deps
all: $(GAME) $(ASSETS)

run: all
	"$(GAME)"

test: all $(SMOKE)
	"$(GAME)" --help
	@set -e; for scenario in input audio 1 2 3 4 5 6 7 8 ending; do \
		printf 'Testing %s\n' "$$scenario"; \
		$(SDL_DUMMY_DRIVERS) "$(SMOKE)" "$(ASSET_DIR)" "$$scenario"; \
	done

check-deps:
	@$(PKG_CONFIG) --exists $(SDL_PACKAGES) || { \
		printf '%s\n' "Install pkg-config and development packages for $(SDL_PACKAGES)." >&2; \
		exit 1; \
	}

$(GAME): $(MAIN_OBJECT) $(GAME_OBJECTS)
	$(CC) $(LDFLAGS) -o "$@" $^ $(SDL_LIBS) $(LDLIBS)

$(SMOKE): $(SMOKE_OBJECT) $(GAME_OBJECTS)
	$(CC) $(LDFLAGS) -o "$@" $^ $(SDL_LIBS) $(LDLIBS)

$(OBJ_DIR)/%.o: %.c Makefile | check-deps
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) -DCONTRA_SDL=$(SDL) -Isrc $(SDL_CFLAGS) $(CFLAGS) -std=c17 -MMD -MP -c "$<" -o "$@"

$(ASSET_DIR)/contra.ini: contra.ini
	@mkdir -p "$(@D)"
	cp "$<" "$@"

$(ASSET_DIR)/%: resource/%
	@mkdir -p "$(@D)"
	cp "$<" "$@"

$(BGM): $(ASSET_DIR)/bgm/%.raw: resource/BGMS.dat
	@mkdir -p "$(@D)"
	unzip -p "$<" "$(@F)" > "$@"

clean:
	$(RM) -- $(GAME) $(SMOKE) $(OBJECTS) $(DEPFILES) $(ASSETS)
	@set -e; for dir in "$(OBJ_DIR)/src" "$(OBJ_DIR)/tests" "$(OBJ_DIR)" \
	    "$(ASSET_DIR)/bgm" "$(ASSET_DIR)/sfx" "$(ASSET_DIR)" "$(OUTPUT_DIR)"; do \
		if test -d "$$dir"; then rmdir --ignore-fail-on-non-empty -- "$$dir"; fi; \
	done

-include $(DEPFILES)
