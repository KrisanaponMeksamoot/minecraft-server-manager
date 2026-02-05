#include <mcsv_manager/systemd.hpp>
#include <systemd/sd-bus.h>

namespace mcsv_manager {

bool start_unit(const std::string& unit) {
    sd_bus* bus = nullptr;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = nullptr;

    if (sd_bus_open_system(&bus) < 0)
        return false;

    int r = sd_bus_call_method(
        bus,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "StartUnit",
        &err,
        &msg,
        "ss",
        unit.c_str(),
        "replace"
    );

    sd_bus_message_unref(msg);
    sd_bus_error_free(&err);
    sd_bus_unref(bus);

    return r >= 0;
}

}
