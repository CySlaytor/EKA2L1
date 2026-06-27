#pragma once

#include <drivers/graphics/common.h>
#include <drivers/graphics/graphics.h>
#include <services/window/classes/winuser.h>

#include <common/linked.h>
#include <common/region.h>
#include <common/rgb.h>
#include <common/vecx.h>

#include <queue>
#include <string>

namespace eka2l1 {
    struct fbsfont;
}

namespace eka2l1::epoc {
    struct bitwise_bitmap;

    enum class brush_style {
        null = 0,
        solid = 1,
        pattern = 2,
        vertical_hatch = 3,
        foward_diagonal_hatch = 4,
        horizontal_hatch = 5,
        rearward_diagonal_hatch = 6,
        square_cross_hatch = 7,
        diamond_cross_hatch = 8
    };

    enum class pen_style {
        null = 0,
        solid = 1,
        dotted = 2,
        dashed = 3,
        dot_dash = 4,
        dot_dot_dash = 5
    };

    struct graphic_context : public window_client_obj {
        canvas_base *attached_window;

        common::double_linked_queue_element context_attach_link;
        fbsfont *text_font;

        bool recording{ false };
        bool flushed{ false };

        brush_style fill_mode;
        pen_style line_mode;

        common::rgba brush_color;
        common::rgba pen_color;

        eka2l1::vec2 pen_size;
        eka2l1::rect clipping_rect;
        common::region clipping_region;

        void submit_queue_commands(kernel::thread *rq);
        void on_command_batch_done(service::ipc_context &ctx) override;

        enum class set_color_type {
            brush,
            pen
        };

        bool no_building() const;
        void do_submit_clipping();

        void active(service::ipc_context &context, ws_cmd cmd);
        void deactive(service::ipc_context &context, ws_cmd &cmd);
        void set_brush_color(service::ipc_context &context, ws_cmd &cmd);
        void set_brush_style(service::ipc_context &context, ws_cmd &cmd);
        void set_pen_style(service::ipc_context &context, ws_cmd &cmd);
        void clear_rect(service::ipc_context &context, ws_cmd &cmd);
        void destroy(service::ipc_context &context, ws_cmd &cmd);

        bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;

        explicit graphic_context(window_server_client_ptr client, epoc::window *attach_win = nullptr);
        ~graphic_context() override;
    };
}