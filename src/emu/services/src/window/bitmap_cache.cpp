#include <services/fbs/fbs.h>
#include <services/fbs/palette.h>
#include <services/window/bitmap_cache.h>
#include <services/window/classes/gstore.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>
#include <kernel/chunk.h>
#include <kernel/kernel.h>
#include <system/epoc.h>

#include <loader/nvg.h>
#include <utils/guest/akn.h>

#include <algorithm>
#include <common/buffer.h>
#include <common/log.h>
#include <common/runlen.h>
#include <common/time.h>

#define XXH_INLINE_ALL
#include <lunasvg.h>
#include <xxhash.h>

namespace eka2l1::epoc {
    bitmap_cache::bitmap_cache(kernel_system *kern_)
        : fbss_(nullptr)
        , kern(kern_) {
        std::fill(driver_textures.begin(), driver_textures.end(), 0);
    }

    void bitmap_cache::clean(drivers::graphics_driver *drv) {
        if (!drv) {
            return;
        }

        drivers::graphics_command_builder builder;

        for (const auto tex_handle : driver_textures) {
            if (tex_handle) {
                builder.destroy_bitmap(tex_handle);
            }
        }

        eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
        drv->submit_command_list(retrieved);
    }
}