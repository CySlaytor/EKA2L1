#pragma once

namespace eka2l1::epoc::keysound {
    enum opcode {
        opcode_init = 0,
        opcode_play_key = 1,
        opcode_play_sid = 2,
        opcode_add_sids = 3,
        opcode_push_context = 5,
        opcode_pop_context = 6,
        opcode_bring_to_foreground = 7,
        opcode_lock_context = 9
    };
}