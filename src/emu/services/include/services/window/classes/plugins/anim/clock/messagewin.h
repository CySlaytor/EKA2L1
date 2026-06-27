#pragma once

#include <services/window/classes/plugins/animdll.h>

namespace eka2l1::epoc {
    struct messagewin_anim_executor : public anim_executor {
    public:
        explicit messagewin_anim_executor(canvas_base *canvas);
        std::int32_t handle_request(const std::int32_t opcode, void *args) override;
        ~messagewin_anim_executor();
    };
}