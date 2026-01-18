#include <orderbook/Order.hpp>
#include <orderbook/Orderbook.hpp>
#include <orderbook/PriceHistory.hpp>
#include <iostream>

void printTrades(const std::vector<orderbook::Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "  TRADE: " << t.quantity << " shares @ $"
                  << orderbook::priceToString(t.price)
                  << " (buyer=" << t.buyOrderId << ", seller=" << t.sellOrderId << ")"
                  << std::endl;
    }
}

int main() {
    orderbook::OrderBook book;
    orderbook::PriceHistory history;

    // Buy orders (bids)
    book.addOrder({1, 10000, 100, orderbook::Side::BUY, orderbook::now()});
    book.addOrder({2, 9950, 200, orderbook::Side::BUY, orderbook::now()});
    book.addOrder({3, 9900, 150, orderbook::Side::BUY, orderbook::now()});

    // Sell orders (asks)
    book.addOrder({4, 10050, 75, orderbook::Side::SELL, orderbook::now()});
    book.addOrder({5, 10100, 300, orderbook::Side::SELL, orderbook::now()});
    book.addOrder({6, 10200, 50, orderbook::Side::SELL, orderbook::now()});

    // Record initial state
    history.record(book.getSnapshot());

    std::cout << "=== INITIAL ORDER BOOK ===" << std::endl;
    book.print();
    std::cout << "Mid price: $" << orderbook::priceToString(book.getMidPrice()) << std::endl;

    // Simulate some trading activity
    std::cout << "\n--- Submitting: BUY 100 shares @ $101.00 ---" << std::endl;
    auto trades = book.addOrder({7, 10100, 100, orderbook::Side::BUY, orderbook::now()});
    printTrades(trades);
    history.record(book.getSnapshot());

    std::cout << "\n--- Submitting: SELL 150 shares @ $99.00 ---" << std::endl;
    trades = book.addOrder({8, 9900, 150, orderbook::Side::SELL, orderbook::now()});
    printTrades(trades);
    history.record(book.getSnapshot());

    std::cout << "\n--- Submitting: BUY 200 shares @ $102.00 ---" << std::endl;
    trades = book.addOrder({9, 10200, 200, orderbook::Side::BUY, orderbook::now()});
    printTrades(trades);
    history.record(book.getSnapshot());

    std::cout << "\n=== FINAL ORDER BOOK ===" << std::endl;
    book.print();
    std::cout << "Last trade: $" << orderbook::priceToString(book.getLastTradePrice())
              << " (" << book.getLastTradeQty() << " shares)" << std::endl;

    // Export to CSV
    if (history.exportToCSV("price_history.csv")) {
        std::cout << "\nExported " << history.size() << " snapshots to price_history.csv" << std::endl;
    }

    return 0;
}
