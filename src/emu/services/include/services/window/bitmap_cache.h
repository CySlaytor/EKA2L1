#pragma once

#include <array>
#include <common/vecx.h>
#include <drivers/graphics/common.h>
#include <drivers/itc.h>
#include <services/fbs/bitmap.h>

namespace eka2l1 {
    class kernel_system;
    class fbs_server;
}

namespace eka2l1::epoc {
    constexpr std::uint32_t MAX_CACHE_SIZE = 1024;

    class bitmap_cache {
    public:
        using driver_texture_handle_array = std::array<drivers::handle, MAX_CACHE_SIZE>;

    private:
        driver_texture_handle_array driver_textures;

        fbs_server *fbss_;
        kernel_system *kern;
        drivers::graphics_driver *driver;

    public:
        explicit bitmap_cache(kernel_system *kern_);
        void clean(drivers::graphics_driver *drv);
    };
}