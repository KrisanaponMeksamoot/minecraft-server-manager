#pragma once

#include <mcsv_manager/systemd.hpp>
#include <mcsv_manager/context.hpp>

namespace mcsv_manager {
class McSvMgr {
    mcsv_manager::SystemdBus &bus;
    McSvMgr(mcsv_manager::SystemdBus &bus) : bus(bus) {}

    void new_server(const std::string &name) {
        // TODO: implement this
    }
};
}