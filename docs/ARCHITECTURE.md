# OrderFlow Architecture

This document describes the internal architecture and design decisions of the OrderFlow matching engine.

## Overview

OrderFlow is a limit order book implementation using price-time priority matching. The core design focuses on:

- Fast order matching and cancellation
- Correct execution semantics
- Efficient memory usage
- Clean, maintainable code

## Data Structure Design

### Order Book Organization

The order book maintains two separate price maps:

```
Bids (Buy Orders)
-----------------
std::map<Price, PriceLevel, std::greater<Price>>

Sorted descending: highest price first
Example: $101.00 > $100.50 > $100.00

Asks (Sell Orders)
------------------
std::map<Price, PriceLevel>

Sorted ascending: lowest price first
Example: $100.00 < $100.50 < $101.00
```

This structure allows O(log P) insertion where P is the number of price levels, and O(1) access to the best bid and ask using `map.begin()`.

### Price Level Structure

Each price level contains:

```cpp
struct PriceLevel {
    Quantity totalQuantity;      // Sum of all order quantities at this price
    std::list<Order> orders;     // FIFO queue of orders
};
```

The `std::list` provides:
- O(1) insertion at the back (new orders)
- O(1) removal from any position (when given an iterator)
- Automatic FIFO ordering preservation

### Order Index

Fast cancellation requires O(1) order lookup:

```cpp
std::unordered_map<OrderId, OrderLocation> orderIndex;

struct OrderLocation {
    Side side;                              // BUY or SELL
    Price price;                            // Which price level
    std::list<Order>::iterator iterator;    // Position in the list
};
```

This index maps order IDs to their exact location in the book, enabling direct access without searching.

## Matching Algorithm

### Price-Time Priority

Orders are matched using standard exchange rules:

1. **Price priority**: Better prices execute first
   - Buy orders: higher prices have priority
   - Sell orders: lower prices have priority

2. **Time priority**: At the same price, earlier orders execute first (FIFO)

### Matching Process

When a new order arrives:

```
1. Determine if it can match against the opposite side
   - Buy order: check if bid price >= best ask
   - Sell order: check if ask price <= best bid

2. While quantity remains and matches are possible:
   a. Get best opposing price level
   b. Match against orders in FIFO order
   c. Generate Trade records
   d. Update or remove filled orders
   e. Remove empty price levels

3. If quantity remains, add to the appropriate side of the book
```

### Partial Fills

Orders can partially fill:

```
Example:
- Order A: Sell 100 @ $100.00 (already in book)
- Order B: Buy 150 @ $100.00 (incoming)

Result:
- 100 shares trade @ $100.00
- Order A is fully filled (removed)
- Order B has 50 shares remaining (added to book as Buy 50 @ $100.00)
```

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Explanation |
|-----------|-----------|-------------|
| Add order (no match) | O(log P) | Map insertion at a price level |
| Add order (with match) | O(log P + M) | M = number of orders matched |
| Cancel order | O(log P) | Index lookup O(1) + map cleanup O(log P) |
| Get best bid/ask | O(1) | Direct map access |
| Get market data | O(1) | Cached values |

P = number of distinct price levels (typically 10-1000)
M = number of orders matched (typically 1-10)

### Space Complexity

- O(N) where N is the number of outstanding orders
- Each order appears in three places:
  1. In a price level's order list
  2. In the order index
  3. Referenced by an iterator in OrderLocation

### Benchmark Results

Run `./build/benchmark` to measure performance on your system. The benchmark processes 1 million random orders and reports throughput metrics.

Results will vary based on:
- CPU architecture and clock speed
- Compiler optimization flags
- System load and available memory
- Order distribution and matching rate

## Design Decisions

### Integer-Based Prices

Prices are stored as `int64_t` with a scale factor of 100:

```cpp
Price internalPrice = 10050;  // Represents $100.50
```

Rationale:
- Avoids floating-point precision errors
- Enables exact price comparisons
- Faster arithmetic operations
- Standard practice in financial systems

### Separate Bid and Ask Maps

Alternative considered: Single map with both bids and asks

Chosen approach:
- Two separate maps with different sort orders
- Bids descending, asks ascending

Benefits:
- Simpler logic (no need to reverse iterate)
- Clearer code
- Minimal memory overhead
- Faster best price access

### std::list for Order Storage

Alternative considered: std::deque or custom linked list

Chosen approach: std::list

Benefits:
- Iterator stability (iterators remain valid after insertions/deletions)
- O(1) removal from middle of container
- Standard library implementation is well-optimized
- No manual memory management

Tradeoff:
- Higher memory overhead per element than std::deque
- Worse cache locality
- Acceptable because order lists are typically short (1-100 orders per price)

