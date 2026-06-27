#include <common/rgb.h>
#include <common/time.h>
#include <config/app_settings.h>
#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>
#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <services/window/classes/dsa.h>
#include <services/window/classes/winbase.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <thread>

namespace eka2l1::epoc {
    struct window_drawer_walker : public window_tree_walker {
        drivers::graphics_command_builder &builder_;
        std::uint32_t total_redrawed_;

        explicit window_drawer_walker(drivers::graphics_command_builder &builder)
            : builder_(builder)
            , total_redrawed_(0) {
        }

        bool do_it(window *win) {
            if (win->type != window_kind::client) {
                return false;
            }
            epoc::canvas_base *cv = reinterpret_cast<epoc::canvas_base *>(win);
            if (cv->draw(builder_))
                total_redrawed_++;
            return false;
        }
    };

    struct window_dsa_abort_walker : public window_tree_walker {
        std::int32_t reason_;

        explicit window_dsa_abort_walker(const std::int32_t reason)
            : reason_(reason) {
        }

        bool do_it(window *win) {
            if (win->type == window_kind::client) {
                canvas_base *user = reinterpret_cast<canvas_base *>(win);
                if (user->is_dsa_active()) {
                    std::vector<dsa *> dsa_residents = user->directs_;
                    for (std::size_t i = 0; i < dsa_residents.size(); i++) {
                        dsa_residents[i]->abort(reason_);
                    }
                }
            }
            return false;
        }
    };

    bool focus_callback_free_check_func(screen::focus_change_callback &data) {
        return !data.second;
    }

    void focus_callback_free_func(screen::focus_change_callback &data) {
        data.second = nullptr;
    }

    bool screen_redraw_callback_free_check_func(screen::screen_redraw_callback &data) {
        return !data.second;
    }

    void screen_redraw_callback_free_func(screen::screen_redraw_callback &data) {
        data.second = nullptr;
    }

    bool screen_mode_change_callback_free_check_func(screen::screen_mode_change_callback &data) {
        return !data.second;
    }

    void screen_mode_change_callback_free_func(screen::screen_mode_change_callback &data) {
        data.second = nullptr;
    }

    screen::screen(const int number, epoc::config::screen &scr_conf)
        : number(number)
        , ui_rotation(0)
        , refresh_rate(60)
        , display_scale_factor(1.0f)
        , logic_scale_factor_x(1.0f)
        , logic_scale_factor_y(1.0f)
        , requested_ui_scale_factor(-1.0f)
        , screen_texture(0)
        , dsa_texture(0)
        , disp_mode(display_mode::color16ma)
        , last_vsync(0)
        , last_fps_check(0)
        , last_fps(0)
        , frame_passed_per_sec(0)
        , scr_config(scr_conf)
        , crr_mode(0)
        , focus(nullptr)
        , next(nullptr)
        , screen_buffer_chunk(nullptr)
        , flags_(FLAG_ORIENTATION_LOCK)
        , focus_callbacks(focus_callback_free_check_func, focus_callback_free_func)
        , screen_redraw_callbacks(screen_redraw_callback_free_check_func, screen_redraw_callback_free_func)
        , screen_mode_change_callbacks(screen_mode_change_callback_free_check_func, screen_mode_change_callback_free_func) {
        root = std::make_unique<epoc::window>(nullptr, this, nullptr);
        disp_mode = scr_conf.disp_mode;
        for (std::size_t i = 0; i < scr_config.modes.size(); i++) {
            if (scr_config.modes[i].rotation == 0) {
                physical_mode = static_cast<std::uint8_t>(i);
            }
        }
        if (scr_conf.auto_clear) {
            flags_ = FLAG_AUTO_CLEAR_BACKGROUND;
        }
    }

    std::uint8_t *screen::screen_buffer_ptr() {
        return screen_buffer_chunk ? reinterpret_cast<std::uint8_t *>(screen_buffer_chunk->host_base()) : nullptr;
    }

    eka2l1::vec2 screen::size() const {
        return current_mode().size;
    }

    void screen::ref_dsa_usage() {
        active_dsa_count_++;
    }

    const int screen::total_screen_mode() const {
        return static_cast<int>(scr_config.modes.size());
    }

