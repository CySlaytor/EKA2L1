#include <cassert>
#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <services/window/scheduler.h>
#include <services/window/screen.h>

namespace eka2l1::epoc {
    static void on_anim_due(std::uint64_t userdata, const int cycles_late) {
        anim_due_callback_data *callback = reinterpret_cast<anim_due_callback_data *>(userdata);
        callback->sched->invoke_due_animation(callback->driver, callback->screen_number);
    }

    static void on_scan_callback(std::uint64_t userdata, const int cycles_late) {
        sched_scan_callback_data *callback = reinterpret_cast<sched_scan_callback_data *>(userdata);
        callback->sched->idle_callback(callback->driver);
    }

    animation_scheduler::animation_scheduler(kernel_system *kern, ntimer *timing, const int total_screen)
        : kern_(kern)
        , timing_(timing)
        , callback_scheduled_(false) {
        anim_due_evt_ = timing_->register_event("anim_sched_anim_due_evt", on_anim_due);
        callback_evt_ = timing->register_event("anim_sched_callback_evt", on_scan_callback);

        schedules_.resize(total_screen);
        states_.resize(total_screen);
        callback_datas_.resize(total_screen);

        for (int i = 0; i < total_screen; i++) {
            schedules_[i].scheduled = false;
            states_[i].flags = screen_state::inactive;
            callback_datas_[i].sched = this;
            callback_datas_[i].screen_number = i;
        }
    }

    animation_scheduler::~animation_scheduler() {
        cancel_all();
    }

    bool animation_scheduler::cancel_all() {
        const std::lock_guard<std::mutex> guard(lock_);
        if (callback_evt_ == 0) {
            return false;
        }

        if (callback_scheduled_) {
            timing_->unschedule_event(callback_evt_, reinterpret_cast<std::uint64_t>(&scan_callback_data_));
        }

        for (std::size_t i = 0; i < states_.size(); i++) {
            if (states_[i].flags == screen_state::scheduled) {
                timing_->unschedule_event(anim_due_evt_, reinterpret_cast<std::uint64_t>(&callback_datas_[i]));
            }
        }

        timing_->remove_event(callback_evt_);
        timing_->remove_event(anim_due_evt_);
        callback_evt_ = 0;
        anim_due_evt_ = 0;

        return true;
    }

    void animation_scheduler::schedule(drivers::graphics_driver *driver, screen *scr, const std::uint64_t time) {
        const std::lock_guard<std::mutex> guard(lock_);
        if (schedules_.size() <= scr->number) {
            return;
        }

        anim_schedule sched;
        sched.scr = scr;
        sched.time = time;
        sched.scheduled = true;

        if (schedules_[scr->number].scheduled) {
            schedules_[scr->number].time = time;
        } else {
            schedules_[scr->number] = sched;
        }

        schedule_scans(driver);
    }

    animation_scheduler::anim_schedule *animation_scheduler::get_scheduled_screen_update(const int scr_num) {
        if (schedules_.size() <= scr_num) {
            return nullptr;
        }
        if (!schedules_[scr_num].scheduled) {
            return nullptr;
        }
        return &(schedules_[scr_num]);
    }

    std::int64_t animation_scheduler::get_due_delta(const bool force_redraw, const std::uint64_t due) {
        if (force_redraw) {
            return 0;
        }
        return std::max<std::int64_t>(due - timing_->microseconds(), 0);
    }

    void animation_scheduler::scan_for_redraw(drivers::graphics_driver *driver, const int screen_number, const bool force_redraw) {
        anim_schedule *sched = get_scheduled_screen_update(screen_number);

        if (sched) {
            screen_state &scr_state = states_[screen_number];
            if (scr_state.flags != screen_state::active) {
                const std::int64_t until_due = get_due_delta(force_redraw, sched->time);
                bool should_update = true;
                const std::uint64_t now = timing_->microseconds();

                if (scr_state.flags == screen_state::scheduled) {
                    if (until_due < static_cast<std::int64_t>(scr_state.time_expected_redraw) - static_cast<std::int64_t>(now)) {
                        timing_->unschedule_event(anim_due_evt_, reinterpret_cast<std::uint64_t>(&callback_datas_[screen_number]));
                    } else {
                        should_update = false;
                    }
                }

                if (should_update) {
                    callback_datas_[screen_number].driver = driver;
                    if (until_due == 0) {
                        scr_state.time_expected_redraw = now;
                        lock_.unlock();
                        invoke_due_animation(driver, screen_number);
                        lock_.lock();
                    } else {
                        scr_state.time_expected_redraw = now + until_due;
                        scr_state.flags = screen_state::scheduled;
                        timing_->schedule_event(static_cast<int64_t>(until_due), anim_due_evt_, reinterpret_cast<std::uint64_t>(&callback_datas_[screen_number]));
                    }
                }
            }
        }
    }

    void animation_scheduler::invoke_due_animation(drivers::graphics_driver *driver, const int screen_number) {
        lock_.lock();
        anim_schedule *sched = get_scheduled_screen_update(screen_number);
        epoc::screen *scr = sched->scr;
        assert(sched && "The returned schedule should not be nullptr");
        sched->scheduled = false;
        lock_.unlock();

        {
            kern_->lock();
            {
                const std::lock_guard<std::mutex> guard(scr->screen_mutex);
                scr->redraw(driver);
            }
            kern_->unlock();
        }

        {
            const std::lock_guard<std::mutex> guard(lock_);
            states_[screen_number].flags = screen_state::inactive;
        }
    }

    void animation_scheduler::idle_callback(drivers::graphics_driver *driver) {
        lock_.lock();
        callback_scheduled_ = false;
        for (int screen_num = 0; screen_num < states_.size(); screen_num++) {
            scan_for_redraw(driver, screen_num, false);
        }
        lock_.unlock();
    }

    void animation_scheduler::schedule_scans(drivers::graphics_driver *driver) {
        static constexpr std::uint16_t scheduled_us = 500;
        if (!callback_scheduled_) {
            scan_callback_data_.driver = driver;
            scan_callback_data_.sched = this;
            timing_->schedule_event(scheduled_us, callback_evt_, reinterpret_cast<std::uint64_t>(&scan_callback_data_));
            callback_scheduled_ = true;
        }
    }
}