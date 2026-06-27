#pragma once

#include <common/common.h>
#include <common/types.h>
#include <vector>

namespace eka2l1 {
    class central_repo_server;
    struct central_repo;
    class io_system;
    class device_manager;
}

namespace eka2l1::epoc {
    class akn_skin_icon_config_map {
        eka2l1::central_repo_server *cenrep_serv_;
        io_system *io_;
        device_manager *mngr_;
        std::vector<epoc::uid> cfgs_;
        bool inited_ = false;
        language sys_lang_;

    protected:
        void read_and_parse_repo();

    public:
        explicit akn_skin_icon_config_map(central_repo_server *cenrep_,
            device_manager *mngr, io_system *io_, const language lang_);

        int is_icon_configured(const epoc::uid app_uid);
    };
}