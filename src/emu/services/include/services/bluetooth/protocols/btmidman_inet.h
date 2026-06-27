#pragma once

#include <config/config.h>
#include <services/bluetooth/btmidman.h>

namespace eka2l1::epoc::bt {
    class midman_inet : public midman {
    public:
        explicit midman_inet(const config::state &conf);
        ~midman_inet() override;

        midman_type type() const override {
            return MIDMAN_INET_BT;
        }
    };
}