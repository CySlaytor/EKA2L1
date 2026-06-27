#include <services/sisregistry/sisregistry.h>
#include <system/epoc.h>

namespace eka2l1 {
    sisregistry_server::sisregistry_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SisRegistryServer") {
    }
}