    void screen::vsync(ntimer *timing, std::uint64_t &next_vsync_us) {
        std::uint64_t crr = timing->microseconds();
        std::uint64_t frame_time = 1000000 / refresh_rate;
        if (crr >= last_vsync + frame_time) {
            last_vsync = crr;
        } else {
            last_vsync += frame_time;
        }
        next_vsync_us = last_vsync;
    }

    bool screen::redraw(drivers::graphics_command_builder &builder, const bool need_bind) {
        if (need_update_visible_regions()) {
            recalculate_visible_regions();
        }
        if (need_bind) {
            builder.bind_bitmap(screen_texture);
        }
        builder.set_feature(eka2l1::drivers::graphics_feature::cull, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::blend, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::clipping, false);

        builder.clear(eka2l1::vecx<float, 6>({ 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 }), drivers::draw_buffer_bit_depth_buffer | drivers::draw_buffer_bit_stencil_buffer | ((flags_ & FLAG_SERVER_REDRAW_PENDING) ? drivers::draw_buffer_bit_color_buffer : 0));

        builder.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
            drivers::blend_factor::frag_out_alpha, drivers::blend_factor::one_minus_frag_out_alpha,
            drivers::blend_factor::one, drivers::blend_factor::one);

        window_drawer_walker adrawwalker(builder);
        root->walk_tree(&adrawwalker, window_tree_walk_style::bonjour_children);

