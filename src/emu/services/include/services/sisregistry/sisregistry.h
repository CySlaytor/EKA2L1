#pragma once

#include <services/framework.h>

namespace eka2l1 {
    class sisregistry_server : public service::typical_server {
    public:
        explicit sisregistry_server(eka2l1::system *sys);
    };
}