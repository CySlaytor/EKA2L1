#pragma once

#include <common/types.h>
#include <kernel/server.h>
#include <utils/dll.h>

namespace eka2l1 {
    const std::string get_loader_server_name_through_epocver(const epocver ver);

    class loader_server : public service::server {
        void load_process(service::ipc_context &context);
        void load_library(service::ipc_context &context);
        void get_info(service::ipc_context &context);
        void check_library_hash(service::ipc_context &context);

    public:
        explicit loader_server(system *sys);
    };
}