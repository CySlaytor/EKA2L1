#pragma once

#include <QMainWindow>
#include <QResizeEvent>
#include <QTimer>
#include <drivers/ui/input_dialog.h>
#include <functional>
#include <qt/btnmap/executor.h>
#include <qt/displaywidget.h>
#include <qt/state.h>
#include <string>

namespace eka2l1 {
    namespace kernel {
        class process;
    }
}

class main_window : public QMainWindow {
    Q_OBJECT

public:
    main_window(QApplication &app, QWidget *parent, eka2l1::desktop::emulator &emulator_state);
    ~main_window();

    void load_and_show();
    void setup_and_switch_to_game_mode();
    display_widget *render_window() { return displayer_; }

    bool controller_event_handler(eka2l1::drivers::input_event &event);
    bool deliver_key_event(const std::uint32_t code, const bool is_press);
    bool deliver_overlay_mouse_event(const eka2l1::vec3 &pos, const int button_id, const int action_id, const int mouse_id) { return false; }
    int map_mouse_id_to_touch_index(int mouse_id, const bool on_release) { return mouse_id; }

    std::function<void(eka2l1::kernel::process *)> get_process_exit_callback();

    void resizeEvent(QResizeEvent *event) override;

private slots:
    void on_app_exited(eka2l1::kernel::process *proc);
    void attempt_auto_boot();

signals:
    void app_exited(eka2l1::kernel::process *proc);

private:
    void setup_screen_draw();
    void auto_boot_game();

    QApplication &application_;
    eka2l1::desktop::emulator &emulator_state_;
    display_widget *displayer_;
    eka2l1::qt::btnmap::executor *map_executor_;
    QTimer *boot_timer_;

    std::size_t active_screen_draw_callback_;
    std::size_t active_screen_mode_change_callback_;
};