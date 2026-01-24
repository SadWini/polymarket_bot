#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace poly {
    enum class Venue {
        BINANCE,
        POLYMARKET
    };

    enum class Side {
        BUY,
        SELL,
        UNKNOWN
    };

    struct MarketEvent {
        Venue venue;
        std::string symbol;
        uint64_t timestamp_exch;
        uint64_t timestamp_recv;

        double price;
        double size;
        Side side;

        double best_bid = 0.0;
        double best_asks = 1.0;
        double bid_depth = 0.0;
        double bid_asks = 0.0;

        std::string original_payload;
    };

    inline uint64_t now_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
}