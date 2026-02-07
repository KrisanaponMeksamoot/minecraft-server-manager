#pragma once

#include <string>
#include <vector>

#include <mcsv_manager/systemd.hpp>
#include <mcsv_manager/context.hpp>

namespace mcsv_manager {
class McSvMgr {
    mcsv_manager::SystemdBus &bus;

    public:
    McSvMgr(mcsv_manager::SystemdBus &bus) : bus(bus) {}

    std::vector<std::string> list() {
        std::vector<std::string> out;
        auto units = bus.callListUnitFilesByPatterns({}, {"minecraft@*.service"});
        for (auto [path, state] : units) {
            if (path.starts_with("/etc/systemd/system/minecraft@"))
                out.push_back(path.substr(30, path.length()-38));
        }
        return out;
    }

    SystemdBus::Unit server_unit_status(const std::string &name) {
        auto units = bus.callListUnitsByNames({("minecraft@"+name+".service").data()});
        if (units.size()) return units.front();
        throw std::runtime_error("not found");
    }

    SystemdBus::Process server_process(const std::string &name) {
        auto ps = bus.callGetUnitProcesses("mcsv_manager.service");
        if (ps.size()) return ps.front();
        throw std::runtime_error("not found");
    }
};
}