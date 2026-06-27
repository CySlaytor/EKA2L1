#pragma once

#include <cstdint>
#include <services/context.h>
#include <services/window/opheader.h>

namespace eka2l1::epoc {
    namespace ws {
        using uid = std::uint32_t;
        using handle = std::uint32_t;
    };

    class window_server_client;
    using window_server_client_ptr = window_server_client *;

    struct screen;

    struct window_client_obj {
        ws::uid id;

        window_server_client *client;
        screen *scr;

        explicit window_client_obj(window_server_client_ptr client, screen *scr);
        virtual ~window_client_obj() {}

        virtual bool execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        virtual void on_command_batch_done(service::ipc_context &ctx) {}
    };
}