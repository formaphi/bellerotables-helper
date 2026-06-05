IMAGE = iptables-test
CONTAINER = iptables-dev

up:
	docker build -t $(IMAGE) . && docker run -d --name $(CONTAINER) --cap-add NET_ADMIN -v $(CURDIR):/src $(IMAGE)

down:
	docker rm -f $(CONTAINER)

enter:
	docker exec -it $(CONTAINER) bash

rebuild: down up

build:
	docker exec $(CONTAINER) bash -c "cmake -B /build -S /src -DCMAKE_BUILD_TYPE=Debug && cmake --build /build"

.PHONY: up down enter rebuild build