### OrderLocation Caching

Stores both the price and iterator for each order:

```cpp
struct OrderLocation {
    Side side;
    Price price;
    std::list<Order>::iterator iterator;
};
```

Alternative considered: Only store price, search list on cancellation

Benefits of caching iterator:
- O(1) removal instead of O(N) where N = orders at that price
- Critical for high-frequency cancellation workloads
- Memory cost is minimal (one pointer per order)

## Matching Engine Internals

### Add Order Flow

```
OrderBook::addOrderToBook(Order order)
    |
    v
fillLimitOrder(order, trades)  // Attempt to match
    |
    +-- If BUY: match against asks (ascending price)
    |   |
    |   +-- For each ask price level <= bid price
    |       |
    |       +-- For each order in FIFO order
    |           |
    |           +-- Fill (partial or complete)
    |           +-- Record Trade
    |           +-- Update quantities
    |
    +-- If SELL: match against bids (descending price)
        |
        +-- For each bid price level >= ask price
            |
            +-- For each order in FIFO order
                |
                +-- Fill (partial or complete)
                +-- Record Trade
                +-- Update quantities
    |
    v
If quantity remains:
    |
    +-- Create new PriceLevel if needed
    +-- Add order to level's order list
    +-- Update orderIndex with location
    +-- Update totalQuantity
    |
    v
Return vector of trades
```

### Cancel Order Flow

```
OrderBook::cancelOrder(OrderId id)
    |
    v
Look up in orderIndex
    |
    +-- Not found? Return false
    |
    v
Get OrderLocation (side, price, iterator)
    |
    v
Get appropriate map (bids or asks)
    |
    v
Find price level
    |
    v
Erase order using cached iterator (O(1))
    |
    v
Update totalQuantity
    |
    v
If price level now empty:
    |
    +-- Remove price level from map
    |
    v
Remove from orderIndex
    |
    v
Return true
```

## Market Data Tracking

### Price History

The `PriceHistory` class maintains snapshots over time:

```cpp
std::deque<PriceData> snapshots;

struct PriceData {
    Timestamp timestamp;
    Price bestBid;
    Price bestAsk;
    Price midPrice;
    Price spread;
    Price lastTradePrice;
    Quantity lastTradeQty;
};
```

Uses `std::deque` for:
- O(1) append at back
- O(1) removal from front (for rolling window)
- Efficient memory layout for sequential access
- Configurable maximum size (default 10,000 snapshots)

### CSV Export

Format:
```
timestamp_ns,best_bid,best_ask,mid_price,spread,last_trade_price,last_trade_qty
```

Timestamps are nanoseconds from start time, allowing precise reconstruction of market events.

## Thread Safety

The current implementation is **not thread-safe**.

For concurrent access, consider:

1. **External locking**: Wrap OrderBook in a mutex
   - Simple to implement
   - Coarse-grained locking

2. **Lock-free data structures**: Use atomic operations
   - Complex to implement correctly
   - Best performance for high concurrency
   - Consider libraries like Boost.Lockfree

3. **Actor model**: Single-threaded event loop
   - No locking needed
   - Good for I/O bound workloads

## Extension Points

### Custom Order Types

Implement by extending the Order struct and modifying matching logic:

- Stop orders: Trigger when market reaches a price
- Iceberg orders: Hide partial quantity
- Fill-or-kill: Execute completely or cancel
- Immediate-or-cancel: Execute what's possible, cancel rest

### Persistence

Add serialization methods:
- Save order book state to disk
- Load from checkpoint
- Replay order stream for recovery

### Network Interface

Add network layer:
- FIX protocol support
- WebSocket market data feed
- REST API for order management
- Binary protocol for lowest latency

## Testing

### Current State

The project includes:
- `benchmark` executable for performance testing
- `simulation` executable for visual verification
- `orderbook` demo for functional verification

### Manual Testing

Test order matching correctness by running:
```bash
./build/orderbook
```

Test performance by running:
```bash
./build/benchmark
```

### Future Testing

Potential additions:
- Unit test framework
- Automated correctness tests
- Memory leak detection with valgrind
- Long-running stability tests

## Future Optimizations

### Cache Optimization

- Align PriceLevel to cache line boundaries
- Pool allocate Orders to reduce fragmentation
- Use flat_map for small order books

### Algorithmic

- Skip lists instead of std::map for better cache locality
- B+ trees for larger order books
- SIMD for batch order processing

### Profiling

Key hot paths to optimize:
- Map insertion/removal
- Iterator traversal during matching
- Trade vector allocations

Use tools:
- perf (Linux)
- Instruments (macOS)
- VTune (Intel)
