#pragma once

#include <common/uid.h>
#include <utils/consts.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc::msv {
    struct mtm_component {
        std::uint32_t group_idx_;

        std::u16string name_;
        std::u16string filename_;
        std::uint32_t comp_uid_;
        std::uint32_t specific_uid_;
        std::uint16_t entry_point_;
        std::uint16_t major_;
        std::uint16_t minor_;
        std::uint16_t build_;

        mtm_component *next_;
    };

    struct mtm_group {
        std::uint32_t mtm_uid_;
        std::uint32_t tech_type_uid_;

        std::uint8_t cap_avail_;
        std::uint8_t cap_send_;
        std::uint8_t cap_body_;

        mtm_component comps_;

        std::uint32_t ref_count_;
    };

    class mtm_registry {
        std::vector<mtm_group> groups_;
        std::map<std::uint32_t, std::vector<mtm_component *>> comps_;
        io_system *io_;

        std::u16string list_path_;
        std::vector<std::u16string> mtm_files_;

    protected:
        void add_entry_to_mtm_list(const std::u16string &path);
        bool install_group_from_rsc(const std::u16string &path);

    public:
        explicit mtm_registry(io_system *io);
        ~mtm_registry();

        void load_mtm_list();
        void save_mtm_list();

        bool install_group(const std::u16string &path);

        mtm_group *get_group(const std::uint32_t idx) {
            if (groups_.size() <= idx) {
                return nullptr;
            }
            return &groups_[idx];
        }

        void set_list_path(const std::u16string &list) {
            list_path_ = list;
        }
    };
}