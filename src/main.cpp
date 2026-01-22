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

void printResult(const orderbook::OrderResult& result) {
    if (!result.accepted) {
        std::cout << "  REJECTED: " << result.rejectReason << std::endl;
    } else {
        printTrades(result.trades);
        if (result.remainingQuantity > 0) {
            std::cout << "  Remaining: " << result.remainingQuantity << " shares" << std::endl;
        }
    }
}

int main() {
    using namespace orderbook;

    OrderBook book;
    PriceHistory history;

    // Buy orders (bids) - different traders
    book.addOrderToBook(Order::Limit(1, 10000, 100, Side::BUY, "TraderA", STPMode::CANCEL_NEWEST));
    book.addOrderToBook(Order::Limit(2, 9950, 200, Side::BUY, "TraderB", STPMode::CANCEL_NEWEST));
    book.addOrderToBook(Order::Limit(3, 9900, 150, Side::BUY, "TraderC", STPMode::CANCEL_NEWEST));

    // Sell orders (asks) - different traders
    book.addOrderToBook(Order::Limit(4, 10050, 75, Side::SELL, "TraderD", STPMode::CANCEL_NEWEST));
    book.addOrderToBook(Order::Limit(5, 10100, 300, Side::SELL, "TraderE", STPMode::CANCEL_NEWEST));
    book.addOrderToBook(Order::Limit(6, 10200, 50, Side::SELL, "TraderF", STPMode::CANCEL_NEWEST));

    // Record initial state
    history.record(book.getSnapshot());

    std::cout << "=== INITIAL ORDER BOOK ===" << std::endl;
    book.print();
    std::cout << "Mid price: $" << priceToString(book.getMidPrice()) << std::endl;

    // Test LIMIT orders
    std::cout << "\n--- Submitting: LIMIT BUY 100 shares @ $101.00 ---" << std::endl;
    auto result = book.addOrderToBook(Order::Limit(7, 10100, 100, Side::BUY, "TraderG", STPMode::CANCEL_NEWEST));
    printResult(result);
    history.record(book.getSnapshot());

    std::cout << "\n--- Submitting: LIMIT SELL 150 shares @ $99.00 ---" << std::endl;
    result = book.addOrderToBook(Order::Limit(8, 9900, 150, Side::SELL, "TraderH", STPMode::CANCEL_NEWEST));
    printResult(result);
    history.record(book.getSnapshot());

    // Test MARKET orders
    std::cout << "\n--- Submitting: MARKET BUY 50 shares ---" << std::endl;
    result = book.addOrderToBook(Order::Market(9, 50, Side::BUY, "TraderI", STPMode::CANCEL_NEWEST));
    printResult(result);
    history.record(book.getSnapshot());

    std::cout << "\n--- Submitting: MARKET SELL 30 shares ---" << std::endl;
    result = book.addOrderToBook(Order::Market(10, 30, Side::SELL, "TraderJ", STPMode::CANCEL_NEWEST));
    printResult(result);
    history.record(book.getSnapshot());

    // Test FOK order (should fail if not enough liquidity)
    std::cout << "\n--- Submitting: FOK BUY 10000 shares @ $102.00 (should fail) ---" << std::endl;
    result = book.addOrderToBook(Order::Limit(11, 10200, 10000, Side::BUY, "TraderK", STPMode::CANCEL_NEWEST, TimeInForce::FOK));
    printResult(result);

    // Test IOC order (fill what you can, cancel rest)
    std::cout << "\n--- Submitting: IOC BUY 500 shares @ $102.00 ---" << std::endl;
    result = book.addOrderToBook(Order::Limit(12, 10200, 500, Side::BUY, "TraderL", STPMode::CANCEL_NEWEST, TimeInForce::IOC));
    printResult(result);
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
