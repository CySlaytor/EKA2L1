#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/bytepair.h>
#include <common/bytes.h>
#include <common/crypt.h>
#include <common/cvt.h>
#include <common/flate.h>
#include <common/log.h>
#include <loader/e32img.h>
#include <utils/err.h>

#include <cstdio>
#include <miniz.h>
#include <sstream>

namespace eka2l1::loader {
    struct e32img_import_sec_header {
        uint32_t size;
        uint32_t num_reloc;
    };

    enum compress_type {
        deflate_c = 0x101F7AFC // Removed byte_pair_c as we don't support EKA1
    };

    static void read_relocations(common::ro_stream *stream, e32_reloc_section &section, uint32_t offset) {
        if (offset == 0)
            return;

        stream->seek(offset, common::beg);
        stream->read(reinterpret_cast<void *>(&section.size), 4);
        stream->read(reinterpret_cast<void *>(&section.num_relocs), 4);

        if (section.size <= 8) {
            section.num_relocs = 0;
            return;
        }

        for (uint32_t i = 0; i < section.num_relocs; i++) {
            e32_reloc_entry reloc_entry;
            stream->read(reinterpret_cast<void *>(&reloc_entry.base), 4);
            stream->read(reinterpret_cast<void *>(&reloc_entry.size), 4);

            assert((reloc_entry.size) % 2 == 0);

            reloc_entry.rels_info.resize(((reloc_entry.size - 8) / 2));
            for (auto &rel_info : reloc_entry.rels_info) {
                stream->read(reinterpret_cast<void *>(&rel_info), 2);
            }

            i += static_cast<int>(reloc_entry.rels_info.size()) - 1;
            (!reloc_entry.rels_info.empty()) && (reloc_entry.rels_info.back() == 0) ? (i -= 1) : 0;
            section.entries.push_back(reloc_entry);
        }
    }

    static void parse_export_dir(e32img &img) {
        if (img.header.export_dir_offset == 0)
            return;

        uint32_t *exp = reinterpret_cast<uint32_t *>(img.data.data() + img.header.export_dir_offset);
        for (std::uint32_t i = 0; i < img.header.export_dir_count; i++) {
            img.ed.syms.push_back(*exp++);
        }
    }

    static void parse_iat(e32img &img) {
        uint32_t *imp_addr = reinterpret_cast<uint32_t *>(img.data.data() + img.header.code_offset + img.header.text_size);
        while (*imp_addr != 0) {
            img.iat.its.push_back(*imp_addr++);
        }
    }

    static constexpr std::uint32_t E32IMG_SIGNATURE = 0x434F5045;

    bool is_e32img(common::ro_stream *stream, std::uint32_t *uid_array) {
        std::uint32_t temp_arr[3];
        if (!uid_array)
            uid_array = temp_arr;

        std::uint32_t check = 0;
        std::uint32_t sig = 0;

        stream->read(&uid_array[0], 4);
        stream->read(&uid_array[1], 4);
        stream->read(&uid_array[2], 4);
        stream->read(&check, 4);
        stream->read(&sig, 4);

        const bool result = (sig == E32IMG_SIGNATURE);
        stream->seek(0, common::seek_where::beg);

        return result;
    }

    std::int32_t parse_e32img_header(common::ro_stream *stream, e32img_header &header, e32img_header_extended &extended,
        std::uint32_t &uncompressed_size, epocver &ver) {
        if (!stream)
            return epoc::error_argument;

        stream->seek(0, common::seek_where::beg);
        stream->read(&header.uid1, 4);
        stream->read(&header.uid2, 4);
        stream->read(&header.uid3, 4);
        stream->read(&header.check, 4);
        stream->read(&header.sig, 4);

        if (header.sig != E32IMG_SIGNATURE)
            return epoc::error_corrupt;

        std::uint32_t uid_type[3] = { static_cast<std::uint32_t>(header.uid1), header.uid2, header.uid3 };
        if (crypt::calculate_checked_uid_checksum(uid_type) != header.check)
            return epoc::error_corrupt;

        std::uint32_t temp = 0;
        stream->read(&temp, 4);

        // Filter out EKA1 images entirely
        if ((temp == 0x2000) || (temp == 0x1000)) {
            LOG_ERROR(LOADER, "EKA1 images are not supported in this dedicated OS 9.3 build. Ignoring file.");
            return epoc::error_not_supported;
        }

        // Process EKA2
        ver = epocver::epoc93fp2; // Hardcode target
        stream->seek(0, common::seek_where::beg);
        stream->read(&header, sizeof(e32img_header));

        if (common::get_system_endian_type() == common::big_endian) {
            header.uid1 = static_cast<e32_img_type>(common::byte_swap(static_cast<std::uint32_t>(header.uid1)));
            header.uid2 = common::byte_swap(header.uid2);
            header.uid3 = common::byte_swap(header.uid3);
            header.check = common::byte_swap(header.check);
            header.sig = common::byte_swap(header.sig);
            header.petran_build = common::byte_swap(header.petran_build);
            header.flags = common::byte_swap(header.flags);
            header.code_size = common::byte_swap(header.code_size);
            header.data_size = common::byte_swap(header.data_size);
            header.heap_size_min = common::byte_swap(header.heap_size_min);
            header.stack_size = common::byte_swap(header.stack_size);
            header.bss_size = common::byte_swap(header.bss_size);
            header.entry_point = common::byte_swap(header.entry_point);
            header.code_base = common::byte_swap(header.code_base);
            header.data_base = common::byte_swap(header.data_base);
            header.dll_ref_table_count = common::byte_swap(header.dll_ref_table_count);
            header.export_dir_offset = common::byte_swap(header.export_dir_offset);
            header.export_dir_count = common::byte_swap(header.export_dir_count);
            header.text_size = common::byte_swap(header.text_size);
            header.code_offset = common::byte_swap(header.code_offset);
            header.data_offset = common::byte_swap(header.data_offset);
            header.code_reloc_offset = common::byte_swap(header.code_reloc_offset);
            header.data_reloc_offset = common::byte_swap(header.data_reloc_offset);
            header.priority = common::byte_swap(header.priority);
        }

        int header_format = (static_cast<int>(header.flags) >> 24) & 0xF;

        if (header_format > 0) {
            stream->read(&uncompressed_size, 4);

            if (header_format == 2) {
                stream->read(&extended.info, sizeof(e32img_vsec_info));
                stream->read(&extended.exception_des, 4);
                stream->read(&extended.spare2, 4);
                stream->read(&extended.export_desc_size, 2);
                stream->read(&extended.export_desc_type, 1);
                stream->read(&extended.export_desc, 1);

                if (common::get_system_endian_type() == common::big_endian) {
                    extended.exception_des = common::byte_swap(extended.exception_des);
                    extended.spare2 = common::byte_swap(extended.spare2);
                    extended.export_desc_size = common::byte_swap(extended.export_desc_size);
                    extended.info.cap1 = common::byte_swap(extended.info.cap1);
                    extended.info.cap2 = common::byte_swap(extended.info.cap2);
                    extended.info.secure_id = common::byte_swap(extended.info.secure_id);
                    extended.info.vendor_id = common::byte_swap(extended.info.vendor_id);
                }
            }

            if (common::get_system_endian_type() == common::big_endian) {
                uncompressed_size = common::byte_swap(uncompressed_size);
            }
        }

        return epoc::error_none;
    }

