#ifndef POLYMARKET_BOT_ORDERBOOK_H
#define POLYMARKET_BOT_ORDERBOOK_H

#include <iomanip>
#include <iostream>

#include "data_types.h"
#include <map>
#include <string>
#include <vector>

class OrderBook {
private:
    std::string asset_id;
    std::map<double, double, std::greater<double>> bids;
    std::map<double, double> asks;

public:
    explicit OrderBook(std::string id) : asset_id(id) {}
    void update_level(Side side, double price, double size) {
        if (side == Side::BID) {
            if (size > 0)
                bids[price] = size;
            else
                bids.erase(price);
        } else {
            if (size > 0)
                asks[price] = size;
            else
                asks.erase(price);
        }
    }

    void apply_snapshot(const OrderBookSnapshot& snapshot) {
        bids.clear();
        asks.clear();

    }

    std::pair<double, double> get_best_ask() const {
        if (asks.empty()) return {-1.0, 0.0};
        return *asks.begin();
    }

    std::pair<double, double> get_best_bid() const {
        if (bids.empty()) return {-1.0, 0.0};
        return *bids.begin();
    }

    // --- DEBUGGING ---
    void print_book() const {
        auto bb = get_best_bid();
        auto ba = get_best_ask();

        std::cout << "---" << asset_id << " ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);

        if (ba.first != -1.0)
            std::cout << "ASK:" << ba.first << " (" << ba.second << ")" << std::endl;
        else
            std::cout << "ASK: None";
        if (bb.first != -1.0)
            std::cout << "BID:" << bb.first << " (" << bb.second << ")" << std::endl;
        else
            std::cout << "BID: None";

        std::cout << "----------" << std::endl;
    }
};
#endif //POLYMARKET_BOT_ORDERBOOK_H