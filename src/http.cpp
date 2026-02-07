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

static json list_server(HttpContext &ctx) {
    return ctx.ctx.manager->list();
}

static json server_status(HttpContext &ctx, const std::string &name) {
    json o = {};
    auto u = ctx.ctx.manager->server_unit_status(name);
    {
        json j = {};
        j["name"] = u.name;
        j["load_state"] = u.load_state;
        j["active_state"] = u.active_state;
        j["sub_state"] = u.sub_state;
        j["following"] = u.following;
        j["object_path"] = u.object_path;
        // j["job_id"] = u.job_id;
        j["job_type"] = u.job_type;
        j["job_path"] = u.job_path;

        o["unit"] = j;
    }

    if (u.active_state == "active") {
        int pid = ctx.ctx.manager->server_process(name).pid;
        std::ifstream f("/proc/"+std::to_string(pid)+"/stat");
        std::string comm, state;
        int ppid, pgrp, session, tty_nr, tp_gid;
        unsigned int flags, rss, rsslim;
        unsigned long minflt, cminflt, majflt, cmajflt, utime, stime, vsize;
        long cutime, cstime, priority, nice, num_threads, itrealvalue;
        unsigned long long starttime;
        f >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >> tp_gid;
        f >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime;
        f >> cutime >> cstime >> priority >> nice >> num_threads >> itrealvalue;
        f >> starttime >> vsize >> rss >> rsslim;

        json j = {};
        j["comm"] = comm;
        j["state"] = state;
        j["minflt"] = minflt;
        j["cminflt"] = cminflt;
        j["majflt"] = majflt;
        j["cmajflt"] = cmajflt;
        j["utime"] = utime;
        j["stime"] = stime;
        j["cutime"] = cutime;
        j["cstime"] = cstime;

        j["num_threads"] = num_threads;
        j["starttime"] = starttime;
        // j["vsize"] = vsize;
        o["stat"] = j;
        
        std::ifstream fm("/proc/" + std::to_string(pid) + "/statm");

        long size, resident, shared, text, lib, data, dt;
        fm >> size >> resident >> shared >> text >> lib >> data >> dt;

        long page_size = sysconf(_SC_PAGESIZE);

        json jm = {};
        jm["size"]     = size * page_size;
        jm["rss"]      = resident * page_size;
        jm["shared"]   = shared * page_size;
        jm["text"]     = text * page_size;
        jm["data"]     = data * page_size;

        o["statm"] = jm;
    }


    return o;
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

    if (ppath.size() == 0) {
        http::response<http::string_body> res{
            http::status::not_found, req.version()
        };
        res.set(http::field::content_type, "text/plain");
        res.body() = "404 unknown endpoint";
        res.prepare_payload();
        http::write(ctx.sock, res);
    } else if (ppath.size() == 1) {
        if (ppath.front() == "status") {
            send_err_or_ok(ctx, &proc_status);
        } else if (ppath.front() == "servers") {
            send_err_or_ok(ctx, &list_server);
        } else goto err_no_endpoint;
    } else if (ppath.size() == 3) {
        if (ppath.front() == "server" && ppath[2] == "status") {
            send_err_or_ok(ctx, std::bind(server_status, std::placeholders::_1, ppath[1]));
        } else goto err_no_endpoint;
    } else goto err_no_endpoint;
    
    return;

    err_no_endpoint:
    http::response<http::string_body> res{
        http::status::not_found, req.version()
    };
    res.set(http::field::content_type, "text/plain");
    res.body() = "404 unknown endpoint";
    res.prepare_payload();
    http::write(ctx.sock, res);
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