    std::optional<e32img> parse_e32img(common::ro_stream *stream, bool read_reloc) {
        if (!stream)
            return std::nullopt;

        e32img img;
        const std::size_t file_size = stream->size();

        if (parse_e32img_header(stream, img.header, img.header_extended, img.uncompressed_size, img.epoc_ver) != epoc::error_none) {
            return std::nullopt;
        }

        int header_format = (static_cast<int>(img.header.flags) >> 24) & 0xF;
        if (header_format > 0)
            img.has_extended_header = true;

        compress_type ctype = static_cast<compress_type>(img.header.compression_type);

        if (img.header.compression_type > 0) {
            if (ctype != compress_type::deflate_c) {
                LOG_ERROR(LOADER, "Only deflate_c is supported in this build. Rejecting payload.");
                return std::nullopt;
            }

            img.data.resize(img.uncompressed_size + img.header.code_offset);
            std::uint32_t start_compress = img.header.code_offset;

            stream->seek(0, common::seek_where::beg);
            stream->read(img.data.data(), img.header.code_offset);

            std::vector<char> temp_buf(file_size - start_compress);
            stream->seek(start_compress, common::seek_where::beg);
            size_t bytes_read = stream->read(temp_buf.data(), static_cast<uint32_t>(temp_buf.size()));

            if (bytes_read != temp_buf.size()) {
                LOG_ERROR(LOADER, "File reading improperly");
            }

            flate::bit_input input(reinterpret_cast<uint8_t *>(temp_buf.data()), static_cast<int>(temp_buf.size() * 8));
            flate::inflater inflate_machine(input);
            inflate_machine.init();
            inflate_machine.read(reinterpret_cast<uint8_t *>(&img.data[img.header.code_offset]), img.uncompressed_size);
        } else {
            img.uncompressed_size = static_cast<uint32_t>(file_size);
            img.data.resize(file_size);
            stream->seek(0, common::seek_where::beg);
            stream->read(img.data.data(), static_cast<uint32_t>(img.data.size()));
        }

        parse_export_dir(img);
        parse_iat(img);

        common::ro_buf_stream decompressed_stream(reinterpret_cast<std::uint8_t *>(&img.data[0]), img.data.size());

        decompressed_stream.seek(img.header.import_offset, common::seek_where::beg);
        decompressed_stream.read(reinterpret_cast<void *>(&img.import_section.size), 4);
        img.import_section.imports.resize(img.header.dll_ref_table_count);

        for (auto &import : img.import_section.imports) {
            decompressed_stream.read(reinterpret_cast<void *>(&import.dll_name_offset), 4);
            decompressed_stream.read(reinterpret_cast<void *>(&import.number_of_imports), 4);

            img.dll_names.push_back(import.dll_name);

            if (import.number_of_imports == 0)
                continue;

            const auto crr_size = decompressed_stream.tell();
            decompressed_stream.seek(img.header.import_offset + import.dll_name_offset, common::beg);

            char temp = 1;
            while (temp != 0) {
                decompressed_stream.read(&temp, 1);
                if (temp != 0)
                    import.dll_name += temp;
            }

            decompressed_stream.seek(static_cast<uint32_t>(crr_size), common::beg);
            import.ordinals.resize(import.number_of_imports);

            for (auto &oridinal : import.ordinals) {
                decompressed_stream.read(reinterpret_cast<void *>(&oridinal), 4);
            }
        }

        if (read_reloc) {
            read_relocations(reinterpret_cast<common::ro_stream *>(&decompressed_stream), img.code_reloc_section, img.header.code_reloc_offset);
            read_relocations(reinterpret_cast<common::ro_stream *>(&decompressed_stream), img.data_reloc_section, img.header.data_reloc_offset);
        }

        return img;
    }
}