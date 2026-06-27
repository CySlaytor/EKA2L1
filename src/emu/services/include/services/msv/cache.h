#pragma once

#include <services/msv/common.h>
#include <vector>

#include <common/linked.h>

namespace eka2l1::epoc::msv {
    static constexpr std::uint32_t CACHE_THRESHOLD = 120;

    struct entry_range_table {
    private:
        msv_id min_;
        msv_id max_;
        std::uint32_t flags_;
        std::vector<entry> entries_;
        common::double_linked_queue_element folder_link_;

        enum { FLAG_GRAND_CHILD_PRESENT = 1 << 0 };

        friend struct visible_folder;

    public:
        explicit entry_range_table();
        bool add_tons(std::vector<entry> &ents, const std::size_t start_index, const std::size_t end_index);
        entry *get(const msv_id id);

        void min_range(msv_id min) { min_ = min; }
        void max_range(msv_id max) { max_ = max; }
        const msv_id min_range() const { return min_; }
        const msv_id max_range() const { return max_; }
        const bool in_range(const msv_id id) const { return (id >= min_) && (id <= max_); }

        bool grand_child_present() const { return flags_ & FLAG_GRAND_CHILD_PRESENT; }
        void grand_child_present(const bool value) {
            flags_ &= ~FLAG_GRAND_CHILD_PRESENT;
            if (value) {
                flags_ |= FLAG_GRAND_CHILD_PRESENT;
            }
        }
    };

    enum visible_folder_children_query_error {
        visible_folder_children_query_ok = 0,
        visible_folder_children_query_no_child_id_array = 1,
        visible_folder_children_incomplete = 2
    };

    struct visible_folder {
    private:
        common::roundabout tables_;
        std::uint32_t flags_;
        msv_id myid_;

        enum { FLAG_ALL_CHILDREN_PRESENT = 1 << 0 };
        void clear_tables();

    public:
        common::double_linked_queue_element indexer_link_;

        explicit visible_folder(const msv_id my_id);
        ~visible_folder();

        bool add_entry_list(std::vector<entry> &ent, const bool complete_child_of_folder = false);
        entry *get_entry(const msv_id id);
        std::vector<entry *> get_children_by_parent(const msv_id parent_id, visible_folder_children_query_error *error = nullptr);

        const msv_id id() const { return myid_; }

        bool all_children_present() const { return flags_ & FLAG_ALL_CHILDREN_PRESENT; }
        void all_children_present(const bool value) {
            flags_ &= ~FLAG_ALL_CHILDREN_PRESENT;
            if (value) {
                flags_ |= FLAG_ALL_CHILDREN_PRESENT;
            }
        }
    };
}