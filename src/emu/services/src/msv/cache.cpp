#include <algorithm>
#include <services/msv/cache.h>

namespace eka2l1::epoc::msv {
    entry_range_table::entry_range_table()
        : min_(0)
        , max_(0)
        , flags_(0) {
    }

    bool entry_range_table::add_tons(std::vector<entry> &ents, const std::size_t start_index, const std::size_t end_index) {
        if (ents.empty() || (start_index > end_index) || (end_index >= ents.size())) {
            return true;
        }

        bool no_need_look_too_much = entries_.empty();

        if (!no_need_look_too_much) {
            for (std::size_t i = start_index; i <= end_index; i++) {
                if (!std::binary_search(entries_.begin(), entries_.end(), ents[i], [](const entry &lhs, const entry &rhs) {
                        return lhs.id_ < rhs.id_;
                    })) {
                    entries_.push_back(ents[i]);
                }
            }
        } else {
            entries_.insert(entries_.begin(), ents.begin() + start_index, ents.begin() + end_index + 1);
        }

        std::sort(entries_.begin(), entries_.end(), [](const entry &lhs, const entry &rhs) {
            return lhs.id_ < rhs.id_;
        });

        min_ = entries_.front().id_;
        max_ = entries_.back().id_;

        return true;
    }

    entry *entry_range_table::get(const msv_id id) {
        if ((id < min_) || (id > max_)) {
            return nullptr;
        }

        auto ite = std::lower_bound(entries_.begin(), entries_.end(), id, [](const entry &ent, const msv_id target) {
            return ent.id_ < target;
        });

        if ((ite == entries_.end()) || (ite->id_ != id)) {
            return nullptr;
        }

        return &(*ite);
    }

    visible_folder::visible_folder(const msv_id my_id)
        : flags_(0)
        , myid_(my_id) {
    }

    visible_folder::~visible_folder() {
        clear_tables();
    }

    void visible_folder::clear_tables() {
        while (!tables_.empty()) {
            entry_range_table *table = E_LOFF(tables_.first()->deque(), entry_range_table, folder_link_);
            delete table;
        }
    }

    bool visible_folder::add_entry_list(std::vector<entry> &entries, const bool complete_child_of_folder) {
        if (entries.empty()) {
            if (complete_child_of_folder) {
                all_children_present(true);
            }

            return true;
        }

        auto make_tables_and_add = [&](const std::size_t starting_index, const std::size_t size) {
            for (std::size_t i = 0; i < (size + CACHE_THRESHOLD - 1) / CACHE_THRESHOLD; i++) {
                entry_range_table *table = new entry_range_table;
                table->add_tons(entries, starting_index + i * CACHE_THRESHOLD, common::min<std::size_t>(entries.size() - 1, starting_index + (i + 1) * CACHE_THRESHOLD - 1));

                if (entries[i * CACHE_THRESHOLD].parent_id_ != myid_) {
                    table->grand_child_present(true);
                }

                tables_.push(&table->folder_link_);
            }
        };

        std::sort(entries.begin(), entries.end(), [](const entry &lhs, const entry &rhs) {
            return lhs.id_ < rhs.id_;
        });

        if (tables_.empty()) {
            make_tables_and_add(0, entries.size());
            if (complete_child_of_folder) {
                all_children_present(true);
            }

            return true;
        }

        std::size_t skipped = 0;
        std::size_t last_skip = 0;

        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            last_skip = skipped;

            bool grand_child_incoming = false;

            while (the_table && (skipped < entries.size()) && (entries[skipped].id_ < the_table->max_range())) {
                if (entries[skipped].parent_id_ != myid_) {
                    grand_child_incoming = true;
                }
                skipped++;
            }

            if (grand_child_incoming) {
                the_table->grand_child_present(true);
            }

            if (skipped > last_skip)
                the_table->add_tons(entries, last_skip, skipped - 1);

            if (first->next == end) {
                if (skipped < entries.size()) {
                    make_tables_and_add(skipped, entries.size() - skipped);
                    break;
                }
            }

            first = first->next;
        } while (first != end);

        if (complete_child_of_folder) {
            all_children_present(true);
        }

        return true;
    }

    entry *visible_folder::get_entry(const msv_id id) {
        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            if (!first) {
                break;
            }

            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            entry *result = the_table->get(id);

            if (result) {
                return result;
            }

            first = first->next;
        } while (first != end);

        return nullptr;
    }

    std::vector<entry *> visible_folder::get_children_by_parent(const msv_id parent_id, visible_folder_children_query_error *error) {
        std::vector<entry *> ents;

        if (error) {
            *error = visible_folder_children_query_ok;
        }

        if (parent_id == myid_) {
            if (!all_children_present()) {
                if (error) {
                    *error = visible_folder_children_incomplete;
                }

                return ents;
            }

            common::double_linked_queue_element *first = tables_.first();
            common::double_linked_queue_element *end = tables_.end();

            do {
                if (!first) {
                    break;
                }

                entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
                if (the_table->grand_child_present()) {
                    for (std::size_t i = 0; i < the_table->entries_.size(); i++) {
                        if (the_table->entries_[i].parent_id_ == myid_) {
                            ents.push_back(&the_table->entries_[i]);
                        }
                    }
                } else {
                    for (std::size_t i = 0; i < the_table->entries_.size(); i++) {
                        ents.push_back(&the_table->entries_[i]);
                    }
                }

                first = first->next;
            } while (first != end);

            return ents;
        }

        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            if (!first)
                break;
            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            if (the_table->in_range(parent_id)) {
                entry *ent = the_table->get(parent_id);
                if (!ent || !ent->children_looked_up()) {
                    if (error) {
                        *error = visible_folder_children_query_no_child_id_array;
                    }

                    return ents;
                }

                for (std::size_t i = 0; i < ent->children_ids_.size(); i++) {
                    ents.push_back(get_entry(ent->children_ids_[i]));
                }
            }

            first = first->next;
        } while (first != end);

        return ents;
    }
}