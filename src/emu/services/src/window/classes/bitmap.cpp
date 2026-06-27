#include <services/fbs/fbs.h>
#include <services/window/classes/bitmap.h>
#include <services/window/window.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    wsbitmap::wsbitmap(window_server_client_ptr client, epoc::bitwise_bitmap *bmp, fbsbitmap *parent)
        : window_client_obj(client, nullptr)
        , bitmap_(bmp)
        , parent_(parent) {
        if (parent_) {
            parent_->ref();
        }
    }

    wsbitmap::~wsbitmap() {
        if (parent_) {
            parent_->deref();
        }
    }

    fbsbitmap *wsbitmap::get_and_update_parent() {
        fbsbitmap *previous_parent = parent_;
        fbsbitmap *clean = previous_parent->final_clean();

        if (clean != previous_parent) {
            clean->ref();
            previous_parent->deref();
            parent_ = clean;
        }
        return parent_;
    }

    bool wsbitmap::execute_command(service::ipc_context &context, ws_cmd &cmd) {
        bool quit = false;
        if (cmd.header.op == 0) {
            context.complete(epoc::error_none);
            client->delete_object(cmd.obj_handle);
            quit = true;
        }
        return quit;
    }
}