#############
# APPLICATION
#############
SRC := src/main.cpp
EXE := build/$(basename $(notdir $(SRC)))

default-target: $(EXE)

INC := game-libs mg_test
what-INC: ; @echo $(INC)

CXXFLAGS_BASE := -std=c++20 -Wall -Wextra -Wpedantic
CXXFLAGS_INC := $(foreach DIR, $(INC), -I$(DIR))
what-Idir: ; @echo $(CXXFLAGS_INC)
CXXFLAGS_SDL := `pkg-config --cflags sdl2`
CXXFLAGS_TTF := `pkg-config --cflags SDL2_ttf`
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_INC) $(CXXFLAGS_SDL) $(CXXFLAGS_TTF)
LDLIBS_SDL := `pkg-config --libs sdl2`
LDLIBS_TTF := `pkg-config --libs SDL2_ttf`
LDLIBS := $(LDLIBS_SDL) $(LDLIBS_TTF)

############
# UNIT TESTS
############
RUN_TESTS := build-tests/run-tests
TESTS := src/tests.cpp

test: $(RUN_TESTS)

build-tests:
	mkdir -p build-tests

.PHONY: $(RUN_TESTS)
$(RUN_TESTS): $(TESTS) | build-tests
	@$(CXX) $(CXXFLAGS) $< -o $@
	$(RUN_TESTS)

build:
	@mkdir -p build

$(EXE): $(SRC) | build
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

HEADER_LIST := build/$(basename $(notdir $(SRC))).d
what-HEADER_LIST: ; @echo $(HEADER_LIST)

.PHONY: $(HEADER_LIST)
$(HEADER_LIST): $(SRC)
	$(CXX) $(CXXFLAGS) -M $^ -MF $@

build-tags:
	@mkdir -p build-tags

build-tags/ctags-dlist: src/ctags-dlist.cpp | build-tags
	$(CXX) $(CXXFLAGS_BASE) $^ -o $@

.PHONY: tags
tags: $(HEADER_LIST) build-tags/ctags-dlist
	build-tags/ctags-dlist $(HEADER_LIST)
	ctags --c-kinds=+p+x --extra=+q -L build-tags/headers.txt
	ctags --c-kinds=+p+x+l --extra=+q -a $(SRC)

.PHONY: what
what:
	@echo
	@echo --- My make variables ---
	@echo
	@echo "CXX          : "$(CXX)
	@echo "CXXFLAGS     : "$(CXXFLAGS)
	@echo "SRC          : "$(SRC)
	@echo "EXE          : "$(EXE)
	@echo "HEADER_LIST  : "$(HEADER_LIST)
	@echo "INC          : "$(INC)

.PHONY: how
how:
	@echo
	@echo --- Build and Run ---
	@echo
	@echo "             Vim shortcut    Vim command line (approximate)"
	@echo "             ------------    ------------------------------"
	@echo "Build        ;<Space>        :make -B  <if(error)> :copen"
	@echo "Run          ;r<Space>       :!./build/main"
	@echo "Run in Vim   ;w<Space>       :!./build/main <args> &"
	@echo "Make tags    ;t<Space>       :make tags"
	@echo "Run tests                    :make test"

