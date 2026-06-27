#pragma once

#include <services/fbs/adapter/font_adapter.h>
#include <services/fbs/font.h>
#include <unordered_map>
#include <utils/consts.h>
#include <vector>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc {
    struct open_font_info {
        std::u16string family;
        std::size_t idx;
        epoc::adapter::font_file_adapter_base *adapter;
        epoc::open_font_face_attrib face_attrib;
        epoc::open_font_metrics metrics;
        std::uint32_t metric_identifier;
    };

    class font_store {
        std::vector<open_font_info> open_font_store;
        std::vector<epoc::adapter::font_file_adapter_instance> font_adapters;
        std::vector<epoc::typeface_support> typefaces;

        eka2l1::io_system *io;

    public:
        explicit font_store(eka2l1::io_system *io)
            : io(io) {
        }

        void add_fonts(std::vector<std::uint8_t> &buf, const epoc::adapter::font_file_adapter_kind adapter_kind);
        open_font_info *seek_the_open_font(epoc::font_spec_base &spec);
        epoc::typeface_support *get_typeface_support(const std::uint32_t index);

        const std::size_t number_of_fonts() const {
            return open_font_store.size();
        }

        const std::size_t number_of_typefaces() const {
            return typefaces.size();
        }
    };
}