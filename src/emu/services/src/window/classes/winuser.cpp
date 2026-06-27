#include <services/fbs/fbs.h>
#include <services/window/classes/dsa.h>
#include <services/window/classes/gctx.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/screen.h>
#include <services/window/util.h>
#include <services/window/window.h>

#include <kernel/kernel.h>
#include <kernel/timing.h>

#include <common/log.h>
#include <common/vecx.h>
#include <config/config.h>

#include <drivers/itc.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    static constexpr std::uint8_t bits_per_ordpos = 4;
    static constexpr std::uint8_t max_ordpos_pri = 0b1111;
    static constexpr std::uint8_t max_pri_level = (sizeof(std::uint32_t) / bits_per_ordpos) - 1;

    top_canvas::top_canvas(window_server_client_ptr client, screen *scr, window *parent)
        : canvas_interface(client, scr, parent, window_kind::top_client) {
    }

    std::uint32_t top_canvas::redraw_priority(int *shift) {
        const std::uint32_t ordinal_pos = std::min<std::uint32_t>(max_ordpos_pri, ordinal_position(false));
        if (shift) {
            *shift = max_pri_level;
        }

        return (ordinal_pos << (max_pri_level * bits_per_ordpos));
    }

    eka2l1::vec2 top_canvas::absolute_position() const {
        return { 0, 0 };
    }

    canvas_base::canvas_base(window_server_client_ptr client, screen *scr, window *parent, const epoc::window_type type_of_window, const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_interface(client, scr, parent, window_kind::client)
        , win_type(type_of_window)
        , pos(0, 0)
        , resize_needed(false)
        , clear_color_enable(true)
        , clear_color(0xFFFFFFFF)
        , filter(pointer_filter_type::pointer_enter | pointer_filter_type::pointer_drag | pointer_filter_type::pointer_move)
        , cursor_pos(-1, -1)
        , dmode(dmode)
        , shadow_height(0)
        , max_pointer_buffer_(0)
        , last_draw_(0)
        , last_fps_sync_(0)
        , fps_count_(0)
        , in_visibility_delay_report_(false) {
        set_initial_state();

        abs_rect.top = reinterpret_cast<canvas_interface *>(parent)->absolute_position();
        abs_rect.size = eka2l1::vec2(0, 0);

        if (parent->type != epoc::window_kind::top_client && parent->type != epoc::window_kind::client) {
            LOG_ERROR(SERVICE_WINDOW, "Parent is not a window client type!");
        } else {
            if (parent->type == epoc::window_kind::client) {
                epoc::canvas_base *user = reinterpret_cast<epoc::canvas_base *>(parent);
                abs_rect.size = user->abs_rect.size;
            } else {
                abs_rect.size = scr->size();
            }
        }

        kernel_system *kern = client->get_ws().get_kernel_system();
        if (kern->get_epoc_version() >= epocver::epoc94) {
            flags |= flag_winmode_fixed;
        }

        set_client_handle(client_handle);
        scr->need_update_visible_regions(true);
    }

    canvas_base::~canvas_base() {
        if (in_visibility_delay_report_) {
            ntimer *timer = client->get_ws().get_ntimer();
            timer->unschedule_event(client->get_ws().get_deliver_delay_report_visiblity_event(),
                reinterpret_cast<std::uint64_t>(this));
        }

        set_visible(false);

        remove_from_sibling_list();

        if (scr) {
            scr->need_update_visible_regions(true);
        }

        client->remove_redraws(this);
    }

    void canvas_base::add_canvas_observer(canvas_observer *ob) {
        auto ite = std::find(observers_.begin(), observers_.end(), ob);
        if (ite == observers_.end()) {
            observers_.push_back(ob);
        }
    }

    void canvas_base::remove_canvas_observer(canvas_observer *ob) {
        auto ite = std::find(observers_.begin(), observers_.end(), ob);
        if (ite != observers_.end()) {
            observers_.erase(ite);
        }
    }

    bool canvas_base::is_dsa_active() const {
        return !directs_.empty();
    }

    void canvas_base::add_dsa_active(dsa *dsa) {
        auto result = std::find(directs_.begin(), directs_.end(), dsa);
        if (result == directs_.end()) {
            directs_.push_back(dsa);
        }
    }

    void canvas_base::remove_dsa_active(dsa *dsa) {
        auto result = std::find(directs_.begin(), directs_.end(), dsa);
        if (result != directs_.end()) {
            directs_.erase(result);
        }
    }

    void canvas_base::add_draw_command(gdi_store_command &command) {
        if (win_type == window_type::blank) {
            return;
        }

        if (!pending_segment_) {
            pending_segment_ = std::make_unique<gdi_store_command_segment>();
        }
        pending_segment_->add_command(command);
    }

    bool canvas_base::can_be_physically_seen() const {
        return is_visible() && !visible_region.empty();
    }

    bool canvas_base::is_visible() const {
        return ((flags & flags_active) && (flags & flags_visible));
    }

    eka2l1::rect canvas_base::bounding_rect() const {
        eka2l1::rect bound;
        bound.top = { 0, 0 };
        bound.size = abs_rect.size;

        return bound;
    }

    eka2l1::rect canvas_base::absolute_rect() const {
        return abs_rect;
    }

    eka2l1::vec2 canvas_base::size() const {
        return abs_rect.size;
    }

    eka2l1::vec2 canvas_base::size_for_egl_surface() const {
        if ((flags & epoc::window::flag_fix_native_orientation) && (abs_rect.size == scr->current_mode().size)) {
            return scr->size();
        }

        return abs_rect.size;
    }

    void canvas_base::report_visiblity_change(const bool forced) {
        if (!forced && in_visibility_delay_report_) {
            return;
        }

        if (forced) {
            in_visibility_delay_report_ = false;
        }

        if ((flags & flag_visiblity_event_report) == 0) {
            return;
        }

        if ((flags & flags_active) == 0) {
            return;
        }

        epoc::event visi_change_evt;
        visi_change_evt.type = epoc::event_code::window_visibility_change;
        visi_change_evt.time = client->get_ws().get_kernel_system()->home_time();
        visi_change_evt.handle = client_handle;
        visi_change_evt.win_visibility_change_evt_.flags_ = 0;

        if (visible_region.empty()) {
            visi_change_evt.win_visibility_change_evt_.flags_ = epoc::window_visiblity_changed_event::not_visible;
        } else {
            if (((flags & flag_shape_region) && (visible_region.identical(shape_region))) || (((flags & flag_shape_region) == 0) && (visible_region.rects_.size() == 1) && (visible_region.rects_[0] == bounding_rect()))) {
                visi_change_evt.win_visibility_change_evt_.flags_ = (epoc::window_visiblity_changed_event::partially_visible | epoc::window_visiblity_changed_event::fully_visible);
            } else {
                visi_change_evt.win_visibility_change_evt_.flags_ = epoc::window_visiblity_changed_event::partially_visible;
            }
        }

        window::queue_event(visi_change_evt);
    }

    void canvas_base::queue_event(const epoc::event &evt) {
        if (!is_visible()) {
            LOG_TRACE(SERVICE_WINDOW, "The client window 0x{:X} is not visible, and can't receive any events", id);
            return;
        }

        window::queue_event(evt);
    }

    struct window_absolute_postion_change_walker : public window_tree_walker {
        eka2l1::vec2 diff_;

        explicit window_absolute_postion_change_walker(const eka2l1::vec2 &diff)
            : diff_(diff) {
        }

        bool do_it(epoc::window *win) override {
            if (win->type == window_kind::client) {
                canvas_base *user = reinterpret_cast<canvas_base *>(win);
                user->recalculate_absolute_position(diff_);
            }

            return false;
        }
    };

    void canvas_base::recalculate_absolute_position(const eka2l1::vec2 &diff) {
        if (diff == eka2l1::vec2(0, 0)) {
            return;
        }

        abs_rect.top += diff;

        if (flags & flag_shape_region) {
            shape_region.advance(diff);
        }
    }

    void canvas_base::set_extent(const eka2l1::vec2 &top, const eka2l1::vec2 &new_size) {
        eka2l1::vec2 pos_diff = top - pos;

        const bool size_changed = (new_size != abs_rect.size);
        const bool pos_changed = (pos_diff != eka2l1::vec2(0, 0));

        handle_extent_changed(new_size, top);

        abs_rect.top += pos_diff;
        abs_rect.size = new_size;

        if (size_changed && !observers_.empty()) {
            for (auto ob : observers_) {
                ob->on_window_size_changed(this, new_size);
            }
        }

        pos = top;

        if (pos_changed) {
            window_absolute_postion_change_walker walker(pos_diff);
            walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children);
        }

        if (pos_changed || size_changed) {
            if (is_visible()) {
                scr->need_update_visible_regions(true);
            }
        }
    }

    static bool should_purge_canvas_base(void *win, epoc::event &evt) {
        epoc::canvas_base *user = reinterpret_cast<epoc::canvas_base *>(win);
        if (user->client_handle == evt.handle) {
            return false;
        }

        return true;
    }

    void canvas_base::set_visible(const bool vis) {
        bool should_trigger_redraw = false;
        bool current_visible_status = (flags & flags_visible) != 0;

        if (current_visible_status != vis) {
            should_trigger_redraw = true;
        } else {
            return;
        }

        flags &= ~flags_visible;

        if (vis) {
            flags |= flags_visible;
        } else {
            client->walk_event(should_purge_canvas_base, this);
        }

        if (should_trigger_redraw) {
            scr->need_update_visible_regions(true);

            client->get_ws().get_anim_scheduler()->schedule(client->get_ws().get_graphics_driver(),
                scr, client->get_ws().get_ntimer()->microseconds());
        }
    }

    std::uint32_t canvas_base::redraw_priority(int *child_shift) {
        int ordpos = ordinal_position(false);
        if (ordpos > max_ordpos_pri) {
            ordpos = max_ordpos_pri;
        }

        int shift = 0;
        int parent_pri = reinterpret_cast<canvas_interface *>(parent)->redraw_priority(&shift);

        if (shift > 0) {
            shift--;
        }

        if (child_shift) {
            *child_shift = shift;
        }

        return (parent_pri + (ordpos << (bits_per_ordpos * shift)));
    }

    epoc::window_group *canvas_base::get_group() {
        epoc::window_group *are_you = reinterpret_cast<epoc::window_group *>(parent);
        while (are_you->type != epoc::window_kind::group) {
            are_you = reinterpret_cast<epoc::window_group *>(are_you->parent);
        }

        return are_you;
    }

    eka2l1::vec2 canvas_base::absolute_position() const {
        return abs_rect.top;
    }

    epoc::display_mode canvas_base::display_mode() const {
        return scr->disp_mode;
    }

    std::uint64_t canvas_base::try_update(kernel::thread *drawer) {
        if (scr->need_update_visible_regions()) {
            scr->recalculate_visible_regions();
        }

        if (can_be_physically_seen()) {
            epoc::animation_scheduler *sched = client->get_ws().get_anim_scheduler();
            ntimer *timing = client->get_ws().get_ntimer();

            const std::uint64_t crr = timing->microseconds();
            const std::uint64_t time_spend_per_frame_us = 1000000 / scr->refresh_rate;

            std::uint64_t wait_time = 0;

            if (crr < time_spend_per_frame_us + last_draw_) {
                wait_time = time_spend_per_frame_us + last_draw_ - crr;
            } else {
                wait_time = 0;
            }

            last_draw_ = ((crr + time_spend_per_frame_us - 1) / time_spend_per_frame_us) * time_spend_per_frame_us;

            if (crr - last_fps_sync_ >= common::microsecs_per_sec) {
                scr->last_fps = fps_count_;

                last_fps_sync_ = crr;
                fps_count_ = 0;
            }

            fps_count_++;

            scr->set_client_draw_pending();

            sched->schedule(client->get_ws().get_graphics_driver(), scr, crr + wait_time);

            if (drawer) {
                drawer->sleep(static_cast<std::uint32_t>(wait_time));
            }

            content_changed(false);
            return wait_time;
        }

        return 0;
    }

    void canvas_base::set_non_fading(service::ipc_context &context, ws_cmd &cmd) {
        const std::uint32_t non_fading = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);

        if (non_fading) {
            flags |= flags_non_fading;
        } else {
            flags &= ~(flags_non_fading);
        }

        context.complete(epoc::error_none);
    }

    void canvas_base::set_size(service::ipc_context &context, ws_cmd &cmd) {
        const object_size new_size = *reinterpret_cast<object_size *>(cmd.data_ptr);
        set_extent(pos, new_size);
        context.complete(epoc::error_none);
    }

    void canvas_base::destroy(service::ipc_context &context, ws_cmd &cmd) {
        on_command_batch_done(context);

        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    bool redraw_msg_canvas::clear_redraw_store() {
        return true;
    }

    void redraw_msg_canvas::store_draw_commands(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_TRACE(SERVICE_WINDOW, "Store draw command stubbed");
        ctx.complete(epoc::error_none);
    }

    void redraw_msg_canvas::invalidate(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect prototype_irect;
        eka2l1::rect whole_win = bounding_rect();

        if (cmd.header.op == EWsWinOpInvalidate) {
            prototype_irect = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);
            prototype_irect.transform_from_symbian_rectangle();
        } else {
            prototype_irect = whole_win;
        }

        if (!prototype_irect.empty() && whole_win.contains(prototype_irect)) {
            invalidate(prototype_irect);

            if (scr->scr_config.flicker_free) {
                background_region.add_rect(prototype_irect);
            }
        }

        context.complete(epoc::error_none);
    }

    void redraw_msg_canvas::on_activate() {
        invalidate(bounding_rect());
    }

    void canvas_base::activate(service::ipc_context &context, ws_cmd &cmd) {
        flags |= flags_active;
        on_activate();

        if (is_visible()) {
            scr->need_update_visible_regions(true);
        }

        context.complete(epoc::error_none);
    }

    void canvas_base::enable_visiblity_change_events(service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        flags |= flag_visiblity_event_report;

        if (!in_visibility_delay_report_) {
            in_visibility_delay_report_ = true;

            window_server &ws = client->get_ws();
            ntimer *timer = ws.get_ntimer();

            timer->schedule_event(epoc::WS_DELIVER_REPORT_VISIBILITY_INIT_DELAY, ws.get_deliver_delay_report_visiblity_event(),
                reinterpret_cast<std::uint64_t>(this));
        } else {
            if (scr->need_update_visible_regions()) {
                scr->recalculate_visible_regions();
            }
        }

        ctx.complete(epoc::error_none);
    }

    void canvas_base::inquire_offset(service::ipc_context &ctx, ws_cmd &cmd) {
        const std::uint32_t handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        canvas_base *win = reinterpret_cast<canvas_base *>(client->get_object(handle));

        if (!win) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        if (win->type == epoc::window_kind::group) {
            win = reinterpret_cast<canvas_base *>(win->child);

            if (win->type != epoc::window_kind::top_client) {
                LOG_ERROR(SERVICE_WINDOW, "Inquire offset with a corrupted window group!");
                ctx.complete(epoc::error_general);

                return;
            }
        }

        eka2l1::vec2 offset_dist = absolute_position() - win->absolute_position();
        ctx.write_data_to_descriptor_argument(reply_slot, offset_dist);
        ctx.complete(epoc::error_none);
    }

    bool canvas_base::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        bool useless = false;
        return execute_command_detail(ctx, cmd, useless);
    }

    bool canvas_base::execute_command_detail(service::ipc_context &ctx, ws_cmd &cmd, bool &did_it) {
        bool result = execute_command_for_general_node(ctx, cmd);
        bool quit = false;

        did_it = true;

        if (result) {
            return false;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpRequiredDisplayMode: {
            if (!(flags & flag_winmode_fixed)) {
                dmode = *reinterpret_cast<epoc::display_mode *>(cmd.data_ptr);
                if (epoc::get_num_colors_from_display_mode(dmode) > epoc::get_num_colors_from_display_mode(scr->disp_mode)) {
                    dmode = scr->disp_mode;
                }
            }
            ctx.complete(static_cast<int>(dmode));
            break;
        }

        case EWsWinOpGetDisplayMode: {
            ctx.complete(static_cast<int>(dmode));
            break;
        }

        case EWsWinOpSetExtent:
        case EWsWinOpSetExtentErr: {
            ws_cmd_set_extent *extent = reinterpret_cast<decltype(extent)>(cmd.data_ptr);
            set_extent(extent->pos, extent->size);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetPos: {
            eka2l1::vec2 *pos_to_set = reinterpret_cast<eka2l1::vec2 *>(cmd.data_ptr);
            set_extent(*pos_to_set, size());
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSize: {
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, abs_rect.size);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetVisible: {
            const std::uint32_t visible = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);

            set_visible(visible != 0);
            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpSetNonFading: {
            set_non_fading(ctx, cmd);
            break;
        }

        case EWsWinOpSetShadowHeight: {
            shadow_height = *reinterpret_cast<int *>(cmd.data_ptr);
            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpShadowDisabled: {
            flags &= ~flags_shadow_disable;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= flags_shadow_disable;
            }

            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpSetBackgroundColor:
        case EWsWinOpSetNoBackgroundColor: {
            if ((cmd.header.cmd_len == 0) && (cmd.header.op == EWsWinOpSetNoBackgroundColor)) {
                LOG_TRACE(SERVICE_WINDOW, "Set no clear for {}", id);
                clear_color_enable = false;
                ctx.complete(epoc::error_none);

                break;
            }

            clear_color = *reinterpret_cast<int *>(cmd.data_ptr);
            clear_color_enable = true;

            kernel_system *kern = client->get_ws().get_kernel_system();

            if (!kern->is_eka1()) {
                clear_color = (clear_color & 0xFF00FF00) | ((clear_color & 0xFF) << 16) | ((clear_color & 0xFF0000) >> 16);
            }

            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpPointerFilter: {
            ws_cmd_pointer_filter *filter_info = reinterpret_cast<ws_cmd_pointer_filter *>(cmd.data_ptr);
            filter &= ~filter_info->mask;
            filter |= filter_info->flags;

            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetPointerGrab: {
            flags &= ~flags_allow_pointer_grab;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= flags_allow_pointer_grab;
            }

            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetPointerCapture: {
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpActivate:
            activate(ctx, cmd);
            break;

        case EWsWinOpSetSize:
        case EWsWinOpSetSizeErr: {
            set_size(ctx, cmd);
            break;
        }

        case EWsWinOpGetIsFaded:
            ctx.complete(static_cast<bool>(flags & flags_faded));
            break;

        case EWsWinOpFree:
            destroy(ctx, cmd);
            quit = true;
            break;

        case EWsWinOpWindowGroupId:
            ctx.complete(get_group()->id);
            break;

        case EWsWinOpPosition:
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, pos);
            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpAbsPosition:
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, absolute_position());
            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpSetCornerType:
            LOG_WARN(SERVICE_WINDOW, "SetCornerType stubbed");

            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpSetColor:
            LOG_WARN(SERVICE_WINDOW, "SetColor stubbed");

            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpEnableVisibilityChangeEvents:
            enable_visiblity_change_events(ctx, cmd);
            break;

        case EWsWinOpSetSurfaceTransparency:
        case EWsWinOpSendEffectCommand:
            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpInquireOffset:
            inquire_offset(ctx, cmd);
            break;

        default: {
            did_it = false;
            break;
        }
        }

        return quit;
    }

    bitmap_backed_canvas::bitmap_backed_canvas(window_server_client_ptr client, screen *scr, window *parent, const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_base(client, scr, parent, epoc::window_type::backed_up, dmode, client_handle) {
    }

    blank_canvas::blank_canvas(window_server_client_ptr client, screen *scr, window *parent,
        const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_base(client, scr, parent, window_type::blank, dmode, client_handle) {
    }

    bool blank_canvas::draw(drivers::graphics_command_builder &builder) {
        if (!clear_color_enable || !can_be_physically_seen() || (!scr->is_screenplay_architecture() && scr->scr_config.blt_offscreen)) {
            return false;
        }

        if ((scr->flags_ & screen::FLAG_SERVER_REDRAW_PENDING) == 0) {
            return false;
        }

        driver_builder_.reset_list();

        eka2l1::vec2 abs_pos = absolute_position();
        auto color_extracted = common::rgba_to_vec(clear_color);

        builder.set_feature(drivers::graphics_feature::blend, false);
        builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

        if (display_mode() <= epoc::display_mode::color16mu) {
            color_extracted.w = 255;
        }

        builder.set_brush_color_detail(color_extracted);
        builder.draw_rectangle(eka2l1::rect(abs_pos * scr->display_scale_factor, { 0, 0 }));

        return true;
    }

    redraw_msg_canvas::redraw_msg_canvas(window_server_client_ptr client, screen *scr, window *parent,
        const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_base(client, scr, parent, epoc::window_type::redraw, dmode, client_handle) {
    }

    void redraw_msg_canvas::handle_extent_changed(const eka2l1::vec2 &new_size, const eka2l1::vec2 &new_pos) {
        if (new_size != abs_rect.size) {
            resize_needed = true;

            eka2l1::rect new_bounding_rect(eka2l1::vec2{ 0, 0 }, new_size);

            if ((new_size.x > abs_rect.size.x) || (new_size.y > abs_rect.size.y)) {
                if (scr->is_screenplay_architecture() || (!scr->scr_config.blt_offscreen && !scr->scr_config.flicker_free)) {
                    common::region newly_expanded;
                    newly_expanded.add_rect(new_bounding_rect);
                    newly_expanded.eliminate(eka2l1::rect(eka2l1::vec2{ 0, 0 }, abs_rect.size));

                    for (std::size_t i = 0; i < newly_expanded.rects_.size(); i++) {
                        background_region.add_rect(newly_expanded.rects_[i]);
                    }
                }

                redraw_region.make_empty();
                invalidate(new_bounding_rect);
            } else {
                redraw_region.clip(new_bounding_rect);
            }

            if (scr->is_screenplay_architecture() || (!scr->scr_config.blt_offscreen && scr->scr_config.flicker_free)) {
                background_region.make_empty();
                background_region.add_rect(eka2l1::rect(eka2l1::vec2{ 0, 0 }, new_size));
            }
        }
    }

    void redraw_msg_canvas::invalidate(const eka2l1::rect &irect) {
        if (irect.empty()) {
            return;
        }

        if (win_type != window_type::redraw) {
            return;
        }

        eka2l1::rect to_queue = irect;

        redraw_region.add_rect(to_queue);

        if (is_visible()) {
            client->queue_redraw(this, to_queue);
            client->trigger_redraw();
        }
    }

    void redraw_msg_canvas::end_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        redraw_rect_curr.make_empty();
        redraw_segments_.promote_last_segment();

        if (content_changed()) {
            try_update(ctx.msg->own_thr);
        }

        flags &= ~flags_in_redraw;
        content_changed(false);

        for (std::size_t i = 0; i < redraw_region.rects_.size(); i++) {
            client->queue_redraw(this, redraw_region.rects_[i]);
        }

        client->trigger_redraw();

        ctx.complete(epoc::error_none);
    }

    void redraw_msg_canvas::begin_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        if (flags & flags_in_redraw) {
            ctx.complete(epoc::error_in_use);
            return;
        }

        if (cmd.header.cmd_len == 0) {
            redraw_rect_curr = bounding_rect();
        } else {
            redraw_rect_curr = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);
            redraw_rect_curr.transform_from_symbian_rectangle();
        }

        redraw_region.eliminate(redraw_rect_curr);

        client->remove_redraws(this);
        redraw_segments_.add_new_segment(redraw_rect_curr, epoc::gdi_store_command_segment_pending_redraw);

        flags |= flags_in_redraw;

        common::double_linked_queue_element *ite = attached_contexts.first();
        common::double_linked_queue_element *end = attached_contexts.end();

        do {
            if (!ite) {
                break;
            }

            epoc::graphic_context *ctx = E_LOFF(ite, epoc::graphic_context, context_attach_link);

            if (ctx->recording) {
                ctx->do_submit_clipping();
                ctx->flushed = true;
            }

            ite = ite->next;
        } while (ite != end);

        ctx.complete(epoc::error_none);
    }

    void redraw_msg_canvas::add_draw_command(gdi_store_command &command) {
        const std::lock_guard<std::mutex> guard(scr->screen_mutex);

        if ((flags & flags_in_redraw) == 0) {
            eka2l1::rect full_size_rect(eka2l1::vec2(0, 0), abs_rect.size);

            gdi_store_command_segment *current_segment = redraw_segments_.get_current_segment();
            if (!current_segment || (current_segment->type_ != gdi_store_command_segment_non_redraw)) {
                redraw_segments_.add_new_segment(full_size_rect, gdi_store_command_segment_non_redraw);
            }
        }

        content_changed(true);

        gdi_store_command_segment *current_segment = redraw_segments_.get_current_segment();
        current_segment->add_command(command);

        canvas_base::add_draw_command(command);
    }

    std::uint64_t redraw_msg_canvas::try_update(kernel::thread *drawer) {
        return canvas_base::try_update(drawer);
    }

    bool redraw_msg_canvas::draw(drivers::graphics_command_builder &builder) {
        if (!can_be_physically_seen()) {
            return false;
        }

        if (size().x == 0 || size().y == 0) {
            return false;
        }

        auto draw_background_color = [&]() {
            if ((scr->is_screenplay_architecture() || !scr->scr_config.blt_offscreen) && clear_color_enable && !background_region.empty()) {
                background_region.advance(abs_rect.top);
                background_region = background_region.intersect(visible_region);

                builder.clip_bitmap_region(background_region, scr->display_scale_factor);

                auto color_extracted = common::rgba_to_vec(clear_color);

                if (display_mode() <= epoc::display_mode::color16mu) {
                    color_extracted.w = 255;
                }

                builder.set_brush_color_detail(color_extracted);
                builder.draw_rectangle(eka2l1::rect(abs_rect.top * scr->display_scale_factor, { 0, 0 }));

                background_region.make_empty();
            } else {
                builder.set_brush_color(eka2l1::vec3(255, 255, 255));
                builder.set_feature(drivers::graphics_feature::clipping, false);
            }
        };

        builder.set_feature(drivers::graphics_feature::blend, false);

        eka2l1::drivers::filter_option filter = (client->get_ws().get_kernel_system()->get_config()->nearest_neighbor_filtering ? eka2l1::drivers::filter_option::nearest : eka2l1::drivers::filter_option::linear);

        if (scr->flags_ & screen::FLAG_SERVER_REDRAW_PENDING) {
            auto &segments = redraw_segments_.get_segments();

            if (!segments.empty()) {
                for (std::size_t i = 0; i < segments.size(); i++) {
                    if (segments[i]->type_ != gdi_store_command_segment_pending_redraw) {
                        draw_background_color();
                        break;
                    }
                }

                builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

                gdi_command_builder gdi_builder(client->get_ws().get_graphics_driver(), builder,
                    *client->get_ws().get_bitmap_cache(), filter, abs_rect.top, scr->display_scale_factor,
                    visible_region);

                for (std::size_t i = 0; i < segments.size(); i++) {
                    if (segments[i]->type_ != gdi_store_command_segment_pending_redraw) {
                        gdi_builder.build_segment(*segments[i]);
                    }
                }
            }
        }

        if (scr->flags_ & screen::FLAG_CLIENT_REDRAW_PENDING) {
            drivers::command_list cmd_list = driver_builder_.retrieve_command_list();
            if (pending_segment_) {
                builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

                gdi_command_builder gdi_builder(client->get_ws().get_graphics_driver(), builder,
                    *client->get_ws().get_bitmap_cache(), filter, abs_rect.top, scr->display_scale_factor,
                    visible_region);

                gdi_builder.build_segment(*pending_segment_);
                pending_segment_.reset();
            }

            if (!cmd_list.empty()) {
                builder.clip_bitmap_region(visible_region, scr->display_scale_factor);
                builder.draw_rectangle(abs_rect);

                if (!builder.merge(cmd_list)) {
                    drivers::command_list screen_draw_prev_list = builder.retrieve_command_list();
                    drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

                    drv->submit_command_list(screen_draw_prev_list);
                    builder.bind_bitmap(scr->screen_texture);

                    if (!builder.merge(cmd_list)) {
                        LOG_ERROR(SERVICE_WINDOW, "Unable to merge redraw window's command list to screen draw's command list!");
                    }
                }
            }
        }

        return true;
    }

    bool redraw_msg_canvas::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        bool did_it = false;
        const bool should_flush = canvas_base::execute_command_detail(ctx, cmd, did_it);

        if (did_it) {
            return should_flush;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpInvalidateFull:
        case EWsWinOpInvalidate:
            invalidate(ctx, cmd);
            break;

        case EWsWinOpBeginRedraw:
        case EWsWinOpBeginRedrawFull: {
            begin_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpEndRedraw: {
            end_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpStoreDrawCommands:
            store_draw_commands(ctx, cmd);
            break;

        default:
            LOG_WARN(SERVICE_WINDOW, "Unimplemented redraw canvas opcode 0x{:X}!", cmd.header.op);
            ctx.complete(epoc::error_none);

            break;
        }

        return false;
    }
}