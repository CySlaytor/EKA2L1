#pragma once

#include <memory>
#include <services/centralrepo/repo.h>

namespace eka2l1 {
    namespace common {
        class chunkyseri;
    }

    int do_state_for_cre(common::chunkyseri &seri, eka2l1::central_repo &repo);
}