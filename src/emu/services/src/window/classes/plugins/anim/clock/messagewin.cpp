#include <common/log.h>
#include <services/window/classes/plugins/anim/clock/messagewin.h>
#include <services/window/classes/winuser.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    messagewin_anim_executor::messagewin_anim_executor(canvas_base *canvas)
        : anim_executor(canvas) {
        canvas->set_visible(false);
    }

    messagewin_anim_executor::~messagewin_anim_executor() {
        canvas_->set_visible(true);
    }
}