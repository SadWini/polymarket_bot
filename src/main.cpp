#include "feed/poly_feed.cpp"
#include "core/types.hpp"
#include <iostream>
#include <fstream>
#include <thread>

class CsvLogger {
    std::ofstream file_;
    std::mutex mtx_;
public:
    CsvLogger(const std::string& filename) {
        file_.open(filename);
        file_ << "timestamp_recv,timestamp_exch,venue,symbol,price,size,side";
    }
    void log(const poly::MarketEvent& evt) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_ << evt.timestamp_recv << ","
            << evt.timestamp_recv << ","
            << (evt.venue == poly::Venue::POLYMARKET ? "POLY" : "BINANCE") << ","
            << evt.symbol << ","
            << evt.price << ","
            << evt.size << ","
            << (evt.side == poly::Side::BUY ? "BUY" : "SELL") << "\n"
            << std::flush;
    }
};

int main() {

    return 0;
}