#ifndef HTTP_CLIENT_H_
#define HTTP_CLIENT_H_

#include <functional>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast/http/verb.hpp>

// One-shot async HTTP(S) request. Each request owns itself until the callback fires, so the
// caller only needs to keep the io_context alive.
class HttpClient {
  public:
    struct Request {
        boost::beast::http::verb method = boost::beast::http::verb::post;
        std::string url;
        std::string body;
        std::string content_type = "application/json";
        std::string bearer_token; // empty => no Authorization header
        int timeout_sec = 10;
    };

    struct Response {
        bool ok = false;  // the transport succeeded and the status is 2xx
        int status = 0;   // 0 when the request never reached a response
        std::string body; // the response body, or the failure reason when !ok
    };

    using OnCompleteFunc = std::function<void(const Response &)>;

    static void Send(boost::asio::io_context &ioc, Request request, OnCompleteFunc on_complete);
};

#endif // HTTP_CLIENT_H_
