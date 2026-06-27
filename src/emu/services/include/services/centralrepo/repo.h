#pragma once

#include <services/centralrepo/common.h>
#include <utils/reqsts.h>
#include <utils/sec.h>

#include <common/types.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace eka2l1 {
    namespace service {
        struct ipc_context;
    }

    class device_manager;
    class central_repo_server;
    class io_system;

    struct central_repo_entry_variant {
        central_repo_entry_type etype;
        std::uint64_t intd;
        double reald;
        std::string strd;
    };

    struct central_repo_entry {
        std::uint32_t key;
        central_repo_entry_variant data;
        std::uint32_t metadata_val;
    };

    enum class central_repo_transaction_mode : std::uint16_t {
        read_only,
        read_write,
        read_write_async
    };

    struct central_repo_entry_access_policy {
        std::uint32_t low_key;
        std::uint32_t high_key;
        std::uint32_t key_mask;

        epoc::security_policy read_access;
        epoc::security_policy write_access;
    };

    struct central_repo_default_meta {
        std::uint32_t low_key;
        std::uint32_t high_key;
        std::uint32_t key_mask;
        std::uint32_t default_meta_data;
    };

    struct central_repo_client_subsession;

    struct central_repo {
        drive_number reside_place;

        std::uint32_t access_count;

        std::uint8_t ver;
        std::uint8_t keyspace_type;
        std::uint32_t uid;

        std::uint32_t owner_uid;

        std::vector<central_repo_entry> entries;
        std::vector<central_repo_client_subsession *> attached;

        central_repo_entry_access_policy default_policy;
        std::vector<central_repo_entry_access_policy> single_policies;
        std::vector<central_repo_entry_access_policy> policies_range;

        std::uint32_t default_meta = 0;
        std::vector<central_repo_default_meta> meta_range;

        std::uint64_t time_stamp;

        std::vector<std::uint32_t> deleted_settings;

        void write_changes(eka2l1::io_system *io, device_manager *mngr);
        central_repo_entry *find_entry(const std::uint32_t key);

        std::uint32_t get_default_meta_for_new_key(const std::uint32_t key);

        bool add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var);
        bool add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var,
            const std::uint32_t meta);

        void query_entries(const std::uint32_t partial_key, const std::uint32_t mask,
            std::vector<central_repo_entry *> &matched_entries,
            const central_repo_entry_type etype);
    };

    struct central_repo_transactor {
        std::unordered_map<std::uint32_t, central_repo_entry> changes;
        central_repo_client_subsession *session;
    };

    struct central_repo_key_filter {
        std::uint32_t partial_key;
        std::uint32_t id_mask;
    };

    struct central_repo_client_subsession {
        central_repo_server *server;

        central_repo *attach_repo;
        central_repo_transactor transactor;

        std::vector<std::uint32_t> key_found_result;

        explicit central_repo_client_subsession();

        int reset_key(eka2l1::central_repo *init_repo, const std::uint32_t key);
        void write_changes(eka2l1::io_system *io, device_manager *mngr);

        void find(service::ipc_context *ctx);
        void reset(service::ipc_context *ctx);
        void set_value(service::ipc_context *ctx);
        void get_value(service::ipc_context *ctx);
        void notify_nof_check(service::ipc_context *ctx);
        void notify(service::ipc_context *ctx);
        void notify_cancel(service::ipc_context *ctx);
        void get_find_result(service::ipc_context *ctx);
        void start_transaction(service::ipc_context *ctx);
        void cancel_transaction(service::ipc_context *ctx);
        void create_value(service::ipc_context *ctx);

        void append_new_key_to_found_eq_list(std::uint32_t *array, const std::uint32_t key,
            const std::uint32_t max_uids_buf);

        void handle_message(service::ipc_context *ctx);

        struct cenrep_notify_info {
            epoc::notify_info sts;

            std::uint32_t mask;
            std::uint32_t match;
        };

        std::vector<cenrep_notify_info> notifies;

        enum session_flags {
            active = 0x1
        };

        std::uint32_t flags;

        bool is_active() const {
            return (flags >> 16) & active;
        }

        void set_active(const bool b) {
            std::uint16_t aflags = flags >> 16;
            flags &= 0x0000FFFF;

            aflags &= ~active;

            if (b) {
                aflags |= active;
            }

            flags |= (aflags << 16);
        }

        void set_transaction_mode(const central_repo_transaction_mode mode) {
            flags &= 0xFFFF0000;
            flags |= static_cast<int>(mode);
        }

        central_repo_transaction_mode get_transaction_mode() {
            return static_cast<central_repo_transaction_mode>(flags);
        }

        central_repo_entry *get_entry(const std::uint32_t key, int mode);

        void modification_success(const std::uint32_t key);

        int add_notify_request(epoc::notify_info &info, const std::uint32_t mask, const std::uint32_t match);

        void cancel_all_notify_requests();

        void cancel_notify_request(const std::uint32_t match_key, const std::uint32_t mask);
    };
}