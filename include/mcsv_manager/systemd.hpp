#pragma once
#include <cinttypes>
#include <string>
#include <vector>
#include <system_error>
#include <systemd/sd-bus.h>

namespace mcsv_manager {

template<typename... Args>
void call_method(
    sd_bus* bus,
    sd_bus_error* error,
    sd_bus_message** reply,
    const char* destination,
    const char* path,
    const char* interface,
    const char* member,
    const char* signature,
    Args&&... args
) {
    int r = sd_bus_call_method(
        bus,
        destination,
        path,
        interface,
        member,
        error,
        reply,
        signature,
        std::forward<Args>(args)...
    );
    if (r < 0) throw std::runtime_error(std::to_string(r));
}

class SystemdBus {
public:
    SystemdBus() {
        connect();
    }

    ~SystemdBus() {
        if (bus)
            sd_bus_unref(bus);
    }

    sd_bus* get() const {
        return bus;
    }

    int fd() const {
        return sd_bus_get_fd(bus);
    }

    uint64_t timeout_usec() const {
        uint64_t t;
        if (sd_bus_get_timeout(bus, &t) < 0)
            return UINT64_MAX;
        return t;
    }

    void process() {
        for (;;) {
            int r = sd_bus_process(bus, nullptr);
            if (r > 0)
                continue;
            if (r == 0)
                break;
            // error -> reconnect
            reconnect();
            break;
        }
    }

    template<typename... Args>
    void call_method(
        sd_bus_error* err,
        sd_bus_message** reply,
        const char* member,
        const char* signature,
        Args&&... args
    ) {
        mcsv_manager::call_method(
            bus, err, reply,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            member, signature, std::forward<Args>(args)...
        );
    }

    std::string callGetUnit(const std::string &name) {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        call_method(&err, &reply, "GetUnit", "s", name.data());
        const char *ans;
        int r = sd_bus_message_read(reply, "o", &ans);
        if (r < 0) throw std::runtime_error(std::to_string(r));
        return ans;
    }

    struct Process {
        std::string control_group;
        uint32_t pid;
        std::string cmd_line;
    };

    std::vector<Process> callGetUnitProcesses(const std::string &name) {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        call_method(&err, &reply, "GetUnitProcesses", "s", name.data());
        const char *cg, *cm;
        uint32_t pid;
        std::vector<Process> ps;
        sd_bus_message_enter_container(reply, 'a', "(sus)");
        while (sd_bus_message_read(reply, "(sus)", &cg, &pid, &cm) > 0) {
            ps.emplace_back(cg, pid, cm);
        }
        return ps;
    }

private:
    sd_bus* bus = nullptr;

    void connect() {
        if (sd_bus_open_system(&bus) < 0)
            throw std::runtime_error("sd_bus_open_system failed");

        sd_bus_set_exit_on_disconnect(bus, 0);
    }

    void reconnect() {
        sd_bus_unref(bus);
        bus = nullptr;
        connect();
    }
};

} // namespace mcsv_manager
