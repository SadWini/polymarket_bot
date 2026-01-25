#pragma once

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>

namespace poly {
    struct PriceLevel {
        double price;
        double size;
    };

    class Orderbook {
        std::map<double, double, std::greater<double>> bids_;
        std::map<double, double, std::less<double>> asks_;

        mutable std::mutex mtx_;
    public:
        void update(bool is_bid, double price, double size) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (is_bid) {
                if (size <= 1e-9) bids_.erase(price);
                else bids_[price] = size;
            } else {
                if (size <= 1e-9) asks_.erase(price);
                else asks_[price] = size;
            }
        }

        void get_state(double& best_bid, double& best_ask, double& bid_depth, double& ask_depth) {
            std::lock_guard<std::mutex> lock(mtx_);

            if (bids_.empty()) {best_bid = 0; bid_depth = 0;}
            else {
                best_bid = bids_.begin()->first;
                bid_depth = bids_.begin()->second;
            }
            if (asks_.empty()) {best_ask = 1; bid_depth = 0;}
            else {
                best_ask = asks_.begin()->first;
                ask_depth = asks_.begin()->second;
            }

        }

        void clear() {
            std::lock_guard<std::mutex> lock(mtx_);
            bids_.clear();
            asks_.clear();
        }
    };
}