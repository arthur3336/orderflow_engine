// Simple trading example demonstrating basic OrderBook usage
// Compile: g++ -std=c++17 -I../include simple_trading.cpp ../src/OrderBook.cpp -o simple_trading

#include "orderbook/Orderbook.hpp"
#include "orderbook/Types.hpp"
#include <iostream>
#include <iomanip>

void printTrade(const Trade& trade) {
    std::cout << "Trade executed: "
              << trade.quantity << " shares @ "
              << priceToString(trade.price)
              << " (Buy #" << trade.buyOrderId
              << " x Sell #" << trade.sellOrderId << ")"
              << std::endl;
}

void printMarketData(const OrderBook& book) {
    auto bestBid = book.getBestBid();
    auto bestAsk = book.getBestAsk();
    auto spread = book.getSpread();
    auto mid = book.getMidPrice();

    std::cout << "\nMarket Data:" << std::endl;
    std::cout << "  Best Bid: "
              << (bestBid ? priceToString(*bestBid) : "N/A") << std::endl;
    std::cout << "  Best Ask: "
              << (bestAsk ? priceToString(*bestAsk) : "N/A") << std::endl;
    std::cout << "  Spread:   "
              << (spread ? priceToString(*spread) : "N/A") << std::endl;
    std::cout << "  Mid:      "
              << (mid ? priceToString(*mid) : "N/A") << std::endl;
}

int main() {
    OrderBook book;
    OrderId nextId = 1;

    std::cout << "OrderFlow Simple Trading Example\n";
    std::cout << "=================================\n\n";

    // Add some initial sell orders (asks)
    std::cout << "Adding sell orders...\n";
    book.addOrder(Order{nextId++, 10100, 50, Side::SELL, now()});   // 50 @ $101.00
    book.addOrder(Order{nextId++, 10150, 75, Side::SELL, now()});   // 75 @ $101.50
    book.addOrder(Order{nextId++, 10200, 100, Side::SELL, now()});  // 100 @ $102.00

    // Add some initial buy orders (bids)
    std::cout << "Adding buy orders...\n";
    book.addOrder(Order{nextId++, 10050, 60, Side::BUY, now()});    // 60 @ $100.50
    book.addOrder(Order{nextId++, 10000, 80, Side::BUY, now()});    // 80 @ $100.00
    book.addOrder(Order{nextId++, 9950, 100, Side::BUY, now()});    // 100 @ $99.50

    printMarketData(book);

    // Execute a market buy order that crosses the spread
    std::cout << "\n\nPlacing aggressive buy order (100 @ $101.50)...\n";
    auto trades = book.addOrder(Order{nextId++, 10150, 100, Side::BUY, now()});

    for (const auto& trade : trades) {
        printTrade(trade);
    }

    printMarketData(book);

    // Execute a market sell order
    std::cout << "\n\nPlacing aggressive sell order (70 @ $100.00)...\n";
    trades = book.addOrder(Order{nextId++, 10000, 70, Side::SELL, now()});

    for (const auto& trade : trades) {
        printTrade(trade);
    }

    printMarketData(book);

    // Cancel an order
    std::cout << "\n\nCancelling order #6 (100 @ $99.50)...\n";
    if (book.cancelOrder(6)) {
        std::cout << "Order cancelled successfully\n";
    } else {
        std::cout << "Order not found\n";
    }

    printMarketData(book);

    // Display final order book
    std::cout << "\n\nFinal Order Book:\n";
    std::cout << "=================\n";
    book.print();

    return 0;
}
