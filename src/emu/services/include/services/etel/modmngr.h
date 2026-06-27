#pragma once

#include <services/etel/common.h>

#include <optional>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;
    class kernel_system;

    namespace kernel {
        using uid = std::uint64_t;
    }
}

namespace eka2l1::epoc::etel {
    class module_manager {
        struct tsy_module_info {
            std::string name_;
            std::vector<kernel::uid> used_sessions_;
        };

        std::vector<tsy_module_info> loaded_;
        std::vector<etel_module_entry> entries_;

    public:
        explicit module_manager();

        bool load_tsy(kernel_system *kern, io_system *io, const kernel::uid borrowed_session, const std::string &module_name);
        bool close_tsy(io_system *io, const kernel::uid borrowed_session, const std::string &module_name);

        void unload_from_sessions(io_system *io, const kernel::uid borrowed_session);

        std::optional<std::uint32_t> get_entry_real_index(const std::uint32_t respective_index, const etel_entry_type type);

        bool get_entry(const std::uint32_t real_index, etel_module_entry **entry);
        bool get_entry_by_name(const std::string &name, etel_module_entry **entry);

        std::size_t total_entries(const etel_entry_type type) const;
    };
}