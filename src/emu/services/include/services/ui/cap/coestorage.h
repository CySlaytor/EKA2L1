#pragma once

#include <optional>
#include <string>

namespace eka2l1 {
    struct central_repo;
    class central_repo_server;
    class device_manager;
    class io_system;
}

namespace eka2l1::epoc {
    class coe_data_storage {
        eka2l1::central_repo *fep_repo_;
        eka2l1::central_repo_server *serv_;

        device_manager *dmngr_;
        io_system *io_;

    public:
        explicit coe_data_storage(eka2l1::central_repo_server *serv, io_system *io,
            device_manager *mngr);

        eka2l1::central_repo *fep_repo();

        std::optional<std::u16string> default_fep();
        void default_fep(const std::u16string &the_fep);
        void serialize();
    };
}