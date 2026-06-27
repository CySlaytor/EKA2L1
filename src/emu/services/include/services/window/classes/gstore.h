#pragma once

#include <common/region.h>
#include <common/vecx.h>
#include <cstdint>
#include <drivers/graphics/common.h>
#include <drivers/itc.h>
#include <memory>

namespace eka2l1::drivers {
    class graphics_driver;
}

namespace eka2l1::epoc {
    class bitmap_cache;

    enum gdi_store_command_opcode : std::uint32_t {
        gdi_store_command_invalid,
        gdi_store_command_draw_rect,
        gdi_store_command_set_clip_rect_single
    };

    struct gdi_store_command_draw_rect_data {
        eka2l1::vec4 color_;
        eka2l1::rect rect_;
    };

    struct gdi_store_command_set_clip_rect_single_data {
        eka2l1::rect clipping_rect_;
    };

    static constexpr std::size_t MAX_COMMAND_STORE_DATA_SIZE = 64;

    struct gdi_store_command {
        gdi_store_command_opcode opcode_ = gdi_store_command_invalid;
        std::uint8_t data_[MAX_COMMAND_STORE_DATA_SIZE];
        std::shared_ptr<std::vector<std::uint8_t>> dynamic_data_;

        template <typename T>
        T &get_data_struct() {
            return *reinterpret_cast<T *>(data_);
        }

        template <typename T>
        const T &get_data_struct_const() const {
            return *reinterpret_cast<const T *>(data_);
        }
    };

    enum gdi_store_command_segment_type {
        gdi_store_command_segment_non_redraw,
        gdi_store_command_segment_pending_redraw,
        gdi_store_command_segment_redraw
    };

    struct gdi_store_command_segment {
        gdi_store_command_segment_type type_;
        std::uint64_t creation_date_;
        common::region region_;
        std::vector<gdi_store_command> commands_;
        std::vector<void *> font_objects_;
        std::vector<void *> bitmap_objects_;

        ~gdi_store_command_segment();
        void add_command(gdi_store_command &cmd);
    };

    class gdi_store_command_collection {
    private:
        std::vector<std::unique_ptr<gdi_store_command_segment>> segments_;
        gdi_store_command_segment *current_segment_;

    public:
        explicit gdi_store_command_collection();
        gdi_store_command_segment *add_new_segment(const eka2l1::rect &draw_rect, const gdi_store_command_segment_type type_);
        void promote_last_segment();

        gdi_store_command_segment *get_current_segment() const {
            return current_segment_;
        }

        const std::vector<std::unique_ptr<gdi_store_command_segment>> &get_segments() const {
            return segments_;
        }
    };

    class gdi_command_builder {
    private:
        drivers::graphics_driver *driver_;
        drivers::graphics_command_builder &builder_;
        bitmap_cache &bcache_;
        float scale_factor_;
        eka2l1::vec2 position_;
        common::region clip_;
        drivers::filter_option texture_filter_;

    public:
        explicit gdi_command_builder(drivers::graphics_driver *drv, drivers::graphics_command_builder &builder, bitmap_cache &bcache,
            drivers::filter_option texture_filter, const eka2l1::vec2 &position, float scale_factor, const common::region &clip);

        void build_segment(const gdi_store_command_segment &segment);
        void build_single_command(const gdi_store_command &command);
        void build_command_draw_rect(const gdi_store_command_draw_rect_data &cmd);
        void build_command_set_clip_rect_single(const gdi_store_command_set_clip_rect_single_data &cmd);
    };
}