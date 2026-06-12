
CC     = gcc
CFLAGS = -Wall -Wextra -g
LIBS   = -pthread

TARGET = airport_sim
SRCS   = main.c belt.c ground_crew.c sorter.c logger.c
OBJS   = $(SRCS:.c=.o)

# Default: build the binary
all: $(TARGET)
	@echo ""
	@echo "  Build successful!"
	@echo "  Run with:  ./airport_sim"
	@echo ""

# Link
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Compile each .c → .o
%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

# Build and run
run: all
	./$(TARGET)

# Remove build files
clean:
	rm -f $(OBJS) $(TARGET) simulation.log
	@echo "  Cleaned."

.PHONY: all run clean
