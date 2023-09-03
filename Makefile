SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

TARGET := libtermbuffer.a
TEST := termbuftest 

INCLUDES := -Iapi
CFLAGS := $(INCLUDES) -ggdb

all: $(TARGET)
	$(info OBJ: $(OBJ))

$(TARGET): $(OBJ)
	@echo $@ $^
	ar rcs $@ $^

build/%.o: src/%.c
	gcc -c -o $@ $< $(CFLAGS)

$(TEST): $(TARGET)
	gcc -o $(TEST) test.c $(TARGET) $(CFLAGS)

run: $(TEST)
	./$(TEST)

debug: $(TEST)
	gdbserver :1234 ./$(TEST) &
	sleep 0.1
	gf2 -ex "target remote localhost:1234"

clean: 
	rm $(OBJ) $(TARGET) $(TEST)

.PHONY: $(TARGET) all test clean
