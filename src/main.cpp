#include "feed/poly_feed.cpp"
#include "feed/binance_feed.cpp"
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
        file_ << "timestamp_recv,timestamp_exch,venue,symbol,price,size,side" << std::endl;
    }
    void log(const poly::MarketEvent& evt) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_ << evt.timestamp_recv << ","
            << evt.timestamp_exch << ","
            << (evt.venue == poly::Venue::POLYMARKET ? "POLY" : "BINANCE") << ","
            << evt.symbol << ","
            << evt.price << ","
            << evt.size << ","
            << (evt.side == poly::Side::BUY ? "BUY" : "SELL") << "\n";
    }

    ~CsvLogger() {
        if (file_.is_open()) file_.flush();
    }
};

int main() {
    boost::asio::io_context ioc;
    CsvLogger logger("market_data.log");
    auto poly_feed = std::make_shared<poly::PolyFeed>(ioc);
    poly_feed->set_calback([&logger](const poly::MarketEvent& evt)
    {
        logger.log(evt);
        std::cout << "[TRADE]" << evt.price << " (" << evt.size << ")" << std::endl;
    });
    auto binance_feed = std::make_shared<poly::BinanceFeed>(ioc);
    binance_feed->set_calback([&logger](const poly::MarketEvent& evt)
    {
        logger.log(evt);
        std::cout << "[TRADE]" << evt.price << " (" << evt.size << ")" << std::endl;
    });

    try {
        poly_feed->connect();
        std::string active_asset_id = "27801427116870763425813473135293780501482981171413880573576379343739069284230";
        poly_feed->subscribe(active_asset_id);
        binance_feed->connect();
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Fata error" << e.what() << std::endl;
    }
    return 0;
}