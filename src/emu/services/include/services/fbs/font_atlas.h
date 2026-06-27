#pragma once

#include <common/vecx.h>
#include <drivers/graphics/common.h>
#include <services/fbs/adapter/font_adapter.h>
#include <services/window/common.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1::drivers {
    class graphics_driver;
    class graphics_command_builder;
}

namespace eka2l1::epoc {
#define ESTIMATE_MAX_CHAR_IN_ATLAS_WIDTH 50

    struct font_atlas {
        std::map<char16_t, adapter::character_info> characters_;
        drivers::handle atlas_handle_;
        adapter::font_file_adapter_base *adapter_;
        std::uint32_t metric_identifier_;
        int size_;

        std::vector<int> last_use_;

        std::pair<char16_t, char16_t> initial_range_;
        std::unique_ptr<std::uint8_t[]> atlas_data_;

        std::size_t typeface_idx_;
        std::int32_t pack_handle_;

    public:
        explicit font_atlas();

        explicit font_atlas(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
            const char16_t initial_char_count, const int font_size, const std::uint32_t metric_identifier_);

        void init(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
            const char16_t initial_char_count, const int font_size, const std::uint32_t metric_identifier_);

        void destroy(drivers::graphics_driver *driver);
    };
}