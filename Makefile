#NOTE(Hersh): Do not ask me how I know how to do this. It is dark and evil and
#light shall not dispell it

FILES=stb_compile_unit.cpp test-runner.cpp
OBJS=$(patsubst %, objs/%.o, $(FILES))
DEPS=$(patsubst %.o, %.d, $(OBJS))
CXXFLAGS=-Og -g -std=c++20
WARNFLAGS=-Wall
LIBS= jpeg
LIBFLAGS= $(patsubst %, -l%, $(LIBS))

all: test-bench

include $(DEPS)

info: stb_compile_unit.cpp stb_image_write.h
	echo $^

.PHONY: info all

test-bench:$(OBJS)
	g++ $(CXXFLAGS)  $^ -o $@ $(LIBFLAGS)
objs/%.cpp.o: %.cpp
	@mkdir -p $(basename $@)
	g++ $(CXXFLAGS) $(WARNFLAGS) -MMD -c $< -o $@

objs/stb_compile_unit.cpp.o: CXXFLAGS += -fno-strict-aliasing 