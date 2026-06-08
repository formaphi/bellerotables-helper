IMAGE = iptables-test
CONTAINER = iptables-dev

up:
	docker build -t $(IMAGE) . && docker run -d --name $(CONTAINER) --cap-add NET_ADMIN -v $(CURDIR):/src $(IMAGE)

down:
	docker rm -f $(CONTAINER)

enter:
	docker exec -it $(CONTAINER) bash

rebuild: down up

build: build-debug

build-debug:
	docker exec $(CONTAINER) bash -c "\
	  conan install /src --profile /src/profiles/default.conan --build=missing --output-folder=/build && \
	  cmake -B /build -S /src -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=/build/conan_toolchain.cmake && \
	  cmake --build /build"

build-asan:
	docker exec $(CONTAINER) bash -c "\
	  conan install /src --profile /src/profiles/asan.conan --build=missing --output-folder=/build && \
	  cmake -B /build -S /src -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=/build/conan_toolchain.cmake && \
	  cmake --build /build"

build-tsan:
	docker exec $(CONTAINER) bash -c "\
	  conan install /src --profile /src/profiles/tsan.conan --build=missing --output-folder=/build && \
	  cmake -B /build -S /src -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=/build/conan_toolchain.cmake && \
	  cmake --build /build"

build-coverage:
	docker exec $(CONTAINER) bash -c "\
	  conan install /src --profile /src/profiles/gcov.conan --build=missing --output-folder=/build && \
	  cmake -B /build -S /src -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=/build/conan_toolchain.cmake && \
	  cmake --build /build && \
	  cd /build && /build/iptables-manager && \
	  gcovr --root /src --html --html-details -o /build/coverage.html"

build-valgrind:
	docker exec $(CONTAINER) bash -c "\
	  conan install /src --profile /src/profiles/default.conan --build=missing --output-folder=/build && \
	  cmake -B /build -S /src -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=/build/conan_toolchain.cmake && \
	  cmake --build /build && \
	  valgrind --leak-check=full --track-origins=yes /build/iptables-manager"

build-tidy:
	docker exec $(CONTAINER) bash -c "\
	  conan install /src --profile /src/profiles/default.conan --build=missing --output-folder=/build && \
	  cmake -B /build -S /src -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=/build/conan_toolchain.cmake \
	    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
	  clang-tidy -p /build /src/main.cpp"

build-format:
	docker exec $(CONTAINER) bash -c "\
	  find /src -name '*.cpp' -o -name '*.h' | grep -v end2end | \
	  xargs clang-format --style=file -i"

.PHONY: up down enter rebuild build build-debug build-asan build-tsan build-coverage build-valgrind build-tidy build-format
