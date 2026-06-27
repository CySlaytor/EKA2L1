#include <services/drm/notifier/notifier.h>
#include <utils/err.h>

namespace eka2l1 {
    bool drm_notifier_client_session::listen(epoc::notify_info &info, std::uint8_t *data_to_write, epoc::des8 *event_type_to_write) {
        if (!notify_.empty()) {
            return false;
        }

        notify_ = info;
        to_write_data_ = data_to_write;
        to_write_event_type_ = event_type_to_write;

        return true;
    }

    bool drm_notifier_client_session::register_event(const std::uint32_t type, const std::string uri) {
        for (std::size_t i = 0; i < accept_types_.size(); i++) {
            if ((accept_types_[i].event_type_ == type) && (accept_types_[i].content_uri_ == uri)) {
                return false;
            }
        }

        accept_types_.push_back({ type, uri });
        return true;
    }

    bool drm_notifier_client_session::unregister_event(const std::uint32_t type, const std::string uri) {
        for (std::size_t i = 0; i < accept_types_.size(); i++) {
            if ((accept_types_[i].event_type_ == type) && (accept_types_[i].content_uri_ == uri)) {
                accept_types_.erase(accept_types_.begin() + i);
                return true;
            }
        }

        return false;
    }
}