#include <CLI/CLI.hpp>
#include <mcsv_manager/http.hpp>
#include <mcsv_manager/systemd.hpp>
#include <mcsv_manager/context.hpp>

#include <unistd.h>

int main(int argc, char **argv) {
    CLI::App app;

    bool verbose = false;

    app.add_option("-v,--verbose", verbose);

    CLI11_PARSE(app, argc, argv);

    std::cout << "Starting server..." << std::endl;

    mcsv_manager::Context ctx;

    mcsv_manager::SystemdBus bus;
    mcsv_manager::HttpServer server(STDIN_FILENO);
    
    ctx.systemd_bus = &bus;
    ctx.server = &server;

    server.run(ctx);

    return 0;
}