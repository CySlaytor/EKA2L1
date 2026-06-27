#pragma once

#include <memory>
#include <services/window/classes/plugins/animdll.h>
#include <string>
#include <vector>

namespace eka2l1::epoc {
    using anim_dll_filename_and_factory_list = std::vector<std::pair<std::u16string, std::unique_ptr<anim_executor_factory>>>;
    void add_anim_executor_factory_to_list(anim_dll_filename_and_factory_list &factories);
}