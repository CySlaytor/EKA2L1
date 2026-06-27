#pragma once

#include <services/etel/common.h>
#include <services/etel/modmngr.h>
#include <services/etel/subsess.h>
#include <services/framework.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1 {
    namespace service {
        class property;
    }

    using etel_subsession_instance = std::unique_ptr<etel_subsession>;

    std::string get_etel_server_name_by_epocver(const epocver ver);

    struct etel_session : public service::typical_session {
        std::vector<etel_subsession_instance> subsessions_;

    protected:
        void add_new_subsession(service::ipc_context *ctx, etel_subsession_instance &instance);

    public:
        ~etel_session() override;

        void load_phone_module(service::ipc_context *ctx);
        void enumerate_phones(service::ipc_context *ctx);
        void get_phone_info_by_index(service::ipc_context *ctx);
        void get_tsy_name(service::ipc_context *ctx);
        void query_tsy_functionality(service::ipc_context *ctx);
        void line_enumerate_call(service::ipc_context *ctx);

        void open_from_session(service::ipc_context *ctx);
        void open_from_subsession(service::ipc_context *ctx);
        void close_sub(service::ipc_context *ctx);

        explicit etel_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_ver);
        void fetch(service::ipc_context *ctx) override;
    };

    class etel_server : public service::typical_server {
    protected:
        friend struct etel_session;

        epoc::etel::module_manager mngr_;

        service::property *call_status_prop_;
        service::property *network_bars_prop_;
        service::property *battery_bars_prop_;
        service::property *charger_status_prop_;
        service::property *call_type_info_prop_;
        service::property *sim_c_status_prop_;

        bool init2ed_;

        void init(kernel_system *kern);
        void init2(kernel_system *kern, io_system *io);

    public:
        explicit etel_server(eka2l1::system *sys);
        void connect(service::ipc_context &ctx) override;

        etel_legacy_level legacy_level();
    };
}