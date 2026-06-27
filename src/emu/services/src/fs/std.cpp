#include <common/cvt.h>
#include <services/fs/std.h>
#include <vfs/vfs.h>

namespace eka2l1::epoc::fs {
    std::string get_server_name_through_epocver(const epocver ver) {
        if (ver < epocver::eka2) {
            return "FileServer";
        }
        return "!FileServer";
    }

    std::uint32_t build_attribute_from_entry_info(entry_info &info) {
        std::uint32_t attrib = epoc::fs::entry_att_normal;

        if (info.has_raw_attribute) {
            attrib = info.raw_attribute;
        } else {
            if (static_cast<int>(info.attribute) & static_cast<int>(io_attrib_write_protected)) {
                attrib |= epoc::fs::entry_att_read_only;
            }
            if (static_cast<int>(info.attribute) & static_cast<int>(io_attrib_hidden)) {
                attrib |= epoc::fs::entry_att_hidden;
            }

            switch (info.type) {
            case io_component_type::dir:
                attrib |= epoc::fs::entry_att_dir;
                break;
            case io_component_type::drive:
                attrib |= epoc::fs::entry_att_volume;
                break;
            case io_component_type::file:
                attrib |= epoc::fs::entry_att_archive;
                break;
            default:
                break;
            }
        }
        return attrib;
    }

    void build_symbian_entry_from_emulator_entry(io_system *io, entry_info &info, epoc::fs::entry &sym_entry) {
        sym_entry.size = static_cast<std::uint32_t>(info.size);
        sym_entry.size_high = static_cast<std::uint32_t>(info.size >> 32);

        const std::u16string name_ucs2 = common::utf8_to_ucs2(info.name);
        sym_entry.attrib = build_attribute_from_entry_info(info);
        sym_entry.name = name_ucs2;
        sym_entry.modified = epoc::time{ info.last_write };

        sym_entry.uid1 = sym_entry.uid2 = sym_entry.uid3 = 0xDEADBEEF;

        if (info.type == io_component_type::file) {
            const std::u16string fullpath_ucs2 = common::utf8_to_ucs2(info.full_path);
            symfile f = io->open_file(fullpath_ucs2, READ_MODE | BIN_MODE);
            if (f && f->valid()) {
                f->read_file(&sym_entry.uid1, sizeof(sym_entry.uid1), 1);
                f->read_file(&sym_entry.uid2, sizeof(sym_entry.uid2), 1);
                f->read_file(&sym_entry.uid3, sizeof(sym_entry.uid3), 1);
            }
        }
    }
}