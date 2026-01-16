#ifndef DATA_TYPES_H
#define DATA_TYPES_H
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

enum class Side
{
    BID,
    ASK
};

struct Level{
    double price;
    double size;
};

struct OrderBookUpdate {
    std::string asset_id;
    Side side;
    double price;
    double size;
    long long timestamp;
};

struct OrderBookSnapshot{
    std::string asset_id;
    long long market_id;
    std::vector<Level> bids;
    std::vector<Level> asks;
    long long timestamp;
};

inline Side string_to_side(std::string s)
{
    if (s == "bids" || s == "BUY")
        return Side::BID;
    if (s == "asks" || s == "SELL")
        return Side::ASK;
    throw std::runtime_error("Unknown side string: " + s);
}

#endif //DATA_TYPES_H
