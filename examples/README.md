# OrderFlow Examples

This directory contains example code demonstrating how to integrate and use the OrderFlow library.

## simple_trading.cpp

Basic example showing:
- Creating an order book
- Adding buy and sell orders
- Executing trades that cross the spread
- Cancelling orders
- Querying market data
- Displaying the order book

### Compile and Run

```bash
cd examples
g++ -std=c++17 -I../include simple_trading.cpp ../src/OrderBook.cpp -o simple_trading
./simple_trading
```

### Expected Output

The example will:
1. Add several buy and sell orders
2. Display initial market data (bid, ask, spread, mid-price)
3. Execute an aggressive buy order that matches existing sells
4. Execute an aggressive sell order that matches existing buys
5. Cancel an order
6. Display the final order book state

## Integration Guide

To integrate OrderFlow into your project:

### Include Headers

```cpp
#include "orderbook/Orderbook.hpp"
#include "orderbook/Types.hpp"
```

### Link Source

Add to your build:
- `OrderBook.cpp`
- Include path: `include/`

### Basic Usage Pattern

```cpp
// Create order book
OrderBook book;

// Generate unique order IDs
OrderId nextId = 1;

// Add orders and capture trades
auto trades = book.addOrder(Order{
    nextId++,           // Unique ID
    10050,             // Price (in cents, so $100.50)
    100,               // Quantity
    Side::BUY,         // BUY or SELL
    now()              // Timestamp
});

// Process trades
for (const auto& trade : trades) {
    // Handle executed trade
}

// Cancel orders
book.cancelOrder(orderId);

// Query market data
auto bestBid = book.getBestBid();
auto bestAsk = book.getBestAsk();
```

## Advanced Examples

Future examples could include:
- Market making bot
- Limit order placement strategy
- Market data analysis
- Integration with price feeds
- Multi-threaded order processing
