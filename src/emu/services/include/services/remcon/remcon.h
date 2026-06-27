#pragma once

#include <services/framework.h>

namespace eka2l1 {
    class remcon_server : public service::typical_server {
    public:
        explicit remcon_server(eka2l1::system *sys);
    };
}