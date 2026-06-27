#pragma once

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    class sensor_server : public service::typical_server {
    public:
        explicit sensor_server(eka2l1::system *sys);
    };
}