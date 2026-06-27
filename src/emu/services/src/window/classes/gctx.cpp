#include <common/cvt.h>
#include <common/log.h>
#include <common/rgb.h>
#include <services/fbs/fbs.h>
#include <services/window/classes/bitmap.h>
#include <services/window/classes/gctx.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/util.h>
#include <services/window/window.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    bool graphic_context::no_building() const {
        return !attached_window || (attached_window->abs_rect.size == eka2l1::vec2(0, 0));
    }

    void graphic_context::active(service::ipc_context &context, ws_cmd cmd) {
        const std::uint32_t window_to_attach_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        attached_window = reinterpret_cast<epoc::canvas_base *>(client->get_object(window_to_attach_handle));
        attached_window->attached_contexts.push(&context_attach_link);
        context.complete(attached_window->scr->number);

        if (no_building()) {
            return;
        }

        kernel_system *kern = context.sys->get_kernel_system();
        kern->unlock();

        attached_window->prepare_for_draw();

        kern->lock();
        recording = true;
        clipping_rect.make_empty();
        clipping_region.make_empty();
        brush_color = attached_window->clear_color;
        do_submit_clipping();
    }

    void graphic_context::do_submit_clipping() {
        eka2l1::rect the_clip;
        epoc::gdi_store_command cmd;

        if (attached_window->flags & epoc::canvas_base::flags_in_redraw) {
            epoc::redraw_msg_canvas *attached_fm_window = reinterpret_cast<epoc::redraw_msg_canvas *>(attached_window);
            the_clip = attached_fm_window->redraw_rect_curr;

            cmd.opcode_ = epoc::gdi_store_command_set_clip_rect_single;
            epoc::gdi_store_command_set_clip_rect_single_data &data = cmd.get_data_struct<epoc::gdi_store_command_set_clip_rect_single_data>();
            data.clipping_rect_ = the_clip;
            attached_window->add_draw_command(cmd);
        }
    }

    void graphic_context::set_brush_color(service::ipc_context &context, ws_cmd &cmd) {
        kernel_system *kern = context.sys->get_kernel_system();
        brush_color = *reinterpret_cast<const common::rgba *>(cmd.data_ptr);
        if (!kern->is_eka1()) {
            brush_color = (brush_color & 0xFF00FF00) | ((brush_color & 0xFF) << 16) | ((brush_color & 0xFF0000) >> 16);
        }
        context.complete(epoc::error_none);
    }

    void graphic_context::on_command_batch_done(service::ipc_context &ctx) {
        if (attached_window) {
            submit_queue_commands(ctx.msg->own_thr);
        }
    }

    void graphic_context::submit_queue_commands(kernel::thread *rq) {
        if (!flushed) {
            if ((attached_window->flags & window::flags_in_redraw) == 0) {
                attached_window->try_update(rq);
            }
            flushed = true;
        }
    }

    void graphic_context::deactive(service::ipc_context &context, ws_cmd &cmd) {
        if (attached_window) {
            context_attach_link.deque();
            submit_queue_commands(context.msg->own_thr);
        }
        attached_window = nullptr;
        recording = false;
        context.complete(epoc::error_none);
    }

    void graphic_context::set_brush_style(service::ipc_context &context, ws_cmd &cmd) {
        fill_mode = *reinterpret_cast<brush_style *>(cmd.data_ptr);
        context.complete(epoc::error_none);
    }

    void graphic_context::set_pen_style(service::ipc_context &context, ws_cmd &cmd) {
        line_mode = *reinterpret_cast<pen_style *>(cmd.data_ptr);
        context.complete(epoc::error_none);
    }

    void graphic_context::clear_rect(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect area = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);
        area.transform_from_symbian_rectangle();
        if (!area.valid()) {
            context.complete(epoc::error_none);
            return;
        }

        epoc::gdi_store_command gdi_cmd;
        epoc::gdi_store_command_draw_rect_data &rect_draw_data = gdi_cmd.get_data_struct<epoc::gdi_store_command_draw_rect_data>();
        rect_draw_data.rect_ = area;
        rect_draw_data.color_ = common::rgba_to_vec(brush_color);
        gdi_cmd.opcode_ = epoc::gdi_store_command_draw_rect;

        if (!epoc::is_display_mode_alpha(attached_window->display_mode())) {
            rect_draw_data.color_.w = 255;
        }

        attached_window->add_draw_command(gdi_cmd);
        context.complete(epoc::error_none);
    }

    void graphic_context::destroy(service::ipc_context &context, ws_cmd &cmd) {
        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    bool graphic_context::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_graphics_context_opcode op = static_cast<decltype(op)>(cmd.header.op);

        using ws_graphics_context_op_handler = std::function<void(graphic_context *,
            service::ipc_context & ctx, ws_cmd & cmd)>;
        using ws_graphics_context_table_op = std::map<ws_graphics_context_opcode, std::tuple<ws_graphics_context_op_handler, bool, bool>>;

        static const ws_graphics_context_table_op v139u_opcode_handlers = {
            { ws_gc_u139_active, { &graphic_context::active, false, false } },
            { ws_gc_u139_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_u139_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_u139_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_u139_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_u139_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_u139_free, { &graphic_context::destroy, true, true } }
        };

        static const ws_graphics_context_table_op v151u_m1_opcode_handlers = {
            { ws_gc_u151m1_active, { &graphic_context::active, false, false } },
            { ws_gc_u151m1_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_u151m1_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_u151m1_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_u151m1_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_u151m1_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_u151m1_free, { &graphic_context::destroy, true, true } }
        };

        static const ws_graphics_context_table_op v151u_m2_opcode_handlers = {
            { ws_gc_u151m2_active, { &graphic_context::active, false, false } },
            { ws_gc_u151m2_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_u151m2_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_u151m2_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_u151m2_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_u151m2_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_u151m2_free, { &graphic_context::destroy, true, true } }
        };

        static const ws_graphics_context_table_op curr_opcode_handlers = {
            { ws_gc_curr_active, { &graphic_context::active, false, false } },
            { ws_gc_curr_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_curr_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_curr_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_curr_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_curr_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_curr_free, { &graphic_context::destroy, true, true } }
        };

        epoc::version cli_ver = client->client_version();
        ws_graphics_context_op_handler handler = nullptr;
        bool need_to_set_flushed = false;
        bool need_quit = false;

#define FIND_OPCODE(op, table)                                                               \
    auto result = table.find(op);                                                            \
    if (result == table.end()) {                                                             \
        LOG_WARN(SERVICE_WINDOW, "Unimplemented graphics context opcode {}", cmd.header.op); \
        return false;                                                                        \
    }                                                                                        \
    if (!std::get<0>(result->second)) {                                                      \
        return false;                                                                        \
    }                                                                                        \
    handler = std::get<0>(result->second);                                                   \
    need_to_set_flushed = std::get<1>(result->second);                                       \
    need_quit = std::get<2>(result->second);

        kernel_system *kern = client->get_ws().get_kernel_system();

        if (cli_ver.major == 1 && cli_ver.minor == 0) {
            if (cli_ver.build <= WS_OLDARCH_VER) {
                FIND_OPCODE(op, v139u_opcode_handlers)
            } else if (cli_ver.build <= WS_NEWARCH_VER) {
                if (kern->get_epoc_version() <= epocver::epoc80) {
                    FIND_OPCODE(op, v139u_opcode_handlers)
                } else if (kern->get_epoc_version() <= epocver::epoc81b) {
                    FIND_OPCODE(op, v151u_m1_opcode_handlers)
                } else if (kern->get_epoc_version() <= epocver::epoc94) {
                    FIND_OPCODE(op, v151u_m2_opcode_handlers)
                } else {
                    FIND_OPCODE(op, curr_opcode_handlers)
                }
            } else {
                FIND_OPCODE(op, curr_opcode_handlers)
            }
        }

        if (need_to_set_flushed) {
            flushed = false;
        }

        handler(this, ctx, cmd);
        return need_quit;
    }

    graphic_context::graphic_context(window_server_client_ptr client, epoc::window *attach_win)
        : window_client_obj(client, nullptr)
        , attached_window(reinterpret_cast<epoc::canvas_base *>(attach_win))
        , text_font(nullptr)
        , fill_mode(brush_style::null)
        , line_mode(pen_style::solid)
        , brush_color(0xFFFFFFFF)
        , pen_color(0)
        , pen_size(1, 1) {
    }

    graphic_context::~graphic_context() {
        context_attach_link.deque();
    }
}