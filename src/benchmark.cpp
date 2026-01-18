/**
 * Order book benchmark
 *
 * Measures raw throughput: how many orders per second can we process?
 */

#include <orderbook/Order.hpp>
#include <orderbook/Orderbook.hpp>
#include <iostream>
#include <random>
#include <chrono>

using namespace orderbook;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> sideDist(0, 1);
std::uniform_int_distribution<Price> priceDist(9800, 10200);
std::uniform_int_distribution<Quantity> qtyDist(10, 100);

Order generateOrder(OrderId& nextId) {
    Side side = sideDist(gen) == 0 ? Side::BUY : Side::SELL;
    Price price = priceDist(gen);
    Quantity qty = qtyDist(gen);
    return {nextId++, price, qty, side, now()};
}

int main() {
    OrderBook book;
    OrderId nextId = 1;
    uint64_t tradeCount = 0;

    // Seed with initial orders
    for (int i = 0; i < 100; ++i) {
        book.addOrder(generateOrder(nextId));
    }

    const int NUM_ORDERS = 1'000'000;  // 1 million orders

    std::cout << "Benchmarking " << NUM_ORDERS << " orders..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ORDERS; ++i) {
        Order order = generateOrder(nextId);
        auto trades = book.addOrder(order);
        tradeCount += trades.size();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double seconds = duration.count() / 1000.0;
    double ordersPerSecond = NUM_ORDERS / seconds;
    double tradesPerSecond = tradeCount / seconds;

    std::cout << "\n=== RESULTS ===" << std::endl;
    std::cout << "Total orders:     " << NUM_ORDERS << std::endl;
    std::cout << "Total trades:     " << tradeCount << std::endl;
    std::cout << "Time:             " << duration.count() << " ms" << std::endl;
    std::cout << "Orders/second:    " << static_cast<int>(ordersPerSecond) << std::endl;
    std::cout << "Trades/second:    " << static_cast<int>(tradesPerSecond) << std::endl;
    std::cout << "Avg time/order:   " << (seconds / NUM_ORDERS) * 1'000'000 << " µs" << std::endl;

    return 0;
}
