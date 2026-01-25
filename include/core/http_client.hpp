#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <string>
#include <iostream>

namespace poly {
    namespace beast = boost::beast;
    namespace http = boost::beast::http;
    namespace ssl = boost::asio::ssl;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    class HttpClient {
        public:
        static std::string get(const std::string& host, const std::string& target) {
            try {
                net::io_context ioc;
                ssl::context ctx(ssl::context::tlsv12_client);
                ctx.set_default_verify_paths();

                tcp::resolver resolver(ioc);
                beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

                if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
                    throw beast::system_error(
                        beast::error_code(static_cast<int>(::ERR_get_error()),net::error::get_ssl_category()));
                }

                auto const results = resolver.resolve(host, "443");
                beast::get_lowest_layer(stream).connect(results);
                stream.handshake(ssl::stream_base::client);

                http::request<http::string_body> req{http::verb::get, target, 11};
                req.set(http::field::host, host);
                req.set(http::field::user_agent, "PolyBot/1.0");

                http::write(stream, req);

                beast::flat_buffer buffer;
                http::response<http::string_body> res;
                http::read(stream, buffer, res);

                beast::error_code ec;
                stream.shutdown(ec);

                return res.body();
            } catch (std::exception const& e) {
                std::cerr << "HTTP Error: " << e.what() << std::endl;
                return "";
            }
        }
    };
}