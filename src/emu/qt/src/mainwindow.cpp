#include <QApplication>
#include <QDebug>
#include <common/cvt.h>
#include <kernel/kernel.h>
#include <qt/mainwindow.h>
#include <qt/utils.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <utils/apacmd.h>

static void mode_change_screen(void *userdata, eka2l1::epoc::screen *scr, const int old_mode) {
    eka2l1::desktop::emulator *state_ptr = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    if (!state_ptr)
        return;

    QSize new_minsize(scr->current_mode().size.x, scr->current_mode().size.y);
    if ((scr->ui_rotation % 180) != 0) {
        new_minsize = QSize(scr->current_mode().size.y, scr->current_mode().size.x);
    }

    display_widget *widget = static_cast<display_widget *>(state_ptr->window);
    new_minsize /= widget->devicePixelRatioF();
    widget->setMinimumSize(new_minsize);
    widget->resize(new_minsize);
}

static void draw_emulator_screen(void *userdata, eka2l1::epoc::screen *scr, const bool is_dsa) {
    eka2l1::desktop::emulator *state_ptr = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    if (!state_ptr || !state_ptr->graphics_driver)
        return;

    state_ptr->graphics_driver->wait_for(&state_ptr->present_status);

    eka2l1::drivers::graphics_command_builder builder;
    const auto window_width = state_ptr->window->window_fb_size().x;
    const auto window_height = state_ptr->window->window_fb_size().y;
    eka2l1::vec2 swapchain_size(window_width, window_height);

    builder.set_swapchain_size(swapchain_size);
    builder.backup_state();

    builder.set_feature(eka2l1::drivers::graphics_feature::cull, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::blend, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::clipping, false);
    builder.set_viewport(eka2l1::rect(eka2l1::vec2(0, 0), swapchain_size));

    builder.clear({ 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f }, eka2l1::drivers::draw_buffer_bit_color_buffer);

    auto &crr_mode = scr->current_mode();
    eka2l1::vec2 size = crr_mode.size;
    if ((scr->ui_rotation % 180) != 0)
        std::swap(size.x, size.y);

    // Force stretch to fill the display!
    // We deleted the aspect ratio correction math here.
    float mult_x = static_cast<float>(window_width) / size.x;
    float mult_y = static_cast<float>(window_height) / size.y;
    float width = static_cast<float>(window_width);
    float height = static_cast<float>(window_height);
    std::uint32_t x = 0, y = 0;

    scr->set_native_scale_factor(state_ptr->graphics_driver.get(), mult_x, mult_y);
    scr->absolute_pos.x = static_cast<int>(x);
    scr->absolute_pos.y = static_cast<int>(y);

    eka2l1::rect dest(eka2l1::vec2(x, y), eka2l1::vec2(width, height));
    eka2l1::rect src(eka2l1::vec2(0, 0), crr_mode.size * scr->display_scale_factor);

    eka2l1::drivers::advance_draw_pos_around_origin(dest, scr->ui_rotation);
    if (scr->ui_rotation % 180 != 0)
        std::swap(dest.size.x, dest.size.y);

    builder.set_texture_filter(scr->screen_texture, true, eka2l1::drivers::filter_option::linear);
    builder.set_texture_filter(scr->screen_texture, false, eka2l1::drivers::filter_option::linear);

    builder.draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0), static_cast<float>(scr->ui_rotation), 0);

    builder.load_backup_state();
    state_ptr->present_status = -100;
    builder.present(&state_ptr->present_status);

    eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
    state_ptr->graphics_driver->submit_command_list(retrieved);
}

