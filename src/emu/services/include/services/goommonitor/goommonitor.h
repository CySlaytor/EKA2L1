#pragma once

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    class goom_monitor_server : public service::typical_server {
    public:
        explicit goom_monitor_server(eka2l1::system *sys);
    };
}