#pragma once

#include <string>

namespace mcsv_manager {
class HttpServer;
}

#include <mcsv_manager/context.hpp>

namespace mcsv_manager {
class HttpServer {
public:
    explicit HttpServer(int listen_fd);
    void run(mcsv_manager::Context &ctx);
private:
    int listen_fd;
};
}
