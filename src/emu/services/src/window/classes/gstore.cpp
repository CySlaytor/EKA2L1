#include <services/window/bitmap_cache.h>
#include <services/window/classes/gstore.h>
#include <services/window/util.h>

#include <services/fbs/bitmap.h>
#include <services/fbs/fbs.h>
#include <services/fbs/font.h>

#include <common/algorithm.h>
#include <common/time.h>

namespace eka2l1::epoc {
    gdi_store_command_segment::~gdi_store_command_segment() {
        for (std::size_t i = 0; i < font_objects_.size(); i++) {
            reinterpret_cast<fbsfont *>(font_objects_[i])->deref();
        }

        for (std::size_t i = 0; i < bitmap_objects_.size(); i++) {
            reinterpret_cast<fbsbitmap *>(bitmap_objects_[i])->deref();
        }
    }

    void gdi_store_command_segment::add_command(gdi_store_command &command) {
        commands_.push_back(command);
    }

    gdi_store_command_collection::gdi_store_command_collection()
        : current_segment_(nullptr) {
    }

    gdi_store_command_segment *gdi_store_command_collection::add_new_segment(const eka2l1::rect &draw_rect, const gdi_store_command_segment_type type) {
        std::unique_ptr<gdi_store_command_segment> new_segment = std::make_unique<gdi_store_command_segment>();

        new_segment->type_ = type;
        new_segment->creation_date_ = common::get_current_utc_time_in_microseconds_since_epoch();
        new_segment->region_.add_rect(draw_rect);

        gdi_store_command_segment *new_segment_ptr = new_segment.get();
        segments_.push_back(std::move(new_segment));

        current_segment_ = new_segment_ptr;
        return new_segment_ptr;
    }

    void gdi_store_command_collection::promote_last_segment() {
        if (segments_.empty()) {
            return;
        }

        gdi_store_command_segment *lastest_segment = segments_.back().get();
        if (lastest_segment->type_ != gdi_store_command_segment_pending_redraw) {
            return;
        }

        for (std::size_t i = 0; i < lastest_segment->region_.rects_.size(); i++) {
            for (std::size_t j = 0; j < segments_.size();) {
                if (segments_[j]->type_ != gdi_store_command_segment_pending_redraw) {
                    segments_[j]->region_.eliminate(lastest_segment->region_.rects_[i]);

                    if (segments_[j]->region_.empty()) {
                        segments_.erase(segments_.begin() + j);
                    } else {
                        j++;
                    }
                } else {
                    j++;
                }
            }
        }

        lastest_segment->type_ = gdi_store_command_segment_redraw;
    }

    gdi_command_builder::gdi_command_builder(drivers::graphics_driver *drv, drivers::graphics_command_builder &builder, bitmap_cache &bcache,
        drivers::filter_option texture_filter, const eka2l1::vec2 &position, float scale_factor, const common::region &clip)
        : driver_(drv)
        , builder_(builder)
        , bcache_(bcache)
        , scale_factor_(scale_factor)
        , position_(position)
        , clip_(clip)
        , texture_filter_(texture_filter) {
    }

    void gdi_command_builder::build_segment(const gdi_store_command_segment &segment) {
        for (std::size_t i = 0; i < segment.commands_.size(); i++) {
            build_single_command(segment.commands_[i]);
        }
    }

    void gdi_command_builder::build_single_command(const gdi_store_command &command) {
        switch (command.opcode_) {
        case gdi_store_command_draw_rect:
            build_command_draw_rect(command.get_data_struct_const<gdi_store_command_draw_rect_data>());
            break;

        case gdi_store_command_set_clip_rect_single:
            build_command_set_clip_rect_single(command.get_data_struct_const<gdi_store_command_set_clip_rect_single_data>());
            break;

        default:
            break;
        }
    }

    void gdi_command_builder::build_command_draw_rect(const gdi_store_command_draw_rect_data &cmd) {
        eka2l1::rect scaled_rect = cmd.rect_;
        scaled_rect.top += position_;

        scale_rectangle(scaled_rect, scale_factor_);

        builder_.set_brush_color_detail(cmd.color_);
        builder_.draw_rectangle(scaled_rect);
    }

    void gdi_command_builder::build_command_set_clip_rect_single(const gdi_store_command_set_clip_rect_single_data &cmd) {
        eka2l1::rect rect_advanced = cmd.clipping_rect_;
        rect_advanced.top += position_;

        common::region clipped;
        clipped.add_rect(rect_advanced);
        clipped = clipped.intersect(clip_);

        if ((clipped.rects_.size() == 1) && (clipped.rects_[0].size == eka2l1::vec2(1, 1))) {
            LOG_TRACE(KERNEL, "HI!");
        }

        builder_.clip_bitmap_region(clipped, scale_factor_);
    }
}