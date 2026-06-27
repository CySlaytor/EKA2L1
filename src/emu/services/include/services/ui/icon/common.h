#pragma once

#include <common/vecx.h>

#include <common/rgb.h>
#include <services/window/common.h>
#include <utils/fscmn.h>

namespace eka2l1::epoc {
    struct akn_icon_srv_return_data {
        int bitmap_handle;
        int mask_handle;
        eka2l1::vec2 content_dim;
    };

    struct akn_icon_init_data {
        int compression;

        epoc::display_mode icon_mode;
        epoc::display_mode icon_mask_mode;
        epoc::display_mode photo_mode;
        epoc::display_mode video_mode;
        epoc::display_mode offscreen_mode;
        epoc::display_mode offscreen_mask_mode;
    };

    struct akn_icon_params {
        enum flags {
            flag_use_default_icon_dir,
            flag_mbm_icon,
            flag_exclude_from_cache,
            flag_disable_compression
        };

        epoc::filename file_name;
        int bitmap_id;
        int mask_id;
        eka2l1::object_size size;
        int mode;
        int rotation;

        common::rgba color;
        epoc::rfile file_native;
        bool app_icon;

        int flags;
    };
}