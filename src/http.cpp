#include <functional>
#include <fstream>
#include <vector>
#include <set>
#include <sstream>

#include <mcsv_manager/http.hpp>
#include <mcsv_manager/context.hpp>
#include <nlohmann/json.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

using json = nlohmann::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;

struct HttpContext {
    mcsv_manager::Context &ctx;
    asio::local::stream_protocol::socket &sock;
    http::request<http::string_body> *req;
};

static json proc_status(HttpContext &ctx) {
    const static std::set<std::string> list_meminfo{
        "MemTotal",
        "MemFree",
        "MemAvailable",
        "SwapTotal",
        "SwapFree"
    };
    json res = {};
    std::ifstream f("/proc/meminfo");
    std::string line, k;
    long v;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        ss >> k >> v;
        k.resize(k.length()-1);
        if (list_meminfo.find(k) != list_meminfo.end()) {
            res[k] = v;
        }
    }

    auto &bus = ctx.ctx.systemd_bus;

    return res;
}

static void send_200_json(HttpContext &ctx, json &body) {
    http::response<http::string_body> res{
        http::status::ok, ctx.req->version()
    };
    res.set(http::field::content_type, "application/json");
    res.body() = body.dump();
    res.prepare_payload();
    http::write(ctx.sock, res);
}

static void send_500(HttpContext &ctx) {
    http::response<http::string_body> res{
        http::status::internal_server_error, ctx.req->version()
    };
    res.set(http::field::content_type, "text/plain");
    res.body() = "500 Internal Server Error\n";
    res.body() += ctx.req->target();
    res.prepare_payload();
    http::write(ctx.sock, res);
}

static void send_err_or_ok(HttpContext &ctx, json &resj) {
    if (resj.is_null()) {
        send_500(ctx);
    } else {
        send_200_json(ctx, resj);
    }
}

static void send_err_or_ok(HttpContext &ctx, const std::function<json(HttpContext &)> &func) {
    try {
        json resj = func(ctx);
        send_err_or_ok(ctx, resj);
    } catch (std::runtime_error) {
        send_500(ctx);
    }
}

static void process_request(HttpContext &ctx) {
    beast::flat_buffer buf;
    http::request<http::string_body> req;
    http::read(ctx.sock, buf, req);
    ctx.req = &req;

    const std::string prefix = "/";

    if (!req.target().starts_with(prefix)) {
        http::response<http::string_body> res{
            http::status::internal_server_error, req.version()
        };
        res.set(http::field::content_type, "text/plain");
        res.body() = "500 wtf\n";
        res.body() += req.target();
        res.prepare_payload();
        http::write(ctx.sock, res);
        return;
    }

    auto target = req.target().substr(prefix.length());
    auto pos = target.find('?');
    auto path = (pos == boost::beast::string_view::npos)
        ? target
        : target.substr(0, pos);
    std::vector<std::string> ppath;
    boost::split(ppath, path, boost::is_any_of("/"));

    if (ppath.size() < 1) {
        http::response<http::string_body> res{
            http::status::not_found, req.version()
        };
        res.set(http::field::content_type, "text/plain");
        res.body() = "404 unknown endpoint";
        res.prepare_payload();
        http::write(ctx.sock, res);
    } else if (ppath.front() == "status") {
        send_err_or_ok(ctx, &proc_status);
    } else {
        http::response<http::string_body> res{
            http::status::not_found, req.version()
        };
        res.set(http::field::content_type, "text/plain");
        res.body() = "404 unknown endpoint";
        res.prepare_payload();
        http::write(ctx.sock, res);
    }
}

namespace mcsv_manager {

HttpServer::HttpServer(int listen_fd) : listen_fd(listen_fd) {}


void HttpServer::run(mcsv_manager::Context &ctx) {
    asio::io_context ioc;
    asio::local::stream_protocol::acceptor acc(ioc);
    acc.assign(asio::local::stream_protocol(), listen_fd);

    for (;;) {
        asio::local::stream_protocol::socket sock(ioc);
        acc.accept(sock);

        HttpContext context{ctx, sock};
        process_request(context);
        context.req = nullptr;
    }
}

}
