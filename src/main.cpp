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
    boost::asio::io_context ioc;
    CsvLogger logger("market_data.log");
    poly::PolyFeed poly_feed(ioc);
    poly_feed.set_calback([&logger](const poly::MarketEvent& evt)
    {
        logger.log(evt);
        std::cout << "[TRADE]" << evt.price << " (" << evt.size << ")" << std::endl;
    });

    try {
        poly_feed.connect();
        std::string active_asset_id = "111938262309312181451460904134500914401044285052974058660940444425309393359260";
        poly_feed.subscribe(active_asset_id);

        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Fata error" << e.what() << std::endl;
    }
    return 0;
}