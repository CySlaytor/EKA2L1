#include <services/sysagt/sysagt.h>
#include <system/epoc.h>

namespace eka2l1 {
    system_agent_server::system_agent_server(eka2l1::system *sys)
        : service::typical_server(sys, "SystemAgent") {
    }
}