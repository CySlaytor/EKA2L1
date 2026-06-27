#pragma once

namespace eka2l1 {
    enum akn_icon_server_request {
        akn_icon_server_retrieve_or_create_shared_icon,
        akn_icon_server_free_bitmap,
        akn_icon_server_get_content_dim,
        akn_icon_server_preserve_icon_data,
        akn_icon_server_destroy_icon_data,
        akn_icon_server_get_init_data,
        akn_icon_server_request_to_enable_cache
    };
}