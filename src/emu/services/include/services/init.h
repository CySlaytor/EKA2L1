#pragma once

namespace eka2l1 {
    class system;

    namespace service {
        void init_services(system *sys);
        void init_services_post_bootup(system *sys);
    }
}