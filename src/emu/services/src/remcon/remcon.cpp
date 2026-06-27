#include <common/log.h>
#include <services/remcon/remcon.h>
#include <utils/err.h>

namespace eka2l1 {
    remcon_server::remcon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!RemConSrv") {
    }
}