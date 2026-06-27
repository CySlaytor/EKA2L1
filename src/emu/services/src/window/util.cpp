#include <common/log.h>
#include <drivers/itc.h>
#include <services/window/common.h>
#include <services/window/util.h>

namespace eka2l1 {
    void scale_rectangle(eka2l1::rect &r, const float scale_factor) {
        r.top *= scale_factor;
        r.size *= scale_factor;
    }
}