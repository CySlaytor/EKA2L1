#pragma once

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    std::string get_comm_server_name_by_epocver(const epocver ver);
}