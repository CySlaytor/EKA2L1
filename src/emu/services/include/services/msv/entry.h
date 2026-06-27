#pragma once

#include <services/msv/cache.h>
#include <services/msv/common.h>

#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <string>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc::msv {
    struct entry_indexer {
    protected:
        eka2l1::io_system *io_;
        drive_number rom_drv_;
        int preferred_lang_;
        std::u16string msg_dir_;

        common::roundabout folders_;
        entry root_entry_;

    public:
        explicit entry_indexer(eka2l1::io_system *io, const std::u16string &msg_folder, int preferred_lang);
        virtual ~entry_indexer();

        virtual entry *get_entry(const std::uint32_t id) { return nullptr; }
        virtual bool owning_service(const std::uint32_t id, std::uint32_t &owning);

        std::optional<std::u16string> get_entry_data_directory(const std::uint32_t id, const std::uint32_t type,
            const std::uint32_t owning_service);

        std::optional<std::u16string> get_entry_data_file(entry *ent);

        virtual entry *add_entry(const entry &ent) { return nullptr; }
        virtual void create_standard_entries(drive_number drv) {}
        virtual std::vector<entry *> get_entries_by_parent(const std::uint32_t parent_id) { return {}; }
    };

    struct sql_entry_indexer : public entry_indexer {
    private:
        sqlite3 *database_;
        sqlite3_stmt *create_entry_stmt_;
        sqlite3_stmt *change_entry_stmt_;
        sqlite3_stmt *relocate_entry_stmt_;
        sqlite3_stmt *visible_folder_find_stmt_;
        sqlite3_stmt *find_entry_stmt_;
        sqlite3_stmt *query_child_entries_stmt_;
        sqlite3_stmt *query_child_ids_stmt_;

        std::uint32_t id_counter_;

        bool load_or_create_databases(bool &newly_created);
        bool collect_children_entries(const msv_id parent_id, std::vector<entry> &entries);
        void fill_entry_information(entry &ent, sqlite3_stmt *stmt, const bool have_extra_id = false);

        msv_id get_suitable_visible_parent_id(const msv_id parent_id);

    public:
        explicit sql_entry_indexer(eka2l1::io_system *io, const std::u16string &msg_folder, int preferred_lang);
        ~sql_entry_indexer() override;

        entry *get_entry(const std::uint32_t id) override;
        std::vector<entry *> get_entries_by_parent(const std::uint32_t parent_id) override;

        entry *add_entry(const entry &ent) override;
        void create_standard_entries(drive_number drv) override;
    };
}