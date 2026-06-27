#include <cassert>
#include <common/log.h>
#include <services/window/fifo.h>

namespace eka2l1::epoc {
    std::uint32_t event_fifo::queue_event(const event &evt) {
        const std::lock_guard<std::mutex> guard(lock_);
        if (q_.size() == maximum_element) {
            do_purge();
        }
        if ((evt.type == epoc::event_code::touch) && ((evt.adv_pointer_evt_.evtype == epoc::event_type::drag) || (evt.adv_pointer_evt_.evtype == epoc::event_type::move))) {
            if (!q_.empty()) {
                epoc::event &evt_last = q_.back().evt;
                if ((evt_last.handle == evt.handle) && (evt_last.type == evt.type) && (evt_last.adv_pointer_evt_.evtype == evt.adv_pointer_evt_.evtype)
                    && (evt_last.adv_pointer_evt_.ptr_num == evt.adv_pointer_evt_.ptr_num)) {
                    evt_last = evt;
                    return static_cast<std::uint32_t>(q_.size());
                }
            }
        }
        std::uint32_t result = queue_event_dont_care(evt);
        trigger_notification();
        return result;
    }

    void event_fifo::do_purge() {
        for (size_t i = 0; i < q_.size(); i++) {
            switch (q_[i].evt.type) {
            case epoc::event_code::event_password:
                break;
            case epoc::event_code::null:
            case epoc::event_code::key:
            case epoc::event_code::touch_enter:
            case epoc::event_code::touch_exit: {
                q_.erase(q_.begin() + i);
                break;
            }
            case epoc::event_code::touch: {
                q_.erase(q_.begin() + i);
                break;
            }
            case epoc::event_code::focus_gained:
            case epoc::event_code::focus_lost: {
                if ((i + 1 < q_.size()) && ((q_[i + 1].evt.type == epoc::event_code::focus_gained) || (q_[i + 1].evt.type == epoc::event_code::focus_lost))) {
                    q_.erase(q_.begin() + i + 1);
                    q_.erase(q_.begin() + i);
                }
                break;
            }
            case epoc::event_code::switch_on: {
                if (i + 1 < q_.size() && (q_[i + 1].evt.type == epoc::event_code::switch_on)) {
                    q_.erase(q_.begin() + i);
                    break;
                }
            }
            default: {
                LOG_ERROR(SERVICE_WINDOW, "Unhandled purge of event type: {}", static_cast<int>(q_[i].evt.type));
                assert(false);
                break;
            }
            }
        }
    }

    event event_fifo::get_event() {
        std::optional<event> evt = get_evt_opt();
        if (!evt) {
            return event(0, event_code::null);
        }
        return *evt;
    }

    std::uint32_t redraw_fifo::queue_event(void *owner, const redraw_event &evt, const std::uint16_t pri) {
        const std::lock_guard<std::mutex> guard(lock_);
        eka2l1::rect target_queue_rect(evt.top_left, evt.bottom_right);
        target_queue_rect.transform_from_symbian_rectangle();

        std::size_t limit = q_.size();
        for (std::size_t i = 0; i < limit; i++) {
            eka2l1::rect queued_rect(q_[i].evt.evt_.top_left, q_[i].evt.evt_.bottom_right);
            queued_rect.transform_from_symbian_rectangle();

            if ((q_[i].evt.evt_.handle == evt.handle) && target_queue_rect.contains(queued_rect)) {
                q_.erase(q_.begin() + i);
                limit--;
            }
        }

        redraw_event_full full_event;
        full_event.owner_ = owner;
        full_event.evt_ = evt;

        std::uint32_t id = queue_event_dont_care(full_event);
        q_.back().pri = pri;

        std::stable_sort(q_.begin(), q_.end(),
            [&](const fifo_element &e1, const fifo_element &e2) {
                return e1.pri < e2.pri;
            });
        return id;
    }

    void redraw_fifo::remove_events(void *owner) {
        const std::lock_guard<std::mutex> guard(lock_);
        common::erase_elements(q_, [owner](fifo_element &elem) -> bool {
            return elem.evt.owner_ == owner;
        });
    }
}