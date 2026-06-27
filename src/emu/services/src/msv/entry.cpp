#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/time.h>

#include <loader/rsc.h>
#include <services/msv/common.h>
#include <services/msv/entry.h>
#include <vfs/vfs.h>

#include <utils/bafl.h>
#include <utils/err.h>

#include <string>

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace eka2l1::epoc::msv {
    constexpr std::uint32_t MSV_FOLDER_TYPE_SERVICE = 1;

    std::u16string get_folder_name(const std::uint32_t owner, const std::uint32_t type) {
        return common::utf8_to_ucs2(fmt::format("{:08x}_{:08x}", type, owner));
    }

    entry_indexer::entry_indexer(eka2l1::io_system *io, const std::u16string &msg_folder, int preferred_lang)
        : io_(io)
        , rom_drv_(drive_z)
        , preferred_lang_(preferred_lang)
        , msg_dir_(msg_folder) {
        drive_number drv_target = drive_z;

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            auto drv_entry = io->get_drive_entry(drv);

            if (drv_entry && (drv_entry->media_type == drive_media::rom)) {
                rom_drv_ = drv;
                break;
            }
        }

        root_entry_.id_ = MSV_ROOT_ID_VALUE;
        root_entry_.mtm_uid_ = MTM_SERVICE_UID_ROOT;
        root_entry_.type_uid_ = MTM_SERVICE_UID_ROOT;
        root_entry_.parent_id_ = epoc::error_not_found;
        root_entry_.data_ = 0;
        root_entry_.service_id_ = MSV_ROOT_ID_VALUE;
        root_entry_.time_ = common::get_current_utc_time_in_microseconds_since_0ad();
        root_entry_.visible_id_ = 0;

        root_entry_.visible_folder(true);
    }

    entry_indexer::~entry_indexer() {
        while (!folders_.empty()) {
            visible_folder *ff = E_LOFF(folders_.first()->deque(), visible_folder, indexer_link_);
            delete ff;
        }
    }

    std::optional<std::u16string> entry_indexer::get_entry_data_directory(const std::uint32_t id, const std::uint32_t type,
        const std::uint32_t owning_service) {
        if ((type == MSV_SERVICE_UID) || (type == MTM_SERVICE_UID_ROOT)) {
            return msg_dir_;
        }

        const std::u16string file_path = eka2l1::add_path(msg_dir_, get_folder_name(owning_service, MSV_FOLDER_TYPE_SERVICE));
        const std::u16string hiearchy_folder = eka2l1::add_path(file_path, common::utf8_to_ucs2(fmt::format("{:X}\\", id & 0xF)));

        if (!io_->exist(hiearchy_folder)) {
            io_->create_directories(hiearchy_folder);
        }

        return hiearchy_folder;
    }

    std::optional<std::u16string> entry_indexer::get_entry_data_file(entry *ent) {
        if (!ent) {
            return std::nullopt;
        }

        std::uint32_t oss = 0;
        owning_service(ent->id_, oss);

        std::optional<std::u16string> base = get_entry_data_directory(ent->id_, ent->type_uid_, oss);
        if (!base.has_value()) {
            return base;
        }

        return eka2l1::add_path(base.value(), common::utf8_to_ucs2(fmt::format("{:08X}", ent->id_)));
    }

    bool entry_indexer::owning_service(const std::uint32_t id, std::uint32_t &owning) {
        if (id == MSV_ROOT_ID_VALUE) {
            return MSV_ROOT_ID_VALUE;
        }

        std::uint32_t lookup_id = id;

        while (true) {
            entry *ent = get_entry(lookup_id);
            if (!ent) {
                return false;
            }

            if (ent->type_uid_ == MSV_SERVICE_UID) {
                owning = ent->id_;
                return true;
            }

            lookup_id = ent->parent_id_;
        }

        return false;
    }

    sql_entry_indexer::sql_entry_indexer(eka2l1::io_system *io, const std::u16string &msg_folder, int preferred_lang)
        : entry_indexer(io, msg_folder, preferred_lang)
        , database_(nullptr)
        , create_entry_stmt_(nullptr)
        , change_entry_stmt_(nullptr)
        , relocate_entry_stmt_(nullptr)
        , visible_folder_find_stmt_(nullptr)
        , find_entry_stmt_(nullptr)
        , query_child_entries_stmt_(nullptr)
        , query_child_ids_stmt_(nullptr)
        , id_counter_(MSV_FIRST_FREE_ENTRY_ID - 1) {
        bool newly_created = false;

        if (load_or_create_databases(newly_created)) {
            if (newly_created) {
                add_entry(root_entry_);
                create_standard_entries(drive_c);
            }
        }
    }

    sql_entry_indexer::~sql_entry_indexer() {
        if (create_entry_stmt_) {
            sqlite3_finalize(create_entry_stmt_);
        }

        if (change_entry_stmt_) {
            sqlite3_finalize(change_entry_stmt_);
        }

        if (relocate_entry_stmt_) {
            sqlite3_finalize(relocate_entry_stmt_);
        }

        if (visible_folder_find_stmt_) {
            sqlite3_finalize(visible_folder_find_stmt_);
        }

        if (find_entry_stmt_) {
            sqlite3_finalize(find_entry_stmt_);
        }

        if (query_child_entries_stmt_) {
            sqlite3_finalize(query_child_entries_stmt_);
        }

        if (query_child_ids_stmt_) {
            sqlite3_finalize(query_child_ids_stmt_);
        }

        if (database_) {
            sqlite3_close(database_);
        }
    }

    static constexpr std::uint32_t MSV_SQL_DATABASE_VERSION = 2;

    bool sql_entry_indexer::load_or_create_databases(bool &newly_created) {
        newly_created = false;

        std::u16string vert_path = eka2l1::add_path(msg_dir_, u"messaging.db");
        std::optional<std::u16string> database_real_path = io_->get_raw_path(vert_path);

        if (!database_real_path) {
            LOG_ERROR(SERVICE_MSV, "Can't retrieve messaging database path!");
            return false;
        }

        if (!io_->exist(vert_path)) {
            newly_created = true;
        }

        if (sqlite3_open16(database_real_path.value().c_str(), &database_) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Fail to establish messaging database!");
            return false;
        }

        const char *INDEX_ENTRY_TABLE_CREATE_STM = "CREATE TABLE IF NOT EXISTS IndexEntry("
                                                   "id INTEGER,"
                                                   "parentId INTEGER,"
                                                   "serviceId INTEGER,"
                                                   "mtmId INTEGER,"
                                                   "type INTEGER,"
                                                   "date INTEGER,"
                                                   "data INTEGER,"
                                                   "size INTEGER,"
                                                   "error INTEGER,"
                                                   "mtmData1 INTEGER,"
                                                   "mtmData2 INTEGER,"
                                                   "mtmData3 INTEGER,"
                                                   "relatedId INTEGER,"
                                                   "bioType INTEGER,"
                                                   "pcSyncCount INTEGER,"
                                                   "reserved INTEGER,"
                                                   "visibleParent INTEGER,"
                                                   "description TEXT,"
                                                   "details TEXT,"
                                                   "PRIMARY KEY (id)"
                                                   ");";

        if (sqlite3_exec(database_, INDEX_ENTRY_TABLE_CREATE_STM, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Fail to create index entry table!");
            return false;
        }

        const char *INDEX_ENTRY_CREATE_PARENT_ID_INDEX_STM = "CREATE INDEX IF NOT EXISTS IndexEntry_ParentIndex ON IndexEntry(parentId);";

        if (sqlite3_exec(database_, INDEX_ENTRY_CREATE_PARENT_ID_INDEX_STM, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_WARN(SERVICE_MSV, "Fail to create index entry's parent indexing!");
        }

        const char *VERSION_TABLE_CREATE_STM = "CREATE TABLE IF NOT EXISTS VersionTable(version INTEGER PRIMARY KEY);";

        if (sqlite3_exec(database_, VERSION_TABLE_CREATE_STM, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_WARN(SERVICE_MSV, "Fail to create version table!");
        } else {
            std::string VERSION_TABLE_ADD_VERSION_STM = fmt::format("INSERT INTO VersionTable Values ({});", MSV_SQL_DATABASE_VERSION);
            if (sqlite3_exec(database_, VERSION_TABLE_ADD_VERSION_STM.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
                LOG_WARN(SERVICE_MSV, "Can't add version number to version table!");
            }
        }

        const char *MAX_ID_GET_STM = "SELECT MAX(id) FROM IndexEntry;";
        sqlite3_stmt *max_id_get_stmt_obj = nullptr;

        if (sqlite3_prepare(database_, MAX_ID_GET_STM, -1, &max_id_get_stmt_obj, nullptr) == SQLITE_OK) {
            const int res = sqlite3_step(max_id_get_stmt_obj);
            if ((res == SQLITE_ROW) || (res == SQLITE_DONE)) {
                id_counter_ = common::max<std::uint32_t>(id_counter_, static_cast<std::uint32_t>(sqlite3_column_int(max_id_get_stmt_obj, 0)));
            }

            sqlite3_finalize(max_id_get_stmt_obj);
        } else {
            LOG_WARN(SERVICE_MSV, "Can't get max ID for ID counter!");
        }

        return true;
    }

    msv_id sql_entry_indexer::get_suitable_visible_parent_id(const msv_id parent_id) {
        entry *parent_ent = entry_indexer::get_entry(parent_id);
        if (parent_ent) {
            if (!parent_ent->visible_folder()) {
                return parent_ent->visible_id_;
            } else {
                return parent_id;
            }
        }

        if (!visible_folder_find_stmt_) {
            const char *FIND_SUITABLE_VISIBLE_PARENT_STMT = "SELECT data, visibleParent FROM IndexEntry WHERE id=:parentId";
            if (sqlite3_prepare(database_, FIND_SUITABLE_VISIBLE_PARENT_STMT, -1, &visible_folder_find_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_MSV, "Unable to prepare find visible folder statement!");
                return 0;
            }
        }

        sqlite3_reset(visible_folder_find_stmt_);

        if (sqlite3_bind_int(visible_folder_find_stmt_, 1, parent_id) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Fail to bind parent id to visible folder find statement");
            return 0;
        }

        const int res = sqlite3_step(visible_folder_find_stmt_);
        msv_id result_id = 0;

        if ((res == SQLITE_ROW) || (res == SQLITE_DONE)) {
            const int data = sqlite3_column_int(visible_folder_find_stmt_, 0);
            const std::uint32_t visible_folder_id = static_cast<std::uint32_t>(sqlite3_column_int(visible_folder_find_stmt_, 1));

            if (data & entry::DATA_FLAG_INVISIBLE) {
                return visible_folder_id;
            }

            return parent_id;
        }

        return result_id;
    }

    void sql_entry_indexer::fill_entry_information(entry &the_entry, sqlite3_stmt *stmt, const bool have_extra_id) {
        the_entry.parent_id_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 0));
        the_entry.service_id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 1));
        the_entry.mtm_uid_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 2));
        the_entry.type_uid_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
        the_entry.time_ = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
        the_entry.data_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
        the_entry.size_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 6));
        the_entry.error_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 7));
        the_entry.mtm_datas_[0] = static_cast<std::int32_t>(sqlite3_column_int(stmt, 8));
        the_entry.mtm_datas_[1] = static_cast<std::int32_t>(sqlite3_column_int(stmt, 9));
        the_entry.mtm_datas_[2] = static_cast<std::int32_t>(sqlite3_column_int(stmt, 10));
        the_entry.related_id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 11));
        the_entry.bio_type_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 12));
        the_entry.pc_sync_count_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 13));
        the_entry.reserved_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 14));
        the_entry.visible_id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 15));
        the_entry.description_ = std::u16string(reinterpret_cast<const char16_t *>(sqlite3_column_text16(stmt, 16)));
        the_entry.details_ = std::u16string(reinterpret_cast<const char16_t *>(sqlite3_column_text16(stmt, 17)));

        if (have_extra_id) {
            the_entry.id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 18));
        }
    }

    entry *sql_entry_indexer::add_entry(const entry &ent) {
        LOG_TRACE(SERVICE_MSV, "Entry indexer add entry stubbed.");
        return nullptr;
    }

    void sql_entry_indexer::create_standard_entries(drive_number drv) {
        LOG_TRACE(SERVICE_MSV, "Entry indexer create standard entries stubbed.");
    }

    entry *sql_entry_indexer::get_entry(const std::uint32_t id) {
        entry *result = entry_indexer::get_entry(id);
        if (!result) {
            if (!find_entry_stmt_) {
                static const char *FIND_ENTRY_STR_STM = "SELECT parentId, serviceId, mtmId, type, date, data, size, error, mtmData1,"
                                                        "mtmData2, mtmData3, relatedId, bioType, pcSyncCount, reserved, visibleParent,"
                                                        "description, details from IndexEntry WHERE id=:id";

                if (sqlite3_prepare(database_, FIND_ENTRY_STR_STM, -1, &find_entry_stmt_, nullptr) != SQLITE_OK) {
                    LOG_ERROR(SERVICE_MSV, "Unable to prepare find entry statement!");
                    return nullptr;
                }
            }

            sqlite3_reset(find_entry_stmt_);
            if (sqlite3_bind_int(find_entry_stmt_, 1, id) != SQLITE_OK) {
                LOG_ERROR(SERVICE_MSV, "Unable to bind id to find entry in database!");
                return nullptr;
            }

            const int result_code = sqlite3_step(find_entry_stmt_);
            if (result_code == SQLITE_DONE) {
                return nullptr;
            }

            entry the_entry;
            the_entry.id_ = id;

            fill_entry_information(the_entry, find_entry_stmt_);

            return entry_indexer::add_entry(the_entry);
        }

        return result;
    }

    bool sql_entry_indexer::collect_children_entries(const msv_id parent_id, std::vector<entry> &entries) {
        if (!query_child_entries_stmt_) {
            static const char *QUERY_CHILD_ENTRIES_STM_STR = "SELECT parentId, serviceId, mtmId, type, date, data, size, error, mtmData1,"
                                                             "mtmData2, mtmData3, relatedId, bioType, pcSyncCount, reserved, visibleParent,"
                                                             "description, details, id from IndexEntry WHERE parentId=:parent_id";

            if (sqlite3_prepare(database_, QUERY_CHILD_ENTRIES_STM_STR, -1, &query_child_entries_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_MSV, "Can't prepare collect children IDs statement!");
                return false;
            }
        }

        sqlite3_reset(query_child_entries_stmt_);
        if (sqlite3_bind_int(query_child_entries_stmt_, 1, parent_id) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Can't bind parent id to query children ids statement!");
            return false;
        }

        do {
            int result = sqlite3_step(query_child_entries_stmt_);
            if (result == SQLITE_DONE) {
                return true;
            }

            if (result != SQLITE_ROW) {
                break;
            }

            if (sqlite3_column_count(query_child_entries_stmt_) != 19) {
                LOG_ERROR(SERVICE_MSV, "Query children entries statement is corrupted!");
                break;
            }

            entry an_entry;
            fill_entry_information(an_entry, query_child_entries_stmt_, true);

            entries.push_back(std::move(an_entry));
        } while (true);

        return false;
    }

    std::vector<entry *> sql_entry_indexer::get_entries_by_parent(const std::uint32_t parent_id) {
        visible_folder_children_query_error error = visible_folder_children_query_ok;
        std::vector<entry *> entries;

        msv_id visible_folder_id = get_suitable_visible_parent_id(parent_id);

        common::double_linked_queue_element *first = folders_.first();
        common::double_linked_queue_element *end = folders_.end();

        auto gather_visible_folder_entries = [&](visible_folder *ff) -> bool {
            std::vector<entry> queries;
            if (!collect_children_entries(visible_folder_id, queries)) {
                LOG_ERROR(SERVICE_MSV, "Unable to query visible folder children entries from database!");
                return false;
            }

            ff->add_entry_list(queries, true);
            return true;
        };

        auto gather_target_parent_entries = [&](visible_folder *ff) -> bool {
            std::vector<entry> queries;
            if (!collect_children_entries(parent_id, queries)) {
                LOG_ERROR(SERVICE_MSV, "Unable to query visible folder children entries from database!");
                return false;
            }

            entry *parent_entry = ff->get_entry(parent_id);
            if (!parent_entry) {
                LOG_ERROR(SERVICE_MSV, "Parent entry still doesn't exist!");
                return false;
            }

            for (std::size_t i = 0; i < queries.size(); i++) {
                parent_entry->children_ids_.push_back(queries[i].id_);
            }

            parent_entry->children_looked_up(true);
            ff->add_entry_list(queries);

            return true;
        };

        do {
            if (!first) {
                break;
            }

            visible_folder *ff = E_LOFF(first, visible_folder, indexer_link_);
            if (ff->id() == visible_folder_id) {
                entries = ff->get_children_by_parent(parent_id, &error);
                if (error == visible_folder_children_incomplete) {
                    gather_visible_folder_entries(ff);
                }

                if ((parent_id != visible_folder_id) && (error != visible_folder_children_query_ok)) {
                    gather_target_parent_entries(ff);
                }

                if (error != visible_folder_children_query_ok) {
                    entries = ff->get_children_by_parent(parent_id, &error);

                    if (error != visible_folder_children_query_ok) {
                        LOG_ERROR(SERVICE_MSV, "An error occured that made it unable to retrieve children entries");
                    }
                }

                return entries;
            }

            first = first->next;
        } while (first != end);

        visible_folder *new_folder = new visible_folder(visible_folder_id);
        gather_visible_folder_entries(new_folder);

        if (parent_id != visible_folder_id) {
            gather_target_parent_entries(new_folder);
        }

        folders_.push(&new_folder->indexer_link_);
        entries = new_folder->get_children_by_parent(parent_id, &error);

        if (error != visible_folder_children_query_ok) {
            LOG_ERROR(SERVICE_MSV, "An error occured that made it unable to retrieve children entries");
        }

        return entries;
    }
}