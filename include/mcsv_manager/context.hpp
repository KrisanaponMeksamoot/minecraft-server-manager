#pragma once

#include <string>
#include <optional>
#include <memory>

#include <mcsv_manager/systemd.hpp>
#include <mcsv_manager/http.hpp>
#include <mcsv_manager/mcsv_mgr.hpp>

namespace mcsv_manager {
struct Context {
    bool verbose;

    mcsv_manager::SystemdBus *systemd_bus;
    mcsv_manager::HttpServer *server;
    mcsv_manager::McSvMgr *manager;
};
}