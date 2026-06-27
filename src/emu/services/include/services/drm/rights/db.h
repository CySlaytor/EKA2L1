#pragma once

#include <cstdint>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace eka2l1::epoc::drm {
    enum rights_export_mode {
        rights_export_mode_none,
        rights_export_mode_forward,
        rights_export_mode_copy,
        rights_export_mode_move
    };

    struct rights_permission {
        std::uint32_t unique_id_;
        std::uint64_t insert_time_;
        std::uint16_t version_rights_main_;
        std::uint16_t version_rights_sub_;
        rights_export_mode export_mode_;
        std::uint32_t info_bits_;
        std::string parent_rights_id_;
        std::string rights_obj_id;
        std::string domain_id_;
        std::string right_issuer_identifier_;
        std::string on_expired_url_;
    };

    struct rights_database {
    private:
        sqlite3 *database_;
        sqlite3_stmt *get_cid_exist_stmt_;
        sqlite3_stmt *delete_record_stmt_;
        sqlite3_stmt *add_right_info_stmt_;
        sqlite3_stmt *add_perm_info_stmt_;
        sqlite3_stmt *add_constraint_info_stmt_;
        sqlite3_stmt *set_perm_constraints_stmt_;
        sqlite3_stmt *query_perms_stmt_;
        sqlite3_stmt *query_constraint_stmt_;
        sqlite3_stmt *get_encrypt_key_stmt_;
        sqlite3_stmt *get_cid_exist_full_stmt_;

        std::u16string db_path_;

        bool get_permission_list(const int right_id, std::vector<rights_permission> &permissions);
        void reset();

    public:
        explicit rights_database(const std::u16string &database);
        ~rights_database();

        int get_entry_id(const std::string &content_id);
        bool get_permission_list(const std::string &content_id, std::vector<rights_permission> &permissions);
        void flush();
    };
}