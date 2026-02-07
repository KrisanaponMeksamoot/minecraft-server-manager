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

std::vector<SystemdBus::Unit> SystemdBus::callListUnitsByNames(
    const std::vector<const char*> &names
) {
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *m = nullptr;
    sd_bus_message *reply = nullptr;

    int r = sd_bus_message_new_method_call(
        bus,
        &m,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "ListUnitsByNames"
    );
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r < 0) throw std::runtime_error(std::to_string(r));
    for (auto v : names) {
        r = sd_bus_message_append(m, "s", v);
        if (r < 0) throw std::runtime_error(std::to_string(r));
    }
    r = sd_bus_message_close_container(m);
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_call(bus, m, 0, &err, &reply);

    std::vector<Unit> units;

    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (r < 0) throw std::runtime_error(std::to_string(r));

    const char *name, *desc, *load, *active, *sub, *follow;
    const char *obj, *job_type, *job_path;
    uint32_t job_id;
    while ((r = sd_bus_message_read(
            reply,
            "(ssssssouso)",
            &name,
            &desc,
            &load,
            &active,
            &sub,
            &follow,
            &obj,
            &job_id,
            &job_type,
            &job_path
        )) > 0) {

        units.emplace_back(
            name, desc, load, active, sub, follow, obj, job_id, job_type, job_path
        );
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return units;
}

std::vector<SystemdBus::Unit> SystemdBus::callListUnitsByPatterns(
    const std::vector<const char*> &states,
    const std::vector<const char*> &patterns
) {
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *m = nullptr;
    sd_bus_message *reply = nullptr;

    int r = sd_bus_message_new_method_call(
        bus,
        &m,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "ListUnitsByPatterns"
    );
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r < 0) throw std::runtime_error(std::to_string(r));
    for (auto v : states) {
        r = sd_bus_message_append(m, "s", v);
        if (r < 0) throw std::runtime_error(std::to_string(r));
    }
    r = sd_bus_message_close_container(m);
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r < 0) throw std::runtime_error(std::to_string(r));
    for (auto v : patterns) {
        r = sd_bus_message_append(m, "s", v);
        if (r < 0) throw std::runtime_error(std::to_string(r));
    }
    r = sd_bus_message_close_container(m);
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_call(bus, m, 0, &err, &reply);

    std::vector<Unit> units;

    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (r < 0) throw std::runtime_error(std::to_string(r));

    const char *name, *desc, *load, *active, *sub, *follow;
    const char *obj, *job_type, *job_path;
    uint32_t job_id;
    while ((r = sd_bus_message_read(
            reply,
            "(ssssssouso)",
            &name,
            &desc,
            &load,
            &active,
            &sub,
            &follow,
            &obj,
            &job_id,
            &job_type,
            &job_path
        )) > 0) {

        units.emplace_back(
            name, desc, load, active, sub, follow, obj, job_id, job_type, job_path
        );
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return units;
}

std::vector<std::pair<std::string, std::string>> SystemdBus::callListUnitFilesByPatterns(
    const std::vector<const char*> &states,
    const std::vector<const char*> &patterns
) {
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *m = nullptr;
    sd_bus_message *reply = nullptr;

    int r = sd_bus_message_new_method_call(
        bus,
        &m,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "ListUnitFilesByPatterns"
    );
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r < 0) throw std::runtime_error(std::to_string(r));
    for (auto v : states) {
        r = sd_bus_message_append(m, "s", v);
        if (r < 0) throw std::runtime_error(std::to_string(r));
    }
    r = sd_bus_message_close_container(m);
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r < 0) throw std::runtime_error(std::to_string(r));
    for (auto v : patterns) {
        r = sd_bus_message_append(m, "s", v);
        if (r < 0) throw std::runtime_error(std::to_string(r));
    }
    r = sd_bus_message_close_container(m);
    if (r < 0) throw std::runtime_error(std::to_string(r));

    r = sd_bus_call(bus, m, 0, &err, &reply);

    std::vector<std::pair<std::string, std::string>> units;

    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ss)");
    if (r < 0) throw std::runtime_error(std::to_string(r));

    const char *path, *state;
    while ((r = sd_bus_message_read(
            reply,
            "(ss)",
            &path, &state
        )) > 0) {

        units.emplace_back(path, state);
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return units;
}

}
