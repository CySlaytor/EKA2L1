#include <services/comm/comm.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    std::string get_comm_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::eka2) {
            return "CommServer";
        }

        return "!CommServer";
    }
}