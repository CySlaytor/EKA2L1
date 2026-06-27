#pragma once

#include <common/uid.h>
#include <loader/mbm.h>
#include <mem/ptr.h>
#include <services/window/common.h>

namespace eka2l1 {
    class fbs_server;
}

namespace eka2l1::epoc {
    static constexpr epoc::uid bitwise_bitmap_uid = 0x10000040;
    static constexpr std::uint32_t LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE = 50;

    static constexpr std::uint32_t NORMAL_BITMAP_UID_REV2 = 0x9A2C;
    static constexpr std::uint32_t NVG_BITMAP_UID_REV2 = 0x39B9273E;

    static const std::uint32_t SUPPORTED_REV2_UIDS[] = {
        NVG_BITMAP_UID_REV2
    };

    static const std::size_t SUPPORTED_REV2_UID_COUNT = sizeof(SUPPORTED_REV2_UIDS) / sizeof(std::uint32_t);

    enum bitmap_file_compression {
        bitmap_file_no_compression = 0,
        bitmap_file_byte_rle_compression = 1,
        bitmap_file_twelve_bit_rle_compression = 2,
        bitmap_file_sixteen_bit_rle_compression = 3,
        bitmap_file_twenty_four_bit_rle_compression = 4,
        bitmap_file_thirty_two_u_bit_rle_compression = 5,
        bitmap_file_thirty_two_a_bit_rle_compression = 6,
        bitmap_file_palette_compression = 7
    };

    enum bitmap_color {
        monochrome_bitmap = 0,
        color_bitmap = 1,
        color_bitmap_with_alpha = 2,
        color_bitmap_with_alpha_pm = 3
    };

    struct bitwise_bitmap {
        enum settings_flag {
            large_bitmap = 0x00010000,
            dirty_bitmap = 0x00010000,
            violate_bitmap = 0x00020000
        };

        uid uid_;

        struct settings {
            std::uint32_t flags_{ 0 };

            display_mode initial_display_mode() const;
            display_mode current_display_mode() const;

            void current_display_mode(const display_mode &mode);
            void initial_display_mode(const display_mode &mode);

            bool is_large() const;
            void set_large(const bool result);

            bool dirty_bitmap() const;
            void dirty_bitmap(const bool is_it);

            bool violate_bitmap() const;
            void violate_bitmap(const bool is_it);

            void set_width(const std::uint16_t bpp);
            std::uint16_t get_width() const;
        } settings_;

        eka2l1::ptr<void> allocator_;
        eka2l1::ptr<void> pile_;
        int byte_width_;
        loader::sbm_header header_;
        int spare1_;
        int data_offset_;
        int compressed_in_ram_;
        bool offset_from_me_;

        void construct(loader::sbm_header &info, epoc::display_mode disp_mode, void *data, const void *base,
            const bool support_current_display_mode_flag, const bool white_fill = false);

        void post_construct(fbs_server *serv);
        int copy_to(std::uint8_t *dest, const eka2l1::vec2 &dest_size, fbs_server *serv);

        bitmap_file_compression compression_type() const;

        std::uint8_t *data_pointer(fbs_server *serv);
        std::uint32_t data_size() const;
    };
}