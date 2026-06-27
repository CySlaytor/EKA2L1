#pragma once

#include <optional>
#include <services/ui/skin/common.h>
#include <string>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc {
    std::optional<std::u16string> find_skin_file(eka2l1::io_system *io, const epoc::pid skin_pid);
    std::optional<epoc::pid> pick_first_skin(eka2l1::io_system *io);
    std::optional<std::u16string> get_resource_path_of_skin(eka2l1::io_system *io, const epoc::pid skin_pid);
    std::u16string pid_to_string(const epoc::pid skin_pid);
}