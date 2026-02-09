#include <orderbook/Order.hpp>
#include <orderbook/Orderbook.hpp>
#include <orderbook/PriceHistory.hpp>
#include <iostream>

void printTrades(const std::vector<orderbook::Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "  TRADE #" << t.tradeId << ": " << t.quantity << " shares @ $"
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

    // Test ORDER MODIFICATION
    // Add fresh orders to modify against a book with both sides
    book.addOrderToBook(Order::Limit(20, 9900, 100, Side::BUY, "TraderM", STPMode::CANCEL_NEWEST));
    book.addOrderToBook(Order::Limit(21, 10300, 80, Side::SELL, "TraderN", STPMode::CANCEL_NEWEST));

    std::cout << "\n=== ORDER BOOK BEFORE MODIFICATIONS ===" << std::endl;
    book.print();

    // Case 1: Quantity decrease (keeps time priority)
    std::cout << "\n--- Modifying order #20: qty 100 -> 60 (same price) ---" << std::endl;
    auto modResult = book.modifyOrder(20, 9900, 60);
    std::cout << "  " << (modResult.accepted ? "ACCEPTED" : "REJECTED: " + modResult.rejectReason)
              << " (was " << modResult.oldQuantity << " @ $" << priceToString(modResult.oldPrice)
              << " -> " << modResult.newQuantity << " @ $" << priceToString(modResult.newPrice) << ")" << std::endl;

    // Case 2: Price change (loses time priority)
    std::cout << "\n--- Modifying order #20: price $99.00 -> $99.50 ---" << std::endl;
    modResult = book.modifyOrder(20, 9950, 60);
    std::cout << "  " << (modResult.accepted ? "ACCEPTED" : "REJECTED: " + modResult.rejectReason)
              << " (was " << modResult.oldQuantity << " @ $" << priceToString(modResult.oldPrice)
              << " -> " << modResult.newQuantity << " @ $" << priceToString(modResult.newPrice) << ")" << std::endl;

    // Case 3: Reject — buy price would cross spread (>= best ask $103.00)
    std::cout << "\n--- Modifying order #20: price $99.50 -> $105.00 (should fail — crosses spread) ---" << std::endl;
    modResult = book.modifyOrder(20, 10500, 60);
    std::cout << "  " << (modResult.accepted ? "ACCEPTED" : "REJECTED: " + modResult.rejectReason) << std::endl;

    // Case 4: Reject — order not found
    std::cout << "\n--- Modifying order #999 (should fail — not found) ---" << std::endl;
    modResult = book.modifyOrder(999, 9900, 50);
    std::cout << "  " << (modResult.accepted ? "ACCEPTED" : "REJECTED: " + modResult.rejectReason) << std::endl;

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
