#include <CLI/CLI.hpp>
#include <mcsv_manager/http.hpp>
#include <mcsv_manager/systemd.hpp>
#include <mcsv_manager/context.hpp>

#include <systemd/sd-daemon.h>

#include <sstream>
#include <nlohmann/json.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

using json = nlohmann::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;

int main(int argc, char **argv) {

    mcsv_manager::SystemdBus bus;
    mcsv_manager::HttpServer server(0);
    
    mcsv_manager::Context ctx;

    ctx.systemd_bus = &bus;
    ctx.server = &server;

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
        std::cout << k << " " << v << std::endl;
        if (list_meminfo.find(k) != list_meminfo.end()) {
            res[k] = v;
        }
    }

    std::cout << res.dump() << std::endl;

    auto ps = bus.callGetUnitProcesses("apache2.service");
    for (auto p : ps)
        std::cout << p.control_group << ' ' << p.pid << ' ' << p.cmd_line << std::endl;
}