# Makefile for Bakery Simulation
# Multi-threaded discrete event simulation using pthreads

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
CFLAGS_DEBUG = -Wall -Wextra -g -pthread -DDEBUG
TARGET = bakery_sim
TARGET_DEBUG = bakery_sim_debug

SOURCES = main.c bakery.c queue.c
HEADERS = bakery.h queue.h
OBJECTS = $(SOURCES:.c=.o)

# Test files
TEST_FILES = test_small.txt test_edge_burst.txt test_edge_capacity.txt test_edge_sparse.txt test_medium.txt test_large.txt

.PHONY: all clean debug run test test_all help

# Default target
all: $(TARGET)

# Build optimized version
$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

# Build debug version
debug: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS_DEBUG) $(SOURCES) -o $(TARGET_DEBUG)
	@echo "Debug build complete: $(TARGET_DEBUG)"

# Run with default small test
run: $(TARGET)
	@echo "Running with test_small.txt..."
	@./$(TARGET) < test_small.txt

# Run all tests
test_all: $(TARGET)
	@echo "=========================================="
	@echo "Running Small Test (10 customers)..."
	@echo "=========================================="
	@./$(TARGET) < test_small.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"
	@echo ""
	@echo "=========================================="
	@echo "Running Edge Case: Burst (30 customers at t=0)..."
	@echo "=========================================="
	@./$(TARGET) < test_edge_burst.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"
	@echo ""
	@echo "=========================================="
	@echo "Running Edge Case: Capacity Test (30 customers)..."
	@echo "=========================================="
	@./$(TARGET) < test_edge_capacity.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"
	@echo ""
	@echo "=========================================="
	@echo "Running Edge Case: Sparse (15 customers, wide intervals)..."
	@echo "=========================================="
	@./$(TARGET) < test_edge_sparse.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"
	@echo ""
	@echo "=========================================="
	@echo "Running Medium Test (100 customers)..."
	@echo "=========================================="
	@./$(TARGET) < test_medium.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"

# Run large test
test_large: $(TARGET)
	@echo "=========================================="
	@echo "Running Large Test (1000 customers)..."
	@echo "=========================================="
	@./$(TARGET) < test_large.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"

# Run with custom configuration
test_config: $(TARGET)
	@echo "Testing with 2 chefs..."
	@./$(TARGET) -c 2 < test_medium.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"
	@echo ""
	@echo "Testing with 8 chefs..."
	@./$(TARGET) -c 8 < test_medium.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"
	@echo ""
	@echo "Testing with 16 chefs..."
	@./$(TARGET) -c 16 < test_medium.txt 2>&1 | grep -A 100 "PERFORMANCE STATISTICS"

# Benchmark different chef counts on large dataset
benchmark: $(TARGET)
	@echo "=========================================="
	@echo "PERFORMANCE BENCHMARK"
	@echo "Testing with different chef counts on large dataset"
	@echo "=========================================="
	@for chefs in 1 2 4 8 16; do \
		echo ""; \
		echo "--- Testing with $$chefs chef(s) ---"; \
		./$(TARGET) -c $$chefs < test_large.txt 2>&1 | grep -E "(Configuration:|Throughput:|Wall-Clock Time:|Chef Utilization:)"; \
	done

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET_DEBUG) $(OBJECTS)
	@echo "Clean complete"

# Help target
help:
	@echo "Bakery Simulation Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build optimized version"
	@echo "  make debug        - Build debug version with symbols"
	@echo "  make run          - Build and run with small test"
	@echo "  make test_all     - Run all test cases"
	@echo "  make test_large   - Run large test (1000 customers)"
	@echo "  make test_config  - Test with different configurations"
	@echo "  make benchmark    - Benchmark with different chef counts"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  ./$(TARGET) < test_small.txt"
	@echo "  ./$(TARGET) -c 8 < test_medium.txt"
	@echo "  ./$(TARGET) -c 16 -b 50 -s 8 < test_large.txt"
