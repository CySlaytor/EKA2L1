#include <common/algorithm.h>
#include <common/log.h>
#include <common/time.h>
#include <services/window/common.h>

namespace eka2l1::epoc {
    event::event(const std::uint32_t handle, event_code evt_code)
        : handle(handle)
        , type(evt_code)
        , time(common::get_current_utc_time_in_microseconds_since_0ad()) {
    }

    bool is_display_mode_color(const epoc::display_mode disp_mode) {
        return disp_mode >= epoc::display_mode::color16;
    }

    bool is_display_mode_mono(const epoc::display_mode disp_mode) {
        return disp_mode <= epoc::display_mode::gray256;
    }

    bool is_display_mode_alpha(const display_mode disp_mode) {
        return disp_mode == display_mode::color16map || disp_mode == display_mode::color16ma;
    }

    int get_bpp_from_display_mode(const epoc::display_mode bpp) {
        switch (bpp) {
        case epoc::display_mode::gray2:
            return 1;
        case epoc::display_mode::gray4:
            return 2;
        case epoc::display_mode::gray16:
        case epoc::display_mode::color16:
            return 4;
        case epoc::display_mode::gray256:
        case epoc::display_mode::color256:
            return 8;
        case epoc::display_mode::color4k:
            return 12;
        case epoc::display_mode::color64k:
            return 16;
        case epoc::display_mode::color16m:
            return 24;
        case epoc::display_mode::color16mu:
        case epoc::display_mode::color16ma:
        case epoc::display_mode::color16map:
            return 32;
        default:
            return 24;
        }
    }

    int get_num_colors_from_display_mode(const epoc::display_mode disp_mode) {
        switch (disp_mode) {
        case epoc::display_mode::gray2:
            return 2;
        case epoc::display_mode::gray4:
            return 4;
        case epoc::display_mode::gray16:
        case epoc::display_mode::color16:
            return 16;
        case epoc::display_mode::gray256:
        case epoc::display_mode::color256:
            return 256;
        case epoc::display_mode::color4k:
            return 4096;
        case epoc::display_mode::color64k:
            return 65536;
        case epoc::display_mode::color16m:
        case epoc::display_mode::color16mu:
        case epoc::display_mode::color16ma:
        case epoc::display_mode::color16map:
            return 16777216;
        default:
            return 0;
        }
    }

    epoc::display_mode get_display_mode_from_bpp(const int bpp, const bool has_color) {
        switch (bpp) {
        case 1:
            return epoc::display_mode::gray2;
        case 2:
            return epoc::display_mode::gray4;
        case 4:
            return (has_color ? epoc::display_mode::color16 : epoc::display_mode::gray16);
        case 8:
            return (has_color ? epoc::display_mode::color256 : epoc::display_mode::gray256);
        case 12:
            return epoc::display_mode::color4k;
        case 16:
            return epoc::display_mode::color64k;
        case 24:
            return epoc::display_mode::color16m;
        case 32:
            return epoc::display_mode::color16ma;
        default:
            break;
        }
        return epoc::display_mode::color_last;
    }

    int get_byte_width(const std::uint32_t pixels_width, const std::uint8_t bits_per_pixel) {
        int word_width = 0;
        switch (bits_per_pixel) {
        case 1:
            word_width = (pixels_width + 31) / 32;
            break;
        case 2:
            word_width = (pixels_width + 15) / 16;
            break;
        case 4:
            word_width = (pixels_width + 7) / 8;
            break;
        case 8:
            word_width = (pixels_width + 3) / 4;
            break;
        case 12:
        case 16:
            word_width = (pixels_width + 1) / 2;
            break;
        case 24:
            word_width = (((pixels_width * 3) + 11) / 12) * 3;
            break;
        case 32:
            word_width = pixels_width;
            break;
        default:
            assert(false);
            break;
        }
        return word_width * 4;
    }

    epoc::display_mode string_to_display_mode(const std::string &disp_str) {
        const std::string disp_str_lower = common::lowercase_string(disp_str);
        if (disp_str_lower == "color16map")
            return epoc::display_mode::color16map;
        if (disp_str_lower == "color16ma")
            return epoc::display_mode::color16ma;
        if (disp_str_lower == "color16mu")
            return epoc::display_mode::color16mu;
        if (disp_str_lower == "color64k")
            return epoc::display_mode::color64k;
        return epoc::display_mode::color16ma;
    }

    key_code map_scancode_to_keycode(std_scan_code scan_code) {
        if (scan_code <= std_key_scroll_lock) {
            return keymap[scan_code];
        } else if (scan_code > std_key_scroll_lock && scan_code < std_key_f1) {
            return static_cast<key_code>(scan_code);
        } else if (scan_code >= std_key_f1 && scan_code <= std_key_application_27) {
            return keymap[scan_code - 67];
        }
        return key_null;
    }

    std_scan_code post_processing_scancode(std_scan_code resulted, const int rotation) {
        const std_scan_code ROUND_ARROW_MAP[] = {
            std_key_right_arrow, std_key_up_arrow, std_key_left_arrow, std_key_down_arrow
        };
        int index = -1;
        switch (resulted) {
        case std_key_right_arrow:
            index = 0;
            break;
        case std_key_up_arrow:
            index = 1;
            break;
        case std_key_left_arrow:
            index = 2;
            break;
        case std_key_down_arrow:
            index = 3;
            break;
        default:
            break;
        }
        if (index == -1)
            return resulted;
        return ROUND_ARROW_MAP[(index + (rotation / 90)) % 4];
    }

    std::optional<std::uint32_t> map_key_to_inputcode(key_map &map, std::uint32_t keycode) {
        auto it = map.find(keycode);
        if (it == map.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    int get_approximate_pixel_to_twips_mul(const epocver ver) {
        switch (ver) {
        case epocver::epoc6:
        case epocver::epoc7:
        case epocver::epoc80:
        case epocver::epoc81a:
        case epocver::epoc81b:
            return 15;
        case epocver::epoc93fp1:
        case epocver::epoc93fp2:
        case epocver::epoc94:
            return 9;
        default:
            break;
        }
        return 8;
    }

    graphics_orientation number_to_orientation(int rot) {
        switch (rot) {
        case 0:
            return graphics_orientation::normal;
        case 90:
            return graphics_orientation::rotated90;
        case 180:
            return graphics_orientation::rotated180;
        case 270:
            return graphics_orientation::rotated270;
        default:
            return graphics_orientation::normal;
        }
    }
}