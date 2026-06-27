#include <services/drm/rights/db.h>

#include <common/algorithm.h>
#include <common/crypt.h>
#include <common/log.h>

namespace eka2l1::epoc::drm {
    rights_database::rights_database(const std::u16string &database_path)
        : database_(nullptr)
        , get_cid_exist_stmt_(nullptr)
        , delete_record_stmt_(nullptr)
        , add_right_info_stmt_(nullptr)
        , add_perm_info_stmt_(nullptr)
        , add_constraint_info_stmt_(nullptr)
        , set_perm_constraints_stmt_(nullptr)
        , query_perms_stmt_(nullptr)
        , query_constraint_stmt_(nullptr)
        , get_encrypt_key_stmt_(nullptr)
        , get_cid_exist_full_stmt_(nullptr)
        , db_path_(database_path) {
        int result = sqlite3_open16(database_path.data(), &database_);
        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to open DRM database!");
            return;
        }

        result = sqlite3_exec(database_, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
        if (result != SQLITE_OK) {
            LOG_WARN(SERVICE_DRMSYS, "Foreign keys can not be enabled on DRM database!");
        }

        static const char *CREATE_RIGHT_INFO_TABLE = "CREATE TABLE IF NOT EXISTS RightInfo("
                                                     "id INTEGER PRIMARY KEY,"
                                                     "contentId STRING,"
                                                     "contentHash STRING,"
                                                     "issuer STRING,"
                                                     "contentName STRING,"
                                                     "authSeed STRING,"
                                                     "encryptKey STRING"
                                                     ")";

        result = sqlite3_exec(database_, CREATE_RIGHT_INFO_TABLE, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to create right info table. Last error: {}!", sqlite3_errmsg(database_));
            return;
        }

        static const char *CREATE_PERMISSION_INFO_TABLE = "CREATE TABLE IF NOT EXISTS PermissionInfo("
                                                          "id INTEGER PRIMARY KEY,"
                                                          "rightsId INTEGER,"
                                                          "insertTime INTEGER,"
                                                          "version INTEGER,"
                                                          "topLevelConstraintId INTEGER,"
                                                          "playConstraintId INTEGER,"
                                                          "displayConstraintId INTEGER,"
                                                          "executeConstraintId INTEGER,"
                                                          "printConstraintId INTEGER,"
                                                          "exportConstraintId INTEGER,"
                                                          "exportMode INTEGER,"
                                                          "infoBits INTEGER,"
                                                          "parentRightsCid STRING,"
                                                          "myRightsCid STRING,"
                                                          "domainCid STRING,"
                                                          "rightIssuerIdentifier STRING,"
                                                          "onExpiredUrl STRING,"
                                                          "FOREIGN KEY (topLevelConstraintId) REFERENCES ConstraintInfo(id),"
                                                          "FOREIGN KEY (playConstraintId) REFERENCES ConstraintInfo(id),"
                                                          "FOREIGN KEY (displayConstraintId) REFERENCES ConstraintInfo(id),"
                                                          "FOREIGN KEY (executeConstraintId) REFERENCES ConstraintInfo(id),"
                                                          "FOREIGN KEY (printConstraintId) REFERENCES ConstraintInfo(id),"
                                                          "FOREIGN KEY (exportConstraintId) REFERENCES ConstraintInfo(id),"
                                                          "CONSTRAINT fk_rightsId FOREIGN KEY (rightsId) REFERENCES RightInfo(id) ON DELETE CASCADE"
                                                          ")";

        result = sqlite3_exec(database_, CREATE_PERMISSION_INFO_TABLE, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to create permission info table!");
            return;
        }

        static const char *CREATE_INDEX_PERM_TABLE_STMT_STRING = "CREATE INDEX IF NOT EXISTS PermissionInfo_RightsId ON PermissionInfo(rightsId)";
        result = sqlite3_exec(database_, CREATE_INDEX_PERM_TABLE_STMT_STRING, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_WARN(SERVICE_DRMSYS, "Fail to create indexing table for permission info!");
        }

        static const char *CREATE_CONSTRAINT_INFO_TABLE = "CREATE TABLE IF NOT EXISTS ConstraintInfo("
                                                          "id INTEGER PRIMARY KEY,"
                                                          "permissionId INTEGER,"
                                                          "startTime INTEGER,"
                                                          "endTime INTEGER,"
                                                          "intervalStart INTEGER,"
                                                          "interval INTEGER,"
                                                          "counter INTEGER,"
                                                          "originalCounter INTEGER,"
                                                          "timedCounter INTEGER,"
                                                          "timedInterval INTEGER,"
                                                          "accumulatedTime INTEGER,"
                                                          "individual STRING,"
                                                          "system STRING,"
                                                          "vendorId INTEGER,"
                                                          "secureId INTEGER,"
                                                          "constraintFlags INTEGER,"
                                                          "meteringGraceTime INTEGER,"
                                                          "originalTimedCounter INTEGER,"
                                                          "CONSTRAINT fk_permId FOREIGN KEY (permissionId) REFERENCES PermissionInfo(id) ON DELETE CASCADE"
                                                          ")";

        result = sqlite3_exec(database_, CREATE_CONSTRAINT_INFO_TABLE, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to create constraint info table!");
            return;
        }

        static const char *CREATE_INDEX_CONSTRAINT_TABLE_STMT_STRING = "CREATE INDEX IF NOT EXISTS ConstraintInfo_PermId ON ConstraintInfo(permissionId)";
        result = sqlite3_exec(database_, CREATE_INDEX_CONSTRAINT_TABLE_STMT_STRING, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_WARN(SERVICE_DRMSYS, "Fail to create indexing table for constraint info!");
        }
    }

