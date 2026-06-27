#pragma once

#include <services/window/classes/wsobj.h>

namespace eka2l1 {
    namespace epoc {
        struct bitwise_bitmap;
    }

    struct fbsbitmap;
    class fbs_server;

    namespace epoc {
        struct wsbitmap : public window_client_obj {
            epoc::bitwise_bitmap *bitmap_;
            fbsbitmap *parent_;

            explicit wsbitmap(window_server_client_ptr client, epoc::bitwise_bitmap *bmp,
                fbsbitmap *parent = nullptr);
            ~wsbitmap() override;

            fbsbitmap *get_and_update_parent();
            bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;
        };
    }
}