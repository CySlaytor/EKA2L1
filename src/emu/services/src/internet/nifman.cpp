#include <common/cvt.h>
#include <common/log.h>
#include <services/internet/nifman.h>
#include <services/socket/server.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    nifman_server::nifman_server(eka2l1::system *sys)
        : service::typical_server(sys, "NifmanServer")
        , sock_serv_(nullptr) {
    }
}