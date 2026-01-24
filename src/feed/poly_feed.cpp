#include "feed /feed_client.h"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

namespace poly {
    class PolyFeed : public IFeedClient {
        net::io_context& ioc_;
        ssl::context ctx_{ssl::context::tlsv12_client};
        websocket::stream<beast::ssl_stream<tcp::socket>> ws_;
        EventCallback callback_;
        std::string host_ = "ws-subscriptions-clob.polymarket.com";
        std::string path_ = "/ws/market";
        std::string port_ = "443";

        void process_item(const json& item) {
            if (!item.contains("event_type")) return;
            if (item["event_type"] == "last_trade_price") {
                MarketEvent evt;
                evt.venue = Venue::POLYMARKET;
                evt.symbol = item.value("asset_id", "unknown");
                try {
                    auto p_str = item.value("price", "0");
                    evt.price = std::stod(p_str);

                    auto s_str = item.value("size", "0");
                    evt.size = std::stod(s_str);
                    std::string ts_str = item.value("timestamp", "0");
                    evt.timestamp_exch = std::stoull(ts_str);
                } catch (...) {
                    spdlog::warn("Error converting numbers for event: {}", item.dump());
                    return;
                }

                evt.timestamp_recv = now_ms();
                evt.side = (item.value("side", "") == "BUY") ? Side::BUY : Side::SELL;
                evt.original_payload = item.dump();

                if (callback_) callback_(evt);
            }
        }

    public:
        PolyFeed(net::io_context& ioc) : ioc_(ioc), ws_(net::make_strand(ioc), ctx_) {}

        void set_calback(EventCallback cb) override {
            callback_ = cb;
        }

        void connect() override {
            tcp::resolver resolver(ioc_);
            auto const results = resolver.resolve(host_, port_);

            net::connect(beast::get_lowest_layer(ws_), results);

            if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())){
                throw boost::system::system_error(
                    static_cast<int>(::ERR_get_error()),
                    boost::asio::error::get_ssl_category());
            }

            ws_.next_layer().set_verify_mode(ssl::verify_none);

            ws_.next_layer().handshake(ssl::stream_base::client);
            ws_.handshake(host_, path_);

            spdlog::info("connected");

            read_loop();
        }

        void subscribe(const std::string& token_id) override {
            nlohmann::ordered_json sub_msg;
            sub_msg["type"] = "market";
            sub_msg["assets_ids"] = {token_id};
            std::string payload = sub_msg.dump();
            ws_.write(net::buffer(sub_msg.dump()));
            spdlog::info("WS SEND: {}", payload);
        }

        void run() override {}
    private:
        beast::flat_buffer buffer_;

        void read_loop() {
            ws_.async_read(buffer_,
                [this](beast::error_code ec, std::size_t bytes_transferred){
                    if (ec) {
                        spdlog::error("Read error: {}", ec.message());
                        return;
                    }
                    std::string data = beast::buffers_to_string(buffer_.data());
                    buffer_.consume(buffer_.size());

                    try {
                        parse_message(data);
                    } catch (const std::exception& e) {
                        spdlog::error("Parse error: {}", e.what());
                    }

                    read_loop();
                });

        }

        void parse_message(const std::string& raw_json) {
            try {
                auto j = json::parse(raw_json);

                if (j.is_array()) {
                    for (const auto& item : j) {
                        process_item(item);
                    }
                } else if (j.is_object()) {
                    if (j.contains("type") && j["type"] == "error") {
                        spdlog::error("API Error: {}", raw_json);
                    } else {
                        process_item(j);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("JSON Parse error: {} | Payload: {}", e.what(), raw_json);
            }
        }
    };
}