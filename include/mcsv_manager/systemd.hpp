#pragma once
#include <cinttypes>
#include <string>
#include <array>
#include <tuple>
#include <vector>
#include <system_error>
#include <systemd/sd-bus.h>

namespace mcsv_manager {

template<typename... Args>
int call_method(
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
    return sd_bus_call_method(
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
        int r = mcsv_manager::call_method(
            bus, err, reply,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            member, signature, std::forward<Args>(args)...
        );
        if (r < 0) throw std::runtime_error(std::to_string(r) + " "
                    + (err->name ? err->name : "")
                    + " "
                    + (err->message ? err->message : ""));
    }

    private:
    std::string call_ss_o(const char *method, const char *name, const char *mode) {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        call_method(&err, &reply, method, "ss", name, mode);
        const char *ans;
        int r = sd_bus_message_read(reply, "o", &ans);
        if (r < 0) throw std::runtime_error(std::to_string(r));
        std::string res(ans);
        sd_bus_message_unref(reply);
        return res;
    }
    public:

    std::string callGetUnit(const char *name) {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        call_method(&err, &reply, "GetUnit", "s", name);
        const char *ans;
        int r = sd_bus_message_read(reply, "o", &ans);
        if (r < 0) throw std::runtime_error(std::to_string(r));
        std::string res(ans);
        sd_bus_message_unref(reply);
        return res;
    }

    std::string callReloadOrRestartUnit(const char *name, const char *mode = "replace") {
        return call_ss_o("ReloadOrRestartUnit", name, mode);
    }

    std::string callStartUnit(const char *name, const char *mode = "replace") {
        return call_ss_o("StartUnit", name, mode);
    }

    std::string callStopUnit(const char *name, const char *mode = "replace") {
        return call_ss_o("StopUnit", name, mode);
    }

    struct Change {
        std::string type, path, source;
    };
    /**
     * EnableUnitFiles(in  as files,
     *                 in  b runtime,
     *                 in  b force,
     *                 out b carries_install_info,
     *                 out a(sss) changes);
     */
    std::tuple<bool, std::vector<Change>> callEnableUnitFiles(const char **files, bool runtime = false, bool force = false) {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        call_method(&err, &reply, "EnableUnitFiles", "asbb", files, runtime, force);
        bool carries_install_info;
        struct change {
            const char *a, *b, *c;
        } *changes;
        int r = sd_bus_message_read(reply, "ba(sss)", &carries_install_info, &changes);
        if (r < 0) throw std::runtime_error(std::to_string(r));
        std::vector<Change> res;
        for (change *c = changes; c; c++) {
            res.push_back({c->a, c->b, c->c});
        }
        sd_bus_message_unref(reply);
        return {carries_install_info, res};
    }

    /**
     * DisableUnitFiles(in  as files,
     *                  in  b runtime,
     *                  out a(sss) changes);
     */
    std::tuple<bool, std::vector<Change>> callDisableUnitFiles(const char **files, bool runtime = false) {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        call_method(&err, &reply, "EnableUnitFiles", "asb", files, runtime);
        bool carries_install_info;
        struct change {
            const char *a, *b, *c;
        } *changes;
        int r = sd_bus_message_read(reply, "ba(sss)", &carries_install_info, &changes);
        if (r < 0) throw std::runtime_error(std::to_string(r));
        std::vector<Change> res;
        for (change *c = changes; c; c++) {
            res.push_back({c->a, c->b, c->c});
        }
        sd_bus_message_unref(reply);
        return {carries_install_info, res};
    }

    sd_bus_message *new_callStartTransientUnit() {
        sd_bus_message *m = NULL;
        sd_bus_message_new_method_call(
            bus,
            &m,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "StartTransientUnit"
        );
        return m;
    }

    struct Unit {
        std::string name;
        std::string description;
        std::string load_state;
        std::string active_state;
        std::string sub_state;
        std::string following;
        std::string object_path;
        uint32_t job_id;
        std::string job_type;
        std::string job_path;
    };

    std::vector<Unit> callListUnitsByNames(
        const std::vector<const char*> &names
    );

    std::vector<Unit> callListUnitsByPatterns(
        const std::vector<const char*> &states,
        const std::vector<const char*> &patterns
    );

    std::vector<std::pair<std::string, std::string>> callListUnitFilesByPatterns(
        const std::vector<const char*> &states,
        const std::vector<const char*> &patterns
    );

    struct Process {
        std::string control_group;
        uint32_t pid;
        std::string cmd_line;
    };

    std::vector<Process> callGetUnitProcesses(const char *name) {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        call_method(&err, &reply, "GetUnitProcesses", "s", name);
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
