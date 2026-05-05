# Convenience wrappers around the CMake build.
# (The build itself is driven by CMakeLists.txt -- this Makefile only
# adds the `demo`, `bench`, and `test` shortcuts mentioned in the
# Project Guideline 2025.)

BUILD ?= build

.PHONY: all build demo bench test plot clean

all: build

build:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD) -j

demo: build
	bash demo/cli_demo.sh

bench: build
	bash experiments/benchmark.sh

plot:
	python3 visualization/plot_results.py

test: build
	bash tests/test_algorithms.sh

clean:
	rm -rf $(BUILD)