    rights_database::~rights_database() {
        reset();
    }

    void rights_database::reset() {
        if (get_cid_exist_stmt_) {
            sqlite3_finalize(get_cid_exist_stmt_);
            get_cid_exist_stmt_ = nullptr;
        }

        if (delete_record_stmt_) {
            sqlite3_finalize(delete_record_stmt_);
            delete_record_stmt_ = nullptr;
        }

        if (add_right_info_stmt_) {
            sqlite3_finalize(add_right_info_stmt_);
            add_right_info_stmt_ = nullptr;
        }

        if (add_perm_info_stmt_) {
            sqlite3_finalize(add_perm_info_stmt_);
            add_perm_info_stmt_ = nullptr;
        }

        if (add_constraint_info_stmt_) {
            sqlite3_finalize(add_constraint_info_stmt_);
            add_constraint_info_stmt_ = nullptr;
        }

        if (set_perm_constraints_stmt_) {
            sqlite3_finalize(set_perm_constraints_stmt_);
            set_perm_constraints_stmt_ = nullptr;
        }

        if (query_perms_stmt_) {
            sqlite3_finalize(query_perms_stmt_);
            query_perms_stmt_ = nullptr;
        }

        if (query_constraint_stmt_) {
            sqlite3_finalize(query_constraint_stmt_);
            query_constraint_stmt_ = nullptr;
        }

        if (get_encrypt_key_stmt_) {
            sqlite3_finalize(get_encrypt_key_stmt_);
            get_encrypt_key_stmt_ = nullptr;
        }

        if (get_cid_exist_full_stmt_) {
            sqlite3_finalize(get_cid_exist_full_stmt_);
            get_cid_exist_full_stmt_ = nullptr;
        }

        if (database_) {
            sqlite3_close(database_);
            database_ = nullptr;
        }
    }

