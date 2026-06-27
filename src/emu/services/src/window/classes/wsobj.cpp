#include <common/log.h>
#include <services/window/classes/wsobj.h>
#include <services/window/common.h>
#include <services/window/window.h>

namespace eka2l1::epoc {
    window_client_obj::window_client_obj(window_server_client_ptr client, screen *scr)
        : client(client)
        , scr(scr)
        , id(0) {
        if (client) {
            id = client->get_ws().next_uid();
        } else {
            id = 0;
        }
    }
}