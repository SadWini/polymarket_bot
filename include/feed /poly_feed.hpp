#pragma once

#include "feed_client.h"
#include "core/orderbook.hpp"
#include "core/types.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_map>>
#include <string>
#include <unordered_set>

namespace poly {
    namespace beast = boost::beast;
    namespace net = boost::asio;
    namespace ssl = boost::asio::ssl;
    namespace websocket = beast::websocket;
    using tcp = boost::asio::ip::tcp;

    class PolyFeed : public std::enable_shared_from_this<PolyFeed>, public IFeedClient {
    public:
        explicit PolyFeed(net::io_context& ioc);

        void connect() override;
        void subscribe(const std::string& token_id) override;
        void set_calback(EventCallback cb) override;
        void run() override;
        void stop();

        bool is_connected() const {return ws_.is_open();}

    private:
        // Async Handlers
        void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);
        void on_ssl_handshake(beast::error_code ec);
        void on_handshake(beast::error_code ec);
        void on_read(beast::error_code ec, std::size_t bytes_transferred);
        void on_timer(beast::error_code ec);
        void on_close(beast::error_code ec);

        // Helpers
        void fail(beast::error_code ec, char const* what);
        void send_subscription();
        void do_read();

        // Parsing
        void parse_message(const std::string& raw_json);
        void process_item(const nlohmann::json& item);
        void process_book(const nlohmann::json& item);
        void process_price_change(const nlohmann::json& item);

        // Members
        net::io_context& ioc_;
        ssl::context& ctx_;
        tcp::resolver resolver_;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
        beast::flat_buffer buffer_;
        net::steady_timer timer_;

        // Config
        const std::string host_ = "ws-subscriptions-clob.polymarket.com";
        const std::string path_ = "/ws/market";
        const sdt::string port_ = "443";

        // State
        bool is_closing_ = false;
        EventCallback callback_;

        std::unordered_set<std::string> active_subscriptions_;
        std::unordered_map<std::string, Orderbook> books;
    };
}