        builder.bind_bitmap(0);
        flags_ &= ~(FLAG_SERVER_REDRAW_PENDING | FLAG_CLIENT_REDRAW_PENDING);
        return adrawwalker.total_redrawed_;
    }

    void screen::redraw(drivers::graphics_driver *driver) {
        if (!screen_texture) {
            set_screen_mode(nullptr, driver, crr_mode);
        }
        drivers::graphics_command_builder builder;
        const bool performed = redraw(builder, true);
        eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
        driver->submit_command_list(retrieved);
        fire_screen_redraw_callbacks(false);
    }

    void screen::deinit(drivers::graphics_driver *driver) {
        if (driver) {
            drivers::graphics_command_builder builder;
            if (dsa_texture) {
                builder.destroy_bitmap(dsa_texture);
            }
            if (screen_texture) {
                builder.destroy_bitmap(screen_texture);
            }
            eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
            driver->submit_command_list(retrieved);
        }
    }

    void screen::resize(drivers::graphics_driver *driver, const eka2l1::vec2 &new_size) {
        drivers::graphics_command_builder builder;
        bool need_bind = true;
        eka2l1::vec2 screen_size_scaled = current_mode().size * display_scale_factor;

        if (!screen_texture) {
            screen_texture = drivers::create_bitmap(driver, screen_size_scaled, 32);
        } else {
            builder.bind_bitmap(screen_texture);
            builder.resize_bitmap(screen_texture, screen_size_scaled);
            need_bind = false;
        }

        const bool performed = redraw(builder, need_bind);
        eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
        driver->submit_command_list(retrieved);
    }

    static epoc::window_group *find_group_to_focus(epoc::window *root) {
        epoc::window_group *next_to_focus = reinterpret_cast<epoc::window_group *>(root->child);
        while (next_to_focus != nullptr) {
            assert(next_to_focus->type == window_kind::group && "The window kind of lvl1 root's child must be window group");
            if (next_to_focus->can_receive_focus())
                break;
            next_to_focus = reinterpret_cast<epoc::window_group *>(next_to_focus->sibling);
        }
        return next_to_focus;
    }

    void screen::restore_from_config(drivers::graphics_driver *driver, const eka2l1::config::app_setting &setting) {
        refresh_rate = static_cast<std::uint8_t>(setting.fps);
        flags_ &= ~FLAG_SCREEN_UPSCALE_FACTOR_LOCK;
        if (setting.screen_upscale_method != 0) {
            flags_ |= FLAG_SCREEN_UPSCALE_FACTOR_LOCK;
            display_scale_factor = 1.0f;
        }
        requested_ui_scale_factor = setting.screen_upscale;
        if (driver) {
            driver->set_upscale_shader(setting.filter_shader_path);
        }
    }

    void screen::store_to_config(const eka2l1::config::app_setting &setting) {}

    epoc::window_group *screen::update_focus(window_server *serv, epoc::window_group *closing_group) {
        epoc::window_group *old_focus = focus;
        focus = find_group_to_focus(root.get());
        const bool is_me_currently_focus = (serv->get_current_focus_screen() == this);

        epoc::window_group *alternative_focus = nullptr;
        screen *new_focus_screen = nullptr;

        if (is_me_currently_focus && !focus) {
            new_focus_screen = serv->get_screens();
            while (new_focus_screen != nullptr) {
                if (new_focus_screen != this) {
                    alternative_focus = find_group_to_focus(new_focus_screen->root.get());
                }
                if (alternative_focus) {
                    break;
                }
                new_focus_screen = new_focus_screen->next;
            }
            if (!alternative_focus) {
                new_focus_screen = nullptr;
            }
        }

        if (old_focus != focus || new_focus_screen) {
            if (old_focus && (old_focus != closing_group) && is_me_currently_focus) {
                old_focus->lost_focus();
                flags_ &= ~FLAG_SCREEN_UPSCALE_FACTOR_LOCK;
            }

            if (new_focus_screen) {
                serv->set_focus_screen(new_focus_screen);
                alternative_focus->gain_focus();
                serv->send_focus_group_change_events(new_focus_screen);
                new_focus_screen->fire_focus_change_callbacks(focus_change_target);
                new_focus_screen->restore_from_config(serv->get_graphics_driver(), alternative_focus->saved_setting);
            } else if (focus && is_me_currently_focus) {
                focus->gain_focus();
                serv->send_focus_group_change_events(this);
                fire_focus_change_callbacks(focus_change_target);
                restore_from_config(serv->get_graphics_driver(), focus->saved_setting);
            }
        }
        return (new_focus_screen ? alternative_focus : focus);
    }

    epoc::window_group *screen::get_group_chain() {
        return reinterpret_cast<epoc::window_group *>(root->child);
    }

    void screen::fire_focus_change_callbacks(const focus_change_property property) {
        const std::lock_guard<std::mutex> guard(screen_mutex);
        for (auto &callback : focus_callbacks) {
            if (callback.second)
                callback.second(callback.first, focus, property);
        }
    }

    void screen::fire_screen_redraw_callbacks(const bool is_dsa) {
        for (auto &callback : screen_redraw_callbacks) {
            if (callback.second)
                callback.second(callback.first, this, is_dsa);
        }
    }

    void screen::fire_screen_mode_change_callbacks(const int old_mode) {
        for (auto &callback : screen_mode_change_callbacks) {
            if (callback.second)
                callback.second(callback.first, this, old_mode);
        }
    }

    std::size_t screen::add_focus_change_callback(void *userdata, focus_change_callback_handler handler) {
        const std::lock_guard<std::mutex> guard(screen_mutex);
        focus_change_callback callback_pair = { userdata, handler };
        return focus_callbacks.add(callback_pair);
    }

    std::size_t screen::add_screen_redraw_callback(void *userdata, screen_redraw_callback_handler handler) {
        const std::lock_guard<std::mutex> guard(screen_mutex);
        screen_redraw_callback callback_pair = { userdata, handler };
        return screen_redraw_callbacks.add(callback_pair);
    }

    std::size_t screen::add_screen_mode_change_callback(void *userdata, screen_mode_change_callback_handler handler) {
        const std::lock_guard<std::mutex> guard(screen_mutex);
        screen_mode_change_callback callback_pair = { userdata, handler };
        return screen_mode_change_callbacks.add(callback_pair);
    }

    void screen::abort_all_dsas(const std::int32_t reason) {
        window_dsa_abort_walker walker(reason);
        root->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);
    }

    void screen::set_screen_mode(window_server *winserv, drivers::graphics_driver *drv, const int mode) {
        const int old_mode = crr_mode;
        bool really_changed = false;

        if (crr_mode != mode) {
            LOG_TRACE(SERVICE_WINDOW, "Screen mode changed to {}", mode);
            really_changed = true;
        }

        crr_mode = mode;
        resize(drv, mode_info(mode)->size);

        if (really_changed) {
            fire_screen_mode_change_callbacks(old_mode);
            if (winserv)
                winserv->send_screen_change_events(this);

            const epoc::config::screen_mode *modeinfo_o = mode_info(old_mode);
            const epoc::config::screen_mode *modeinfo_n = mode_info(crr_mode);

            if (modeinfo_o && modeinfo_n) {
                if (modeinfo_o->size != modeinfo_n->size) {
                    abort_all_dsas(dsa_terminate_rotation_change);
                    need_update_visible_regions(true);
                    recalculate_visible_regions();
                }
            }
        }
    }

    const epoc::config::screen_mode *screen::mode_info(const int number) const {
        if (number < scr_config.modes.size()) {
            return &scr_config.modes[number];
        }
        return nullptr;
    }

    const epoc::config::screen_mode &screen::current_mode() const {
        return *mode_info(crr_mode);
    }

    struct window_visible_region_calc_walker : public window_tree_walker {
        common::region visible_left_region_;
        bool trigger_redraw_;

        explicit window_visible_region_calc_walker(const common::region &master_region, bool trigger_redraw = true)
            : visible_left_region_(master_region)
            , trigger_redraw_(trigger_redraw) {
        }

        bool do_it(epoc::window *win) override {
            if (win->type == epoc::window_kind::client) {
                epoc::canvas_base *winuser = reinterpret_cast<epoc::canvas_base *>(win);
                common::region previous_region = winuser->visible_region;
                winuser->visible_region.make_empty();

                if ((winuser->win_type != epoc::window_type::blank) || (!winuser->scr->scr_config.blt_offscreen)) {
                    if (!visible_left_region_.empty() && winuser->is_visible()) {
                        if (winuser->flags & epoc::window::flag_shape_region) {
                            winuser->visible_region = winuser->shape_region;
                        } else {
                            winuser->visible_region.add_rect(winuser->abs_rect);
                        }

                        winuser->visible_region = winuser->visible_region.intersect(visible_left_region_);
                        visible_left_region_.eliminate(winuser->visible_region);
                    }
                }

                if (!previous_region.identical(winuser->visible_region)) {
                    if (winuser->is_dsa_active()) {
                        std::vector<dsa *> dsa_residents = winuser->directs_;
                        for (std::size_t i = 0; i < dsa_residents.size(); i++) {
                            dsa_residents[i]->visible_region_changed(winuser->visible_region);
                        }
                    }
                    winuser->report_visiblity_change();
                    if (trigger_redraw_) {
                        winuser->flags |= screen::FLAG_SERVER_REDRAW_PENDING;
                    }
                }
            }
            return false;
        }
    };

    void screen::recalculate_visible_regions(bool dont_trigger_redraw) {
        common::region master_region;
        eka2l1::rect master_rect{ eka2l1::vec2(0, 0), current_mode().size };

        master_region.add_rect(master_rect);
        window_visible_region_calc_walker walker(master_region, !dont_trigger_redraw);

        root->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children);
        need_update_visible_regions(false);
    }

    void screen::deref_dsa_usage() {
        active_dsa_count_--;
    }

    void screen::need_update_visible_regions(const bool value) {
        if (value) {
            flags_ |= FLAG_NEED_RECALC_VISIBLE;
            if (active_dsa_count_) {
                recalculate_visible_regions(true);
            }
        } else {
            flags_ &= ~FLAG_NEED_RECALC_VISIBLE;
        }
    }

    void screen::try_change_display_rescale(drivers::graphics_driver *driver, const float new_scale_factor) {
        if (new_scale_factor != display_scale_factor) {
            if (screen_texture) {
                eka2l1::vec2 screen_size_scaled = current_mode().size * new_scale_factor;
                drivers::graphics_command_builder cmd_builder;

                cmd_builder.resize_bitmap(screen_texture, screen_size_scaled);
                drivers::command_list retrieved = cmd_builder.retrieve_command_list();
                driver->submit_command_list(retrieved);

                flags_ |= FLAG_SERVER_REDRAW_PENDING;
            }
            display_scale_factor = new_scale_factor;
        }
    }

    void screen::set_native_scale_factor(drivers::graphics_driver *driver, const float new_scale_factor_x,
        const float new_scale_factor_y) {
        if (logic_scale_factor_x != new_scale_factor_x) {
            logic_scale_factor_x = new_scale_factor_x;
        }
        if (logic_scale_factor_y != new_scale_factor_y) {
            logic_scale_factor_y = new_scale_factor_y;
        }

        float correct_display_scale_factor = 1.0f;
        if ((flags_ & FLAG_SCREEN_UPSCALE_FACTOR_LOCK) == 0) {
            correct_display_scale_factor = common::min(logic_scale_factor_x, logic_scale_factor_y);
            if (correct_display_scale_factor < 1.0f) {
                correct_display_scale_factor = 1.0f;
            }
        }
        try_change_display_rescale(driver, correct_display_scale_factor);
    }
}