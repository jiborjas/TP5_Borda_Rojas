CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I../../firmware

TEST_PROTOCOL = test_protocol
TEST_PARSER = test_parser
TEST_APP = test_app

.PHONY: all clean test

all: $(TEST_PROTOCOL) $(TEST_PARSER) $(TEST_APP)

$(TEST_PROTOCOL): test_protocol.c ../../firmware/protocol/protocol.c
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_PARSER): test_parser.c ../../firmware/protocol/parser.c ../../firmware/protocol/protocol.c
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_APP): test_app.c ../../firmware/app/app.c ../../firmware/protocol/protocol.c
	$(CC) $(CFLAGS) -Wno-format-truncation -Istubs $^ -o $@

test: $(TEST_PROTOCOL) $(TEST_PARSER) $(TEST_APP)
	./$(TEST_PROTOCOL)
	./$(TEST_PARSER)
	./$(TEST_APP)

clean:
	rm -f $(TEST_PROTOCOL) $(TEST_PARSER) $(TEST_APP)
