#pragma once

#include <common/vecx.h>
#include <services/window/classes/winbase.h>
#include <tuple>
#include <vector>

namespace eka2l1 {
    class window_server;
}

namespace eka2l1::epoc {
    struct window_pointer_focus_walker : public epoc::window_tree_walker {
        std::vector<std::pair<epoc::event, bool>> evts_;
        eka2l1::vec2 scr_coord_;

        void add_new_event(const epoc::event &evt);
        void process_event_to_target_window(epoc::window *win, epoc::event &evt);
        bool do_it(epoc::window *win) override;
        void clear();
    };

    struct window_key_shipper {
        eka2l1::window_server *serv_;
        std::vector<epoc::event> evts_;

        explicit window_key_shipper(window_server *serv);
        void add_new_event(const epoc::event &evt);
        void start_shipping();
    };
}