#include <drivers/ui/input_dialog.h>
#include <qt/dialog_driver.h>
#include <string>

namespace eka2l1::drivers::ui {
    main_window *main_window_instance = nullptr;

    bool open_input_view(const std::u16string &initial_text, const int max_len, input_dialog_complete_callback complete_callback) {
        if (complete_callback)
            complete_callback(u"");
        return true;
    }

    void close_input_view() {}

    void show_yes_no_dialog(const std::u16string &text, const std::u16string &button1_text, const std::u16string &button2_text,
        yes_no_dialog_complete_callback complete_callback) {
        if (complete_callback)
            complete_callback(1); // Auto YES
    }
}