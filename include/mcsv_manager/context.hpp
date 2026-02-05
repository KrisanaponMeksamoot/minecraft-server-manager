#pragma once

#include <string>
#include <optional>
#include <memory>

#include <mcsv_manager/systemd.hpp>
#include <mcsv_manager/http.hpp>

namespace mcsv_manager {
struct Context {
    mcsv_manager::SystemdBus *systemd_bus;
    mcsv_manager::HttpServer *server;
};
}