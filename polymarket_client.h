#ifndef POLYMARKET_BOT_POLYMARKET_CLIENT_H
#define POLYMARKET_BOT_POLYMARKET_CLIENT_H

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "data_types.h"
#include "orderbook.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

#endif //POLYMARKET_BOT_POLYMARKET_CLIENT_H