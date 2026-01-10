# Part C: Bakery Simulation

## How to Run

**Compile:**
```bash
gcc -o cook main.c bakery.c queue.c -lpthread
```

**Run:**
```bash
./cook < input.txt
```
Or type input directly:

```bash
./cook
10 Customer 1
11 Customer 2
<EOF>
```

**Input Format:**
```
<timestamp> Customer <id>
<EOF>
```

**Clean:**
```bash
rm -f cook
```

## Assumptions

1. **Chef action timing**: When a customer requests cake at time `t`, a chef can only START baking at time `t+1` (1 second later). Similarly, when a customer pays at time `t`, a chef can only START accepting payment at time `t+1`. This is consistent with Example 1 in the project specification.

2. **Chef flexibility**: When a chef cannot acquire the cash register (because another chef is using it), the chef will attempt to bake a cake if requests are available, instead of waiting idle.

3. **Capacity enforcement**: The bakery enforces a maximum of 25 customers AT ANY INSTANT. If customers leave, new customers can enter even if more than 25 customers have visited in total throughout the simulation.

4. **Customer rejection**: If a customer arrives when the bakery is at full capacity (25 customers), they are immediately rejected and do not enter. There is no waiting queue outside the bakery. (As per Doubt Q&A #12)

## Important Note on Non-Determinism

Due to multithreading and thread scheduling:
- The exact order of events at the same timestamp may vary between runs
- When multiple customers arrive simultaneously, the order in which they enter/are processed is non-deterministic
- Among customers who entered at the same time and are waiting to sit, any ordering is valid (FIFO only applies to customers who entered at different times)
- Multiple valid outputs exist for the same input