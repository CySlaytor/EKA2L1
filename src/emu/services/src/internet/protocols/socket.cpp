#include <services/internet/protocols/inet.h>

#include <common/log.h>
#include <common/platform.h>
#include <common/thread.h>

extern "C" {
#include <uv.h>
}

#include <uvlooper/uvlooper.h>

namespace eka2l1::epoc::internet {
    void inet_bridged_protocol::initialize_looper() {
        if (!looper_->started()) {
            looper_->set_loop_thread_prepare_callback([]() { common::set_thread_priority(common::thread_priority_very_high); });
            looper_->start();
        }
    }
}