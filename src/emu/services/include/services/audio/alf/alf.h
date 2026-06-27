#pragma once

#include <services/framework.h>

namespace eka2l1 {
    class alf_streamer_server : public service::typical_server {
    public:
        explicit alf_streamer_server(system *sys);
    };
}