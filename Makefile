# ServIO — lightweight HTTP/1.1 server (C++98)
# Build targets:
#   make            -> debug build with AddressSanitizer (default)
#   make release    -> optimized build, no sanitizer
#   make asan       -> explicit ASan build (alias of default)
#   make clean      -> remove object files / build dir
#   make fclean     -> remove all build artifacts including the binary
#   make re         -> fclean + all
#   make format     -> run clang-format -i over src/
#   make install    -> install $(NAME) into $(DESTDIR)$(PREFIX)/bin
#   make uninstall  -> remove the installed binary

NAME       := servio
VERSION    := 2.0.0-dev

# ---- Toolchain --------------------------------------------------------------

CXX        ?= c++
CXXSTD     ?= -std=c++98

# Strict warnings the project has always built with. -Wno-vla because the CGI
# code uses VLAs intentionally.
WARNFLAGS  := -Wall -Wextra -Werror

INCLUDES   := -Isrc -Isrc/core -Isrc/http -Isrc/utility

DEFINES    := -DPREFIX_FOLDER=\"$(CURDIR)\" -DSERVIO_VERSION=\"$(VERSION)\"

# Auto-generated header dependency files.
DEPFLAGS   := -MMD -MP

BASE_CXXFLAGS := $(CXXSTD) $(WARNFLAGS) $(INCLUDES) $(DEFINES) $(DEPFLAGS)

# Build mode (debug | release | asan). Default is debug+ASan to match the
# project history.
MODE ?= asan

ifeq ($(MODE),release)
    CXXFLAGS  := $(BASE_CXXFLAGS) -O2
    LDFLAGS   :=
    BUILD_DIR := build/release
else ifeq ($(MODE),debug)
    CXXFLAGS  := $(BASE_CXXFLAGS) -O0 -ggdb
    LDFLAGS   :=
    BUILD_DIR := build/debug
else
    # asan / default
    CXXFLAGS  := $(BASE_CXXFLAGS) -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS   := -fsanitize=address
    BUILD_DIR := build/asan
endif

# ---- Sources & objects ------------------------------------------------------

SRC_DIRS   := src src/core src/http src/utility
SRCS       := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.cpp))
OBJS       := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS       := $(OBJS:.o=.d)

# ---- Install paths ----------------------------------------------------------

DESTDIR   ?=
PREFIX    ?= /usr/local
BINDIR    := $(DESTDIR)$(PREFIX)/bin

# ---- Targets ----------------------------------------------------------------

.PHONY: all asan debug release clean fclean re format install uninstall help

all: $(NAME)

asan:    ; @$(MAKE) --no-print-directory MODE=asan    all
debug:   ; @$(MAKE) --no-print-directory MODE=debug   all
release: ; @$(MAKE) --no-print-directory MODE=release all

$(NAME): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)
	@printf '\033[1;32m  built\033[0m  %s  (mode=%s)\n' '$@' '$(MODE)'

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf build
	@printf '\033[1;33m  clean\033[0m  build/\n'

fclean: clean
	@rm -f $(NAME)
	@printf '\033[1;33m  clean\033[0m  $(NAME)\n'

re: fclean all

format:
	@command -v clang-format >/dev/null 2>&1 || { echo "clang-format not installed"; exit 1; }
	@find src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.tpp' \) \
	    -exec clang-format -i {} +
	@printf '\033[1;32m  format\033[0m run on src/\n'

install: $(NAME)
	@install -d $(BINDIR)
	@install -m 0755 $(NAME) $(BINDIR)/$(NAME)
	@printf '\033[1;32m  install\033[0m  %s\n' '$(BINDIR)/$(NAME)'

uninstall:
	@rm -f $(BINDIR)/$(NAME)
	@printf '\033[1;33m  uninstall\033[0m  %s\n' '$(BINDIR)/$(NAME)'

help:
	@sed -n '1,18p' Makefile

-include $(DEPS)
