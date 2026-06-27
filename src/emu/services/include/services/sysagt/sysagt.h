#pragma once

#include <services/framework.h>

namespace eka2l1 {
    class system_agent_server : public service::typical_server {
    public:
        explicit system_agent_server(eka2l1::system *sys);
    };
}