    void rights_database::flush() {
        reset();

        int result = sqlite3_open16(db_path_.c_str(), &database_);
        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to reopen DRM database!");
            return;
        }
    }

    int rights_database::get_entry_id(const std::string &content_id) {
        if (!get_cid_exist_stmt_) {
            static const char *get_cid_EXIST_STMT_STRING = "SELECT id FROM RightInfo WHERE contentId=:targetContentId COLLATE NOCASE";
            if (sqlite3_prepare(database_, get_cid_EXIST_STMT_STRING, -1, &get_cid_exist_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare check CID exist SQL statement! Error: {}", sqlite3_errmsg(database_));
                return -1;
            }
        }

        sqlite3_reset(get_cid_exist_stmt_);
        if (sqlite3_bind_text(get_cid_exist_stmt_, 1, content_id.data(), static_cast<int>(content_id.length()), nullptr) != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to bind content ID string to SQL statement!");
            return -1;
        }

        if (sqlite3_step(get_cid_exist_stmt_) == SQLITE_ROW) {
            return sqlite3_column_int(get_cid_exist_stmt_, 0);
        }

        return -1;
    }

    bool rights_database::get_permission_list(const int rights_id, std::vector<rights_permission> &permissions) {
        if (!query_perms_stmt_) {
            static const char *QUERY_PERMISSION_ENTRIES_STMT_STRING = "SELECT * FROM PermissionInfo WHERE rightsId=:targetRightsId";
            if (sqlite3_prepare(database_, QUERY_PERMISSION_ENTRIES_STMT_STRING, -1, &query_perms_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare permission query statement!");
                return false;
            }
        }

        sqlite3_reset(query_perms_stmt_);
        sqlite3_bind_int(query_perms_stmt_, 1, rights_id);

        do {
            int result = sqlite3_step(query_perms_stmt_);
            if (result != SQLITE_ROW) {
                break;
            }

            rights_permission perm;

            perm.unique_id_ = static_cast<std::uint32_t>(sqlite3_column_int(query_perms_stmt_, 0));
            perm.insert_time_ = static_cast<std::uint64_t>(sqlite3_column_int64(query_perms_stmt_, 2));
            std::uint32_t version = static_cast<std::uint32_t>(sqlite3_column_int(query_perms_stmt_, 3));
            perm.version_rights_main_ = static_cast<std::uint16_t>(version >> 16);
            perm.version_rights_sub_ = static_cast<std::uint16_t>(version);

            perm.export_mode_ = static_cast<rights_export_mode>(sqlite3_column_int(query_perms_stmt_, 10));
            perm.info_bits_ = static_cast<std::uint32_t>(sqlite3_column_int(query_perms_stmt_, 11));

            const std::uint8_t *parent_rights_cid_data = sqlite3_column_text(query_perms_stmt_, 12);
            const std::uint8_t *my_rights_cid_data = sqlite3_column_text(query_perms_stmt_, 13);
            const std::uint8_t *domain_cid_data = sqlite3_column_text(query_perms_stmt_, 14);
            const std::uint8_t *rights_issuer_id_data = sqlite3_column_text(query_perms_stmt_, 15);
            const std::uint8_t *on_expired_url_data = sqlite3_column_text(query_perms_stmt_, 16);

            perm.parent_rights_id_.resize(crypt::base64_decode(parent_rights_cid_data, strlen(reinterpret_cast<const char *>(parent_rights_cid_data)), nullptr, 0));
            perm.rights_obj_id.resize(crypt::base64_decode(my_rights_cid_data, strlen(reinterpret_cast<const char *>(my_rights_cid_data)), nullptr, 0));
            perm.domain_id_.resize(crypt::base64_decode(domain_cid_data, strlen(reinterpret_cast<const char *>(domain_cid_data)), nullptr, 0));
            perm.on_expired_url_.resize(crypt::base64_decode(on_expired_url_data, strlen(reinterpret_cast<const char *>(on_expired_url_data)), nullptr, 0));

            crypt::base64_decode(parent_rights_cid_data, strlen(reinterpret_cast<const char *>(parent_rights_cid_data)), perm.parent_rights_id_.data(), perm.parent_rights_id_.size());
            crypt::base64_decode(my_rights_cid_data, strlen(reinterpret_cast<const char *>(my_rights_cid_data)), perm.rights_obj_id.data(), perm.rights_obj_id.size());
            crypt::base64_decode(domain_cid_data, strlen(reinterpret_cast<const char *>(domain_cid_data)), perm.domain_id_.data(), perm.domain_id_.size());
            crypt::base64_decode(rights_issuer_id_data, strlen(reinterpret_cast<const char *>(rights_issuer_id_data)), reinterpret_cast<char *>(perm.right_issuer_identifier_.data()), perm.right_issuer_identifier_.size());
            crypt::base64_decode(on_expired_url_data, strlen(reinterpret_cast<const char *>(on_expired_url_data)), perm.on_expired_url_.data(), perm.on_expired_url_.size());

            permissions.push_back(perm);
        } while (true);

        return true;
    }

    bool rights_database::get_permission_list(const std::string &content_id, std::vector<rights_permission> &permissions) {
        int rights_id = get_entry_id(content_id);
        if (rights_id < 0) {
            return false;
        }

        return get_permission_list(rights_id, permissions);
    }
}