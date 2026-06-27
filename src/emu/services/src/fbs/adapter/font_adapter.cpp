#include <common/log.h>
#include <services/fbs/adapter/font_adapter.h>
#include <services/fbs/adapter/freetype_font_adapter.h>

namespace eka2l1::epoc::adapter {
    std::unique_ptr<font_file_adapter_base> make_font_file_adapter(const font_file_adapter_kind kind, std::vector<std::uint8_t> &dat) {
        switch (kind) {
        case font_file_adapter_kind::freetype: {
            return std::make_unique<freetype_font_adapter>(dat);
        }
        default: {
            break;
        }
        }
        return nullptr;
    }
}