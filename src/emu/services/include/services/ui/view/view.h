#pragma once

#include <services/framework.h>
#include <services/ui/view/common.h>
#include <vector>

namespace eka2l1 {
    std::string get_view_server_name_by_epocver(const epocver ver);

    class view_server : public service::typical_server {
    public:
        explicit view_server(system *sys);
    };
};