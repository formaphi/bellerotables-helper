IMAGE = iptables-test
CONTAINER = iptables-dev

up:
	docker build -t $(IMAGE) . && docker run -d --name $(CONTAINER) --cap-add NET_ADMIN $(IMAGE)

down:
	docker rm -f $(CONTAINER)

enter:
	docker exec -it $(CONTAINER) bash

rebuild: down up

.PHONY: up down enter rebuild
