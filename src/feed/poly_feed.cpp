#include "feed /feed_client.h"
#include "core/orderbook.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

namespace poly {
    class PolyFeed : public std::enable_shared_from_this<PolyFeed>, public IFeedClient {
        net::io_context& ioc_;
        ssl::context ctx_{ssl::context::tlsv12_client};
        tcp::resolver resolver_;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
        beast::flat_buffer buffer_;

        std::string host_ = "ws-subscriptions-clob.polymarket.com";
        std::string path_ = "/ws/market";
        std::string port_ = "443";
        std::string active_token_id_;

        net::steady_timer reconnect_timer_;
        bool is_closing_ = false;

        EventCallback callback_;

        Orderbook book_;
    public:
        PolyFeed(net::io_context& ioc) : ioc_(ioc),
        resolver_(net::make_strand(ioc)),
        ws_(net::make_strand(ioc), ctx_),
        reconnect_timer_(ioc) {
            ws_.next_layer().set_verify_mode(ssl::verify_none);

            ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
            ws_.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req)
                {
                    req.set(http::field::user_agent, "PolyCppClient/1.0");
                }));
        }

        void connect() override {
            is_closing_ = false;
            spdlog::info("Resolving {}...", host_);

            resolver_.async_resolve(host_, port_,
                beast::bind_front_handler(&PolyFeed::on_resolve, shared_from_this()));
        }

        void set_calback(EventCallback cb) override {
            callback_ = cb;
        }

        void subscribe(const std::string& token_id) override {
            active_token_id_ = token_id;
            if (ws_.is_open()) {
                send_subscription();
            }
        }

        void run() override {
            connect();
        }

        void stop() {
            is_closing_ = true;
            reconnect_timer_.cancel();
            ws_.async_close(websocket::close_code::normal,
                beast::bind_front_handler(&PolyFeed::on_close,shared_from_this()));
        }
    private:
        void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
            if (ec) return fail(ec, "resolve");

            beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

            beast::get_lowest_layer(ws_).async_connect(results,
                beast::bind_front_handler(&PolyFeed::on_connect, shared_from_this()));
        }

        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
            if (ec) return fail(ec, "connect");

            beast::get_lowest_layer(ws_).expires_never();
            auto ssl_handle = ws_.next_layer().native_handle();
            if (!ssl_handle) {
                return fail(beast::error_code(net::error::no_memory), "ssl_handle_null");
            }

            if (!SSL_set_tlsext_host_name(ssl_handle, host_.c_str())) {
                int err = ::ERR_get_error();
                ec = (err != 0) ? beast::error_code(err, net::error::get_ssl_category())
                    : beast::error_code(net::error::invalid_argument);
                return fail(ec, "ssl_sni");
            }

            ws_.next_layer().async_handshake(ssl::stream_base::client,
                beast::bind_front_handler(&PolyFeed::on_ssl_handshake, shared_from_this()));
        }

        void on_ssl_handshake(beast::error_code ec) {
            if (ec) return fail(ec, "ssl_handshake");

            ws_.async_handshake(host_, path_,
                beast::bind_front_handler(&PolyFeed::on_handshake, shared_from_this()));
        }

        void on_handshake(beast::error_code ec) {
            if (ec) return fail(ec, "handshake");

            spdlog::info("Connected via WSS");

            if (!active_token_id_.empty()) {
                send_subscription();
            }

            do_read();
        }

        void do_read() {
            ws_.async_read(buffer_,
                beast::bind_front_handler(&PolyFeed::on_read, shared_from_this()));
        }

        void on_read(beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) return fail(ec, "read");

            std::string data = beast::buffers_to_string(buffer_.data());
            buffer_.consume(buffer_.size());

            try {
                parse_message(data);
            } catch (const std::exception& e) {
                spdlog::error("Parse Exception: {}", e.what());
            }

            do_read();
        }

        void send_subscription() {
            nlohmann::ordered_json sub_msg;
            sub_msg["type"] = "market";
            sub_msg["assets_ids"] = {active_token_id_};

            auto payload = std::make_shared<std::string>(sub_msg.dump());
            ws_.async_write(net::buffer(*payload),
                [payload, self = shared_from_this()](beast::error_code ec, std::size_t)
                {
                    if (ec) spdlog::error("Write error: {}", ec.message());
                    else spdlog::info("Subscribed to {}", self->active_token_id_);
                });
        }

        void fail(beast::error_code ec, char const* what) {
            if (is_closing_) return;

            spdlog::error("Network error: [{}]: {}", what, ec.message());

            spdlog::info("Reconnecting in 2 seconds...");
            beast::get_lowest_layer(ws_).close();
            reconnect_timer_.expires_after(std::chrono::seconds(2));
            reconnect_timer_.async_wait(beast::bind_front_handler(&PolyFeed::on_timer, shared_from_this()));
        }

        void on_timer(beast::error_code ec) {
            if (ec || is_closing_) return;
            connect();
        }

        void on_close(beast::error_code ec) {
            spdlog::info("Connection closed cleanly");
        }

        void process_item(const json& item) {
            if (!item.contains("event_type")) return;
            std::string type = item["event_type"];
            if (type == "book") {
                process_book(item); return;
            }
            if (type ==  "last_trade_price") {
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

                book_.get_state(evt.best_bid, evt.best_ask, evt.bid_depth, evt.ask_depth);
                if (callback_) callback_(evt);
            }
        }

        void process_book(const json& item) {
            try {
                if (item.contains("bids")) {
                    for (const auto& level : item["bids"]) {
                        double p = std::stod(level.value("price", "0"));
                        double s = std::stod(level.value("size", "0"));
                        book_.update(true, p, s);
                    }
                }

                if (item.contains("asks")) {
                    for (const auto& level : item["asks"]) {
                        double p = std::stod(level.value("price", "0"));
                        double s = std::stod(level.value("size", "0"));
                        book_.update(false, p, s);
                    }
                }
            } catch (...) {
                spdlog::warn("Book parse error");
            }
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