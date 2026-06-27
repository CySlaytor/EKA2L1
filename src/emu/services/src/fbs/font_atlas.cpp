#include <common/algorithm.h>
#include <common/time.h>
#include <drivers/graphics/graphics.h>
#include <services/fbs/font_atlas.h>

namespace eka2l1::epoc {
    font_atlas::font_atlas()
        : atlas_handle_(0)
        , atlas_data_(nullptr)
        , pack_handle_(0) {
    }

    void font_atlas::destroy(drivers::graphics_driver *driver) {
        if (atlas_handle_) {
            drivers::graphics_command_builder builder;
            builder.destroy_bitmap(atlas_handle_);

            drivers::command_list retrieved = builder.retrieve_command_list();
            driver->submit_command_list(retrieved);

            atlas_handle_ = 0;
            atlas_data_.reset();
        }

        last_use_.clear();
        characters_.clear();
    }
}