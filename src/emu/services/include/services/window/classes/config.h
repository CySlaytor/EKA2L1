#pragma once

#include <common/vecx.h>
#include <services/window/common.h>
#include <string>
#include <vector>

namespace eka2l1::epoc {
    namespace config {
        struct screen_mode {
            int screen_number;
            int mode_number;

            eka2l1::vec2 size;
            int rotation;

            std::string style;
        };

        struct hardware_state {
            int state_number;
            int mode_normal;
            int mode_alternative;
            int switch_keycode;
        };

        struct screen {
            int screen_number;
            epoc::display_mode disp_mode;

            bool auto_clear;
            bool flicker_free;
            bool blt_offscreen;

            std::vector<screen_mode> modes;
            std::vector<hardware_state> hardware_states;
        };
    }
}