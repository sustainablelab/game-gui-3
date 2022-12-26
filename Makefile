SRC := src/main.cpp
EXE := build/$(basename $(notdir $(SRC)))
HEADER_LIST := build/$(basename $(notdir $(SRC))).d
INC := game-libs

CXXFLAGS_BASE := -std=c++20 -Wall -Wextra -Wpedantic
CXXFLAGS_INC := -I$(INC)
CXXFLAGS_SDL := `pkg-config --cflags sdl2`
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_INC) $(CXXFLAGS_SDL)
LDLIBS := `pkg-config --libs sdl2`

default-target: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

.PHONY: $(HEADER_LIST)
$(HEADER_LIST): $(SRC)
	$(CXX) $(CXXFLAGS) -M $^ -MF $@

build/ctags-dlist: ctags-dlist.cpp
	$(CXX) $(CXXFLAGS_BASE) $^ -o $@

.PHONY: tags
tags: $(HEADER_LIST) build/ctags-dlist
	build/ctags-dlist $(HEADER_LIST)
	ctags --c-kinds=+p+x -L headers.txt
	ctags --c-kinds=+p+x+l -a $(SRC)

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


