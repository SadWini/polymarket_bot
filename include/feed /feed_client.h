#pragma once
#include "core/types.h"
#include <functional>
#include <string>

namespace poly {
    // Engine tells the feed what it should to do when data arrive
    using EventCallback = std::function<void(const MarketEvent&)>;

    class IFeedClient {
    public:
        virtual ~IFeedClient() = default;

        virtual void connect() = 0;

        virtual void subscribe(const std::string& market_id) = 0;

        virtual void set_calback(EventCallback cb) = 0;

        virtual void run() = 0;
    };
}