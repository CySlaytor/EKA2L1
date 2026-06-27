#include <services/goommonitor/goommonitor.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    goom_monitor_server::goom_monitor_server(eka2l1::system *sys)
        : service::typical_server(sys, "GOomMonitorServer") {
    }
}