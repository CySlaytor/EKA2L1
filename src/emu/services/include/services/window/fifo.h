#pragma once

#include <services/window/common.h>
#include <utils/err.h>
#include <utils/reqsts.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace eka2l1::epoc {
    template <typename T, unsigned int MAX_ELEM = 32>
    class base_fifo {
    public:
        enum {
            maximum_element = MAX_ELEM
        };

        enum class queue_priority {
            high,
            low
        };

        struct fifo_element {
            std::uint32_t id;
            std::uint16_t pri;
            T evt;

            fifo_element(const std::uint32_t id, const T &evt)
                : id(id)
                , evt(std::move(evt)) {
            }
        };

    protected:
        std::vector<fifo_element> q_;
        std::mutex lock_;

        epoc::notify_info nof;

    public:
        void trigger_notification() {
            if (q_.size() > 0)
                nof.complete(0);
        }

    protected:
        std::uint32_t queue_event_dont_care(const T &evt) {
            fifo_element element(static_cast<std::uint32_t>(q_.size()) + 1, evt);
            q_.push_back(std::move(element));

            return static_cast<std::uint32_t>(q_.size());
        }

    public:
        base_fifo() {}

        void set_listener(epoc::notify_info nof_info) {
            const std::lock_guard<std::mutex> guard(lock_);

            if (q_.size() > 0) {
                nof_info.complete(0);
                return;
            }

            nof = nof_info;
        }

        void cancel_listener() {
            nof.complete(epoc::error_cancel);
        }

        void cancel_event_queue(std::uint32_t id) {
            const std::lock_guard<std::mutex> guard(lock_);

            auto elem_ite = std::find_if(q_.begin(), q_.end(),
                [=](const fifo_element &elem) { return elem.id == id; });

            if (elem_ite != q_.end()) {
                q_.erase(elem_ite);
            }
        }

        typedef bool (*walker_func)(void *userdata, T &evt);

        void walk(walker_func walker, void *userdata) {
            for (std::uint32_t i = 0; i < q_.size();) {
                if (!walker(userdata, q_[i].evt)) {
                    q_.erase(q_.begin() + i);
                } else {
                    i++;
                }
            }
        }

        std::optional<T> get_evt_opt() {
            const std::lock_guard<std::mutex> guard(lock_);
            if (q_.size() == 0) {
                return std::nullopt;
            }

            fifo_element elem = std::move(q_.front());
            q_.erase(q_.begin());
            return elem.evt;
        }
    };

    class event_fifo : public base_fifo<event, 32> {
    protected:
        bool is_my_priority_really_high(epoc::event_code evt);
        void do_purge();

    public:
        event_fifo()
            : base_fifo<event>() {}

        event get_event();
        std::uint32_t queue_event(const event &evt);
    };

    struct redraw_event_full {
        redraw_event evt_;
        void *owner_;
    };

    class redraw_fifo : public base_fifo<redraw_event_full, 32> {
    public:
        redraw_fifo()
            : base_fifo<redraw_event_full>() {}

        std::uint32_t queue_event(void *owner, const redraw_event &evt, const std::uint16_t pri);
        void remove_events(void *owner);
    };
}