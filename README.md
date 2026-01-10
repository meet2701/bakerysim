# High-Performance Multi-Threaded Bakery Simulation

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Threading](https://img.shields.io/badge/threading-POSIX-green.svg)](https://en.wikipedia.org/wiki/POSIX_Threads)

## Overview

A discrete event simulation of a resource-constrained service system built in C using POSIX threads. This project implements fine-grained concurrency control, work-stealing scheduling, and comprehensive performance instrumentation to handle high-throughput workloads efficiently.

The system models a bakery with configurable capacity constraints, dynamic thread pools, and multiple synchronization queues, achieving **32,260 lock contentions** and **97.38% chef utilization** on 100-customer workloads with **5,394x real-time execution speed**.

---

## Key Features

- **Fine-grained locking strategy** - Critical sections optimized to minimize lock hold time
- **Work-stealing scheduler** - Non-blocking task distribution across worker threads
- **Configurable parallelism** - Dynamic thread pool scaling (1-32 workers)
- **Performance instrumentation** - Real-time tracking of 15+ concurrency metrics
- **Comprehensive test suite** - 6 datasets covering edge cases and stress scenarios

---

## Technical Architecture

### System Design

#### 1. **Critical Section Optimization**
- Minimized lock hold times by separating synchronization from I/O operations
- Moved expensive operations (printf, state updates) outside critical sections
- Achieved low contention rates through targeted mutex usage

#### 2. **Configurable Thread Pool**
- Dynamic worker thread allocation (1-32 threads) via command-line arguments
- Horizontal scaling to match workload characteristics
- Optimal configuration varies by arrival pattern and system resources

#### 3. **Performance Instrumentation**
- Real-time tracking of 15+ concurrency metrics
- Queue depth monitoring, lock contention analysis, utilization statistics
- Data-driven insights for system tuning

#### 4. **Work-Stealing Scheduler**
- Non-blocking task acquisition using `pthread_mutex_trylock()`
- Automatic fallback to alternative work queues when primary tasks unavailable
- Reduces idle time and maximizes CPU utilization

### System Components

#### 1. **Main Scheduler Thread**
- Manages global simulation clock
- Spawns customer threads based on arrival timestamps
- Broadcasts time ticks via `pthread_cond_broadcast()` for deterministic ordering

#### 2. **Worker Thread Pool (Chefs)**
- Configurable size (default: 4 threads)
- Priority-based task selection:
  1. **Payment Processing** (High Priority - requires cash register lock)
  2. **Baking** (Medium Priority - work stealing fallback)
- Persistent threads with conditional wait/signal coordination

#### 3. **Client Threads (Customers)**
- State machine traversal: `ARRIVED → ENTERED → SITTING → ORDERED → PAYING → LEFT`
- Per-customer synchronization primitives (mutex + condition variable)
- FIFO queue discipline enforced through strict ordering

### Concurrency Control Mechanisms

| Mechanism | Purpose | Optimization |
|-----------|---------|--------------|
| `bakery_state_mutex` | Protects queues and capacity counters | **Minimized hold time** - moved prints outside |
| `stats_mutex` | Guards performance metrics | **Separate lock** - prevents stats from blocking core logic |
| `cash_register_mutex` | Serializes payment processing | **Non-blocking trylock** - enables work stealing |
| `print_mutex` | Serializes stdout | **Outside critical sections** |
| Atomic operations | Lock-free counter updates | GCC built-ins (`__sync_fetch_and_add`) |

---

## Build & Execution

### Prerequisites

- **GCC** compiler (with C11 support)
- **POSIX-compliant OS** (Linux, macOS, WSL)
- **pthreads** library (usually included)

### Quick Start

```bash
# Clone and build
cd bakerysim
make

# Run with default configuration (4 chefs)
./bakery_sim < test_medium.txt

# Run with custom configuration
./bakery_sim -c 8 -b 50 -s 10 < test_large.txt
```

### Command-Line Options

```
Usage: ./bakery_sim [OPTIONS]

Options:
  -c <num>    Number of chef threads (default: 4, range: 1-32)
  -s <num>    Sofa capacity (default: 4)
  -b <num>    Bakery capacity (default: 25)
  -f          Fast mode (no sleep delays, for benchmarking)
  -h          Show help message

Input format:
  <time> Customer <id>
  ...
  <EOF>
```

### Makefile Targets

```bash
make              # Build optimized version
make debug        # Build with debug symbols
make test_all     # Run all test cases
make test_large   # Run 1000-customer stress test
make benchmark    # Compare performance across chef counts
make clean        # Remove build artifacts
```

---

## Performance Benchmarks

### Test Dataset Characteristics

| Dataset | Customers | Pattern | Purpose |
|---------|-----------|---------|---------|
| `test_small.txt` | 10 | Gradual arrivals | Basic functionality |
| `test_edge_burst.txt` | 30 | All at t=0 | Capacity stress |
| `test_edge_capacity.txt` | 30 | Rapid bursts | Queue buildup |
| `test_edge_sparse.txt` | 15 | Wide intervals | Low contention |
| `test_medium.txt` | 100 | Mixed pattern | Standard workload |
| `test_large.txt` | 1000 | Realistic mix | Stress test |

### Scalability Analysis (test_medium.txt)

| Configuration | Customers Served | Service Rate | Chef Utilization | Lock Contentions | Wall-Clock Time |
|---------------|-----------------|--------------|------------------|------------------|-----------------|
| 4 chefs, 25 capacity | 65 | 65.00% | 97.38% | 32,260 | 0.025s |
| 8 chefs, 50 capacity | 89 | 89.00% | 79.46% | 87,591 | 0.035s |
| 16 chefs, 150 capacity (large test) | 283 | 28.30% | 66.71% | 248,828 | 0.153s |

**Key Insight:** Increasing parallelism reduces per-thread utilization but improves overall throughput when capacity constraints are lifted.

---

## Implementation Details

### 1. Critical Section Reduction
```c
// Split critical sections to minimize lock hold time
pthread_mutex_lock(&bakery_state_mutex);
customers_in_bakery++;  // Only protect shared state
pthread_mutex_unlock(&bakery_state_mutex);

// Perform I/O outside lock
self->state = ENTERED;
printf("%d Customer %d enters\n", ...);

pthread_mutex_lock(&bakery_state_mutex);
enqueue(standing_queue, self);
pthread_mutex_unlock(&bakery_state_mutex);
```

### 2. Lock Hierarchy Separation
- Dedicated `stats_mutex` isolated from `bakery_state_mutex`
- Statistics updates don't block core simulation operations
- Prevents unnecessary serialization of independent operations

### 3. Non-Blocking Work Stealing
```c
if (pthread_mutex_trylock(&cash_register_mutex) == 0) {
    // Process payment
} else {
    // Register busy - work on baking queue instead
    if (!is_empty(cake_request_queue)) {
        process_baking_task();
    }
}
```

### 4. Dynamic Resource Management
- Chef threads allocated dynamically based on `-c` flag
- Supports up to 10,000 concurrent customers
- Configurable capacity parameters for different workload profiles

---

## Design Considerations

### Concurrency Patterns
- Fine-grained locking to reduce contention hotspots
- Conditional variables for efficient thread coordination
- Atomic operations for lock-free counters

### Performance Characteristics
- Demonstrates Amdahl's Law effects with increasing parallelism
- Non-linear scaling due to synchronization overhead
- Optimal thread count varies by workload characteristics

### Scalability
- Horizontal scaling via configurable thread pools
- Memory-efficient queue structures
- Low overhead synchronization primitives

---

## Potential Enhancements

1. **Lock-Free Queues:** Replace mutex-protected queues with CAS-based structures
2. **NUMA Awareness:** Thread-local caching and CPU pinning for multi-socket systems
3. **Adaptive Thread Pool:** Dynamic worker count based on queue depth
4. **Batch Processing:** Group operations to reduce context switching overhead
5. **Cache-Line Alignment:** Prevent false sharing in hot data structures

---

## Repository Structure

```
bakerysim/
├── main.c                  # Main scheduler and CLI
├── bakery.c                # Core simulation logic
├── bakery.h                # Interface definitions
├── queue.c                 # Linked-list queue implementation
├── queue.h                 # Queue interface
├── Makefile                # Build automation
├── README.md               # Documentation
├── BENCHMARKS.md           # Detailed performance analysis
├── test_small.txt          # 10 customer test
├── test_edge_*.txt         # Edge case tests
├── test_medium.txt         # 100 customer benchmark
└── test_large.txt          # 1000 customer stress test
```

---

## Author

**GitHub:** [@meet2701](https://github.com/meet2701/bakerysim)

A multi-threaded discrete event simulation demonstrating concurrency control, performance optimization, and scalable system design in C.
