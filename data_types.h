#ifndef DATA_TYPES_H
#define DATA_TYPES_H
#include <tuple>

template<typename T1, typename T2, typename T3>
using triple = std::tuple<T1, T2, T3>;

typedef struct{
    std::tuple<int, double, double> level;
} raw_level;

typedef struct {
    long long asset_id;
    raw_level change;
} price_change;

typedef struct {
    long long asset_id;
    long long market_id;
    double old_tick_size;
    double new_tick_size;
    long timestamp;
} tick_change_message;

typedef struct{
    long long asset_id;
    long long market_id;
    long timestamp;
    raw_level levels[];
} BookMessage;

typedef struct{
    long timestamp;
    long long market_id;
    price_change levels[];
} price_change_message;

#endif //DATA_TYPES_H
