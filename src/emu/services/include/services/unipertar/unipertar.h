#pragma once

#include <services/framework.h>

namespace eka2l1 {
    class unipertar_server : public service::typical_server {
    public:
        explicit unipertar_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };
}