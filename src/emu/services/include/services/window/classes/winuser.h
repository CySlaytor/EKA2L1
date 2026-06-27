#pragma once

#include <common/linked.h>
#include <common/region.h>
#include <memory>
#include <services/window/classes/gstore.h>
#include <services/window/classes/winbase.h>
#include <services/window/common.h>

namespace eka2l1 {
    namespace drivers {
        class graphics_command_builder;
    }
}

namespace eka2l1::epoc {
    struct dsa;
    struct window_group;
    struct canvas_interface;
    struct canvas_base;

    struct canvas_observer {
    public:
        virtual ~canvas_observer() = default;
        virtual void on_window_size_changed(epoc::canvas_base *obj, const eka2l1::vec2 &new_size) {}
        virtual void on_window_size_changed(epoc::canvas_interface *obj) {}
    };

    struct canvas_interface : public window {
        virtual std::uint32_t redraw_priority(int *shift = nullptr) = 0;
        virtual eka2l1::vec2 absolute_position() const = 0;

        explicit canvas_interface(window_server_client_ptr client, screen *scr, window *parent)
            : window(client, scr, parent) {}

        explicit canvas_interface(window_server_client_ptr client, screen *scr, window *parent, window_kind type)
            : window(client, scr, parent, type) {}
    };

    struct canvas_base : public canvas_interface {
        epoc::display_mode dmode;
        epoc::window_type win_type;

        eka2l1::vec2 pos{ 0, 0 };
        eka2l1::rect abs_rect;

        bool resize_needed;
        bool clear_color_enable;

        common::roundabout attached_contexts;

        std::uint32_t clear_color;
        std::uint32_t filter;

        eka2l1::vec2 cursor_pos;

        common::region visible_region;
        common::region shape_region;

        int shadow_height;

        std::uint32_t max_pointer_buffer_;
        std::vector<epoc::event> pointer_buffer_;

        std::uint64_t last_draw_;
        std::uint64_t last_fps_sync_;
        std::uint64_t fps_count_;

        bool in_visibility_delay_report_;

        std::vector<dsa *> directs_;

        drivers::graphics_command_builder driver_builder_;
        std::unique_ptr<epoc::gdi_store_command_segment> pending_segment_;
        std::vector<canvas_observer *> observers_;

        explicit canvas_base(window_server_client_ptr client, screen *scr, window *parent, const epoc::window_type type_of_window, const epoc::display_mode dmode, const std::uint32_t client_handle);
        virtual ~canvas_base() override;

        virtual bool draw(drivers::graphics_command_builder &builder) = 0;
        virtual void on_activate() = 0;
        virtual void handle_extent_changed(const eka2l1::vec2 &new_size, const eka2l1::vec2 &new_pos) = 0;
        virtual void add_draw_command(gdi_store_command &command);
        virtual void prepare_for_draw() {}

        void add_canvas_observer(canvas_observer *ob);
        void remove_canvas_observer(canvas_observer *ob);

        epoc::display_mode display_mode() const;
        eka2l1::vec2 absolute_position() const override;

        std::uint32_t redraw_priority(int *shift = nullptr) override;
        eka2l1::rect bounding_rect() const;
        eka2l1::rect absolute_rect() const;
        eka2l1::vec2 size() const;
        eka2l1::vec2 size_for_egl_surface() const;

        void set_extent(const eka2l1::vec2 &top, const eka2l1::vec2 &size);
        void recalculate_absolute_position(const eka2l1::vec2 &diff);
        void report_visiblity_change(const bool forced = false);

        bool is_visible() const;
        bool can_be_physically_seen() const;

        bool is_faded() const { return (flags & flags_faded); }
        bool content_changed() const { return (flags & flag_content_changed); }
        void content_changed(const bool changed) {
            if (changed)
                flags |= flag_content_changed;
            else
                flags &= ~flag_content_changed;
        }

        bool is_dsa_active() const;
        void add_dsa_active(dsa *dsa);
        void remove_dsa_active(dsa *dsa);
        void set_visible(const bool vis);

        virtual std::uint64_t try_update(kernel::thread *drawer);
        void queue_event(const epoc::event &evt) override;

        void set_non_fading(service::ipc_context &context, ws_cmd &cmd);
        void set_size(service::ipc_context &context, ws_cmd &cmd);
        void activate(service::ipc_context &context, ws_cmd &cmd);
        void destroy(service::ipc_context &context, ws_cmd &cmd);
        void enable_visiblity_change_events(service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void inquire_offset(service::ipc_context &ctx, ws_cmd &cmd);

        epoc::window_group *get_group();

        bool execute_command_detail(service::ipc_context &ctx, ws_cmd &cmd, bool &did);
        bool execute_command(service::ipc_context &ctx, ws_cmd &cmd) override;
    };

    struct top_canvas : public canvas_interface {
        explicit top_canvas(window_server_client_ptr client, screen *scr, window *parent);
        std::uint32_t redraw_priority(int *shift = nullptr) override;
        eka2l1::vec2 absolute_position() const override;
    };

    struct blank_canvas : public canvas_base {
        explicit blank_canvas(window_server_client_ptr client, screen *scr, window *parent,
            const epoc::display_mode dmode, const std::uint32_t client_handle);

        void on_activate() override {}
        void handle_extent_changed(const eka2l1::vec2 &new_size, const eka2l1::vec2 &new_pos) override {}
        bool draw(drivers::graphics_command_builder &builder) override;
    };

    struct bitmap_backed_canvas : public canvas_base {
        explicit bitmap_backed_canvas(window_server_client_ptr client, screen *scr, window *parent, const epoc::display_mode dmode, const std::uint32_t client_handle);
        bool draw(drivers::graphics_command_builder &builder) override { return false; }
        void on_activate() override {}
        void handle_extent_changed(const eka2l1::vec2 &new_size, const eka2l1::vec2 &new_pos) override {}
    };

    struct redraw_msg_canvas : public canvas_base {
        common::region redraw_region;
        common::region background_region;
        eka2l1::rect redraw_rect_curr;

        gdi_store_command_collection redraw_segments_;

        explicit redraw_msg_canvas(window_server_client_ptr client, screen *scr, window *parent,
            const epoc::display_mode dmode, const std::uint32_t client_handle);

        void invalidate(const eka2l1::rect &irect);
        void on_activate() override;
        void handle_extent_changed(const eka2l1::vec2 &new_size, const eka2l1::vec2 &new_pos) override;
        void add_draw_command(gdi_store_command &command) override;
        std::uint64_t try_update(kernel::thread *drawer) override;

        void begin_redraw(service::ipc_context &context, ws_cmd &cmd);
        void end_redraw(service::ipc_context &context, ws_cmd &cmd);
        bool clear_redraw_store();
        void store_draw_commands(service::ipc_context &context, ws_cmd &cmd);
        void invalidate(service::ipc_context &context, ws_cmd &cmd);

        bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;
        bool draw(drivers::graphics_command_builder &builder) override;
    };
}