#include <services/unipertar/unipertar.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    unipertar_server::unipertar_server(eka2l1::system *sys)
        : service::typical_server(sys, "Unipertar") {
    }

    void unipertar_server::connect(service::ipc_context &context) {
        context.complete(epoc::error_none);
    }
}