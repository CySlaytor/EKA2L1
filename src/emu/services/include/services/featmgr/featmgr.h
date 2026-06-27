#pragma once

#include <common/common.h>
#include <kernel/server.h>
#include <vector>

namespace eka2l1 {
    class io_system;

    struct featmgr_config_header {
        char magic[4];
        uint32_t unk1;
        uint32_t num_entry;
        uint32_t num_range;
    };

    struct featmgr_config_entry {
        uint32_t uid;
        uint32_t info;
    };

    struct featmgr_config_range {
        uint32_t low_uid;
        uint32_t high_uid;
    };

    struct feature_entry {
        std::uint32_t feature_id_;
        std::uint32_t flags_;
        std::uint32_t data_;
        std::uint32_t reserved_;
    };

    class featmgr_server : public service::server {
        std::vector<epoc::uid> enable_features;
        std::vector<featmgr_config_range> enable_feature_ranges;

        bool config_loaded = false;

        bool load_featmgr_configs(io_system *io);
        void feature_supported(service::ipc_context &ctx);

        void do_feature_scanning(system *sys);

    public:
        featmgr_server(system *sys);
    };
}