/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <drivers/ui/input_dialog.h>

namespace eka2l1::drivers::ui {
    // Silent stubs for bypass-driven user interface actions

    bool open_input_view(const std::u16string &initial_text, const int max_len, input_dialog_complete_callback complete_callback) {
        if (complete_callback) {
            // Silently and instantly complete text queries with the default text
            complete_callback(initial_text);
        }
        return true;
    }

    void close_input_view() {
        // No action required on clean stub
    }

    void show_yes_no_dialog(const std::u16string &text, const std::u16string &button1_text, const std::u16string &button2_text,
        yes_no_dialog_complete_callback complete_callback) {
        if (complete_callback) {
            // Unconditionally confirm alerts (Yes / OK) to prevent execution stalls
            complete_callback(1);
        }
    }
}