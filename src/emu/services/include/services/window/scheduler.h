#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace eka2l1 {
    class ntimer;
    class kernel_system;

    namespace drivers {
        class graphics_driver;
    }
};

namespace eka2l1::epoc {
    class animation_scheduler;
    struct screen;

    struct anim_due_callback_data {
        int screen_number;
        animation_scheduler *sched;
        drivers::graphics_driver *driver;
    };

    struct sched_scan_callback_data {
        animation_scheduler *sched;
        drivers::graphics_driver *driver;
    };

    class animation_scheduler {
        struct anim_schedule {
            screen *scr;
            bool scheduled = false;
            std::uint64_t time;
        };

        struct screen_state {
            enum {
                inactive = 0,
                active = 1 << 0,
                scheduled = 1 << 1
            };

            std::uint8_t flags;
            std::uint64_t time_expected_redraw;
        };

        std::vector<anim_schedule> schedules_;
        std::vector<screen_state> states_;
        std::vector<anim_due_callback_data> callback_datas_;

        ntimer *timing_;
        kernel_system *kern_;

        sched_scan_callback_data scan_callback_data_;

        int anim_due_evt_;
        int callback_evt_;

        bool callback_scheduled_;
        std::mutex lock_;

        void schedule_scans(drivers::graphics_driver *driver);

    public:
        explicit animation_scheduler(kernel_system *kern, ntimer *timing, const int total_screen);
        ~animation_scheduler();

        bool cancel_all();

        void idle_callback(drivers::graphics_driver *driver);
        void invoke_due_animation(drivers::graphics_driver *driver, const int screen_number);

        anim_schedule *get_scheduled_screen_update(const int scr_num);
        std::int64_t get_due_delta(const bool force_redraw, const std::uint64_t due);
        void scan_for_redraw(drivers::graphics_driver *driver, const int screen_number, const bool force_redraw);
        void schedule(drivers::graphics_driver *driver, screen *scr, const std::uint64_t time);
    };
}