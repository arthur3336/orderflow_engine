/**
 * Real-time order book simulation
 *
 * Generates random buy/sell orders and displays price movement in real-time.
 * Press Ctrl+C to stop.
 */

#include <orderbook/Order.hpp>
#include <orderbook/Orderbook.hpp>
#include <orderbook/PriceHistory.hpp>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

using namespace orderbook;

// =============================================================================
// LESSON: C++ Random Number Generation
// =============================================================================
// The <random> library provides high-quality random numbers.
//
// std::random_device - gets entropy from hardware (slow, used once for seed)
// std::mt19937 - Mersenne Twister engine (fast, deterministic given seed)
// std::uniform_int_distribution - maps engine output to a range

std::random_device rd;      // Hardware entropy for seeding
std::mt19937 gen(rd());     // Fast pseudo-random generator

// Distributions - define what range of values we want
std::uniform_int_distribution<int> sideDist(0, 1);
std::uniform_int_distribution<Price> priceDist(9800, 10200);  // $98.00 - $102.00
std::uniform_int_distribution<Quantity> qtyDist(10, 100);

// Generate a random order
Order generateOrder(OrderId& nextId) {
    Side side = sideDist(gen) == 0 ? Side::BUY : Side::SELL;
    Price price = priceDist(gen);
    Quantity qty = qtyDist(gen);
    return {nextId++, price, qty, side, now()};
}

int main() {
    OrderBook book;
    PriceHistory history;
    OrderId nextId = 1;
    uint64_t tradeCount = 0;

    // Seed the book with some initial orders
    std::cout << "Seeding order book..." << std::endl;
    for (int i = 0; i < 20; ++i) {
        book.addOrder(generateOrder(nextId));
    }

    std::cout << "\n=== SIMULATION STARTED (Ctrl+C to stop) ===\n" << std::endl;
    std::cout << "Bid       | Mid       | Ask       | Spread  | Last Trade" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    // Main simulation loop
    while (true) {
        // Generate and submit a random order
        Order order = generateOrder(nextId);
        auto trades = book.addOrder(order);

        // Count trades
        tradeCount += trades.size();

        // Record snapshot for later analysis
        history.record(book.getSnapshot());

        // Display current state (carriage return overwrites the line)
        // \r moves cursor to start of line without newline
        std::cout << "\r$" << priceToString(book.getBestBid())
                  << "  |  $" << priceToString(book.getMidPrice())
                  << "  |  $" << priceToString(book.getBestAsk())
                  << "  |  $" << priceToString(book.getSpread())
                  << "  |  ";

        if (!trades.empty()) {
            // Show the most recent trade
            const Trade& t = trades.back();
            std::cout << t.quantity << " @ $" << priceToString(t.price) << "   ";
        } else {
            std::cout << "              ";  // Clear previous trade display
        }

        std::cout << std::flush;  // Force output (no newline means no auto-flush)

        // Sleep to make it human-readable
        // Change this to speed up or slow down the simulation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Every 1000 orders, export to CSV (for later visualization)
        if (nextId % 1000 == 0) {
            history.exportToCSV("simulation_history.csv");
        }
    }

    return 0;
}
