#pragma once

#include <common/uid.h>

namespace eka2l1::ui::view {
    struct view_id {
        epoc::uid app_uid;
        epoc::uid view_uid;

        inline bool operator==(const view_id &rhs) {
            return (app_uid == rhs.app_uid) && (view_uid == rhs.view_uid);
        }
    };

    static const view_id EMPTY_VIEW_ID = view_id{ 0, 0 };

    struct view_event {
        enum event_type {
            event_active_view = 0,
            event_deactive_view = 1,
            event_screen_device_change = 2,
            event_deactive_notification = 3,
            event_active_notification = 4,
            event_deactive_view_different_instance = 5
        };

        event_type view_event_type;
        view_id view_one_id;
        view_id view_two_id;
        epoc::uid custom_message_id;
        std::int32_t custom_message_length;
    };
}