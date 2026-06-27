#pragma once

#include <common/region.h>
#include <services/context.h>
#include <services/window/opheader.h>

#include <optional>

namespace eka2l1 {
    namespace drivers {
        class graphics_command_builder;
    }

    void scale_rectangle(eka2l1::rect &r, const float scale_factor);
}