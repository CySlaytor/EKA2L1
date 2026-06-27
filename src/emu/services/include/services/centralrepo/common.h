#pragma once

#include <cstdint>

namespace eka2l1 {
    enum central_repo_srv_request {
        cen_rep_init = 0,
        cen_rep_create_int,
        cen_rep_create_real,
        cen_rep_create_string,
        cen_rep_get_int = 5,
        cen_rep_set_int,
        cen_rep_get_real,
        cen_rep_set_real,
        cen_rep_get_string,
        cen_rep_set_string,
        cen_rep_find,
        cen_rep_find_eq_int,
        cen_rep_find_eq_real,
        cen_rep_find_eq_string,
        cen_rep_find_neq_int,
        cen_rep_find_neq_real,
        cen_rep_find_neq_string,
        cen_rep_get_find_res,
        cen_rep_notify_req_check,
        cen_rep_notify_req,
        cen_rep_notify_cancel,
        cen_rep_notify_cancel_all,
        cen_rep_group_nof_req,
        cen_rep_group_nof_cancel,
        cen_rep_reset,
        cen_rep_transaction_start = 27,
        cen_rep_transaction_cancel = 29,
        cen_rep_close = 35
    };

    enum {
        CENTRAL_REPO_UID = 0x10202be9
    };

    enum class central_repo_entry_type {
        none,
        integer,
        real,
        string
    };

    static constexpr int cenrep_pma_ver = 3;
}