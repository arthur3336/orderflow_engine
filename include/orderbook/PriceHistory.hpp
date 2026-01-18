#pragma once

#include <orderbook/Types.hpp>
#include <deque>
#include <string>
#include <fstream>

namespace orderbook {

    struct PriceData {
        Timestamp time;
        Price bidPrice;       // Best bid (0 if empty)
        Price askPrice;       // Best ask (0 if empty)
        Price midPrice;       // (bid + ask) / 2
        Price spread;         // ask - bid
        Price lastTradePrice; // From most recent trade
        Quantity lastTradeQty;
    };

    class PriceHistory {
    public:
        PriceHistory(size_t maxSize = 10000) : maxSize(maxSize) {}

        void record(const PriceData& data) {
            history.push_back(data);
            // Maintain rolling window - remove oldest if over limit
            if (history.size() > maxSize) {
                history.pop_front();
            }
        }

        size_t size() const { return history.size(); }

        const PriceData& latest() const { return history.back(); }

        bool exportToCSV(const std::string& filename) const {
            std::ofstream file(filename);
            if (!file.is_open()) return false;

            // Header
            file << "timestamp_ns,bid,ask,mid,spread,last_price,last_qty\n";

            // Get start time for relative timestamps
            auto startTime = history.empty() ? now() : history.front().time;

            for (const auto& data : history) {
                // Convert timestamp to nanoseconds since start
                auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    data.time - startTime
                ).count();

                file << ns << ","
                     << data.bidPrice << ","
                     << data.askPrice << ","
                     << data.midPrice << ","
                     << data.spread << ","
                     << data.lastTradePrice << ","
                     << data.lastTradeQty << "\n";
            }

            return true;
        }

    private:
        std::deque<PriceData> history;
        size_t maxSize;
    };

}
