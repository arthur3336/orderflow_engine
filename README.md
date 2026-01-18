# OrderFlow

A high-performance limit order book matching engine written in C++17. Implements price-time priority matching with fast order cancellation and comprehensive market data tracking.

## Performance

- **3.5M+ orders/second** (optimized build)
- **920K orders/second** (unoptimized)
- **Sub-microsecond latency** per order on modern hardware

## Features

- Price-time priority matching algorithm
- Fast O(1) order cancellation via indexed lookups
- Real-time market data snapshots (bid, ask, spread, mid-price)
- Historical price tracking with CSV export
- Three executables: demo, simulation, and benchmark
- Integer-based pricing to avoid floating-point precision issues

## Quick Start

### Build

```bash
mkdir build && cd build
cmake ..
make
```

### Run Demo

```bash
./orderbook
```

Executes sample trades and displays the final order book state.

### Run Simulation

```bash
./simulation
```

Generates random orders continuously with live market updates. Press Ctrl+C to stop.

### Run Benchmark

```bash
./benchmark
```

Processes 1 million orders and reports throughput metrics.

## Architecture

### Core Components

**OrderBook** - Main matching engine managing bids and asks

**PriceLevel** - Maintains FIFO queue of orders at a specific price

**Order** - Represents a single buy or sell order with price, quantity, and timestamp

**PriceHistory** - Tracks market snapshots over time for analysis

### Data Structures

```
Bids: std::map<Price, PriceLevel, std::greater<>>  // Descending order
Asks: std::map<Price, PriceLevel>                   // Ascending order
Index: std::unordered_map<OrderId, OrderLocation>   // Fast cancellation
```

Bids are sorted highest-to-lowest, asks lowest-to-highest. Orders within each price level are stored in a linked list to maintain FIFO ordering while allowing O(1) removal.

### Matching Algorithm

Orders are matched using price-time priority:

- Buy orders match against the lowest available sell prices
- Sell orders match against the highest available buy prices
- Orders at the same price level execute in time order (FIFO)
- Partial fills are supported
- Unmatched quantity remains in the order book

## API Reference

### OrderBook Class

#### Adding Orders

```cpp
std::vector<Trade> addOrder(const Order& order)
```

Attempts to match the order against existing orders. Returns all executed trades.

#### Cancelling Orders

```cpp
bool cancelOrder(OrderId id)
```

Removes an order from the book. Returns true if successful.

#### Market Data

```cpp
std::optional<Price> getBestBid()    // Highest buy price
std::optional<Price> getBestAsk()    // Lowest sell price
std::optional<Price> getSpread()     // Ask - Bid
std::optional<Price> getMidPrice()   // (Bid + Ask) / 2
Price getLastTradePrice()
Quantity getLastTradeQty()
```

#### Snapshots

```cpp
PriceData getSnapshot()
```

Returns current market state with timestamp, bid, ask, mid-price, spread, and last trade.

### Order Structure

```cpp
struct Order {
    OrderId id;
    Price price;        // Price in cents (e.g., 10050 = $100.50)
    Quantity quantity;  // Number of shares
    Side side;          // BUY or SELL
    Timestamp timestamp;
};
```

### Trade Structure

```cpp
struct Trade {
    OrderId buyOrderId;
    OrderId sellOrderId;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};
```

## Usage Example

```cpp
#include "orderbook/Orderbook.hpp"
#include "orderbook/Types.hpp"

int main() {
    OrderBook book;

    // Add a sell order: 100 shares @ $100.50
    Order sellOrder{1, 10050, 100, Side::SELL, now()};
    auto trades = book.addOrder(sellOrder);

    // Add a buy order: 50 shares @ $101.00
    Order buyOrder{2, 10100, 50, Side::BUY, now()};
    trades = book.addOrder(buyOrder);

    // trades vector now contains the executed trade
    for (const auto& trade : trades) {
        std::cout << "Traded " << trade.quantity
                  << " shares @ " << priceToString(trade.price)
                  << std::endl;
    }

    // Query market data
    auto bestBid = book.getBestBid();
    auto bestAsk = book.getBestAsk();

    if (bestBid && bestAsk) {
        std::cout << "Bid: " << priceToString(*bestBid) << std::endl;
        std::cout << "Ask: " << priceToString(*bestAsk) << std::endl;
    }

    return 0;
}
```

## Implementation Details

### Price Storage

Prices are stored as integers with a scale factor of 100 to avoid floating-point precision issues. For example, $100.50 is stored as 10050.

Use the `priceToString()` utility function to convert internal prices to human-readable format.

### Order Cancellation

Fast cancellation is achieved through an index that maps order IDs to their location in the order book. This allows O(1) lookup and removal without searching through price levels.

### Memory Management

The order book uses standard library containers with automatic memory management. Memory usage scales linearly with the number of outstanding orders.

### Thread Safety

This implementation is not thread-safe. For concurrent access, wrap operations in appropriate synchronization primitives.

## CSV Export

Both the demo and simulation executables export market data to CSV files:

- `price_history.csv` - Demo output
- `simulation_history.csv` - Simulation output

CSV format:
```
timestamp_ns,best_bid,best_ask,mid_price,spread,last_trade_price,last_trade_qty
```

Timestamps are in nanoseconds relative to the start time.

## Requirements

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16 or higher

## License

See LICENSE file for details.

## Contributing

Contributions are welcome. Please ensure code follows the existing style and passes all tests before submitting pull requests.