main_window::main_window(QApplication &app, QWidget *parent, eka2l1::desktop::emulator &state)
    : QMainWindow(parent)
    , application_(app)
    , emulator_state_(state)
    , active_screen_draw_callback_(0)
    , active_screen_mode_change_callback_(0) {
    // No frameless window flags here anymore, so standard OS borders show up.
    resize(240, 320);

    displayer_ = new display_widget(this);
    displayer_->setStyleSheet("background-color: black;");
    setCentralWidget(displayer_);

    map_executor_ = new eka2l1::qt::btnmap::executor(nullptr, emulator_state_.symsys->get_ntimer());
    std::string profile_path = "bindings\\" + emulator_state_.conf.current_keybind_profile + ".yml";
    map_executor_->deserialize(profile_path);

    boot_timer_ = new QTimer(this);
    connect(boot_timer_, &QTimer::timeout, this, &main_window::attempt_auto_boot);
    connect(this, &main_window::app_exited, this, &main_window::on_app_exited, Qt::QueuedConnection);
}

main_window::~main_window() {
    delete map_executor_;
}

void main_window::load_and_show() {
    show();
}

void main_window::setup_and_switch_to_game_mode() {
    displayer_->setVisible(true);
    displayer_->setFocus();

    qDebug() << "UI initialized. Waiting for Symbian OS Window Server...";
    boot_timer_->start(500);
}

void main_window::attempt_auto_boot() {
    if (active_screen_draw_callback_ == 0) {
        eka2l1::epoc::screen *scr = get_current_active_screen(emulator_state_.symsys.get(), 0);
        if (scr) {
            qDebug() << "Symbian Window Server found! Hooking graphics...";
            active_screen_draw_callback_ = scr->add_screen_redraw_callback(&emulator_state_, [](void *userdata, eka2l1::epoc::screen *scr, const bool is_dsa) {
                draw_emulator_screen(userdata, scr, is_dsa);
            });
            active_screen_mode_change_callback_ = scr->add_screen_mode_change_callback(&emulator_state_, mode_change_screen);
            mode_change_screen(&emulator_state_, scr, 0);
        }
    }

    if (active_screen_draw_callback_ != 0) {
        eka2l1::kernel_system *kern = emulator_state_.symsys->get_kernel_system();
        if (!kern)
            return;

        boot_timer_->stop();

        qDebug() << "Spawning kernel process for:" << emulator_state_.launched_app_name_.c_str();

        std::u16string exe_path = eka2l1::common::utf8_to_ucs2(emulator_state_.launched_app_name_);

        kern->lock();
        eka2l1::kernel::process *process_ptr = kern->spawn_new_process(exe_path, u"");
        if (process_ptr) {
            process_ptr->logon(get_process_exit_callback());
            process_ptr->run();
            qDebug() << "Process successfully injected into the kernel!";
        } else {
            qDebug() << "Failed to spawn process. Ensure the path is correct.";
        }
        kern->unlock();
    }
}

void main_window::setup_screen_draw() {
    // Logic migrated to attempt_auto_boot
}

void main_window::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);

    if (!displayer_ || !displayer_->isVisible())
        return;

    // Force a redraw when the window is stretched/shrunk so the graphic driver isn't lagging behind
    if (emulator_state_.symsys && active_screen_draw_callback_ != 0) {
        eka2l1::epoc::screen *scr = get_current_active_screen(emulator_state_.symsys.get(), 0);
        if (scr) {
            scr->flags_ |= eka2l1::epoc::screen::FLAG_SERVER_REDRAW_PENDING;
            draw_emulator_screen(&emulator_state_, scr, true);
        }
    }
}

bool main_window::deliver_key_event(const std::uint32_t code, const bool is_press) {
    return map_executor_->execute(code, is_press);
}

bool main_window::controller_event_handler(eka2l1::drivers::input_event &event) {
    const std::uint64_t map_type_gamepad = 1;
    if (event.button_.state_ == eka2l1::drivers::button_state::joy) {
        return !map_executor_->execute((map_type_gamepad << 32) | event.button_.button_, event.button_.axis_[0], event.button_.axis_[1]);
    }
    return !map_executor_->execute((map_type_gamepad << 32) | event.button_.button_, event.button_.state_ == eka2l1::drivers::button_state::pressed);
}

std::function<void(eka2l1::kernel::process *)> main_window::get_process_exit_callback() {
    return [this](eka2l1::kernel::process *proc) { emit app_exited(proc); };
}

void main_window::on_app_exited(eka2l1::kernel::process *target_proc) {
    QApplication::quit();
}