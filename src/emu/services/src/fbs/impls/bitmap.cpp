#include <algorithm>
#include <cassert>
#include <common/algorithm.h>
#include <common/bitmap.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/runlen.h>
#include <kernel/kernel.h>
#include <services/fbs/fbs.h>
#include <services/fbs/palette.h>
#include <services/fs/fs.h>
#include <services/window/common.h>
#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    static epoc::bitmap_color get_bitmap_color_type_from_display_mode(const epoc::display_mode bpp) {
        switch (bpp) {
        case epoc::display_mode::gray2:
            return epoc::monochrome_bitmap;
        case epoc::display_mode::color16ma:
        case epoc::display_mode::color16map:
            return epoc::color_bitmap_with_alpha;
        default:
            break;
        }
        return epoc::color_bitmap;
    }

    fbs_bitmap_data_info::fbs_bitmap_data_info()
        : dpm_(epoc::display_mode::none)
        , comp_(epoc::bitmap_file_compression::bitmap_file_no_compression)
        , data_(nullptr)
        , data_size_(0) {
    }

    namespace epoc {
        constexpr std::uint32_t BITWISE_BITMAP_UID = 0x10000040;
        constexpr std::uint32_t MAGIC_FBS_PILE_PTR = 0xDEADFB55;
        constexpr std::uint32_t MAGIC_FBS_HEAP_PTR = 0xDEADFB88;

        display_mode bitwise_bitmap::settings::initial_display_mode() const {
            return static_cast<display_mode>(flags_ & 0x000000FF);
        }

        display_mode bitwise_bitmap::settings::current_display_mode() const {
            return static_cast<display_mode>((flags_ & 0x0000FF00) >> 8);
        }

        void bitwise_bitmap::settings::current_display_mode(const display_mode &mode) {
            flags_ &= 0xFFFF00FF;
            flags_ |= (static_cast<std::uint32_t>(mode) << 8);
        }

        void bitwise_bitmap::settings::initial_display_mode(const display_mode &mode) {
            flags_ &= 0xFFFFFF00;
            flags_ |= static_cast<std::uint32_t>(mode);
        }

        bool bitwise_bitmap::settings::dirty_bitmap() const {
            return flags_ & settings_flag::dirty_bitmap;
        }

        void bitwise_bitmap::settings::dirty_bitmap(const bool is_it) {
            if (is_it)
                flags_ |= settings_flag::dirty_bitmap;
            else
                flags_ &= ~(settings_flag::dirty_bitmap);
        }

        bool bitwise_bitmap::settings::violate_bitmap() const {
            return flags_ & settings_flag::violate_bitmap;
        }

        void bitwise_bitmap::settings::violate_bitmap(const bool is_it) {
            if (is_it)
                flags_ |= settings_flag::violate_bitmap;
            else
                flags_ &= ~(settings_flag::violate_bitmap);
        }

        bool bitwise_bitmap::settings::is_large() const {
            return flags_ & settings_flag::large_bitmap;
        }

        void bitwise_bitmap::settings::set_large(const bool result) {
            if (result)
                flags_ |= settings_flag::large_bitmap;
            else
                flags_ &= ~(settings_flag::large_bitmap);
        }

        static void do_white_fill(std::uint8_t *dest, const std::size_t size, epoc::display_mode mode) {
            std::fill(dest, dest + size, 0xFF);
        }

        void bitwise_bitmap::construct(loader::sbm_header &info, epoc::display_mode disp_mode, void *data, const void *base, const bool support_current_display_mode_flag, const bool white_fill) {
            uid_ = epoc::BITWISE_BITMAP_UID;
            allocator_ = MAGIC_FBS_HEAP_PTR;
            pile_ = MAGIC_FBS_PILE_PTR;
            byte_width_ = 0;
            header_ = info;
            spare1_ = 0;
            data_offset_ = 0xFFFFFF;
            offset_from_me_ = false;

            if (info.compression != epoc::bitmap_file_no_compression) {
                compressed_in_ram_ = true;
            } else {
                compressed_in_ram_ = false;
            }

            if (data) {
                if (data < base) {
                    data_offset_ = static_cast<int>(reinterpret_cast<const std::uint8_t *>(base) - reinterpret_cast<const std::uint8_t *>(data));
                    data_offset_ *= -1;
                } else {
                    data_offset_ = static_cast<int>(reinterpret_cast<const std::uint8_t *>(data) - reinterpret_cast<const std::uint8_t *>(base));
                }
            } else {
                data_offset_ = 0;
            }

            settings_.initial_display_mode(disp_mode);

            if (support_current_display_mode_flag)
                settings_.current_display_mode(disp_mode);

            byte_width_ = get_byte_width(info.size_pixels.width(), static_cast<std::uint8_t>(info.bit_per_pixels));

            if (white_fill && (data_offset_ != 0)) {
                do_white_fill(reinterpret_cast<std::uint8_t *>(data), info.bitmap_size - sizeof(loader::sbm_header), settings_.current_display_mode());
            }
        }

        void bitwise_bitmap::post_construct(fbs_server *serv) {
            if (serv->legacy_level() >= FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION) {
                if ((header_.compression == epoc::bitmap_file_byte_rle_compression) || (header_.compression == epoc::bitmap_file_twelve_bit_rle_compression))
                    header_.compression += epoc::LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE;
            }

            if (serv->legacy_level() == FBS_LEGACY_LEVEL_EARLY_EKA2) {
                settings_.set_large(offset_from_me_ ? false : true);
            } else if (serv->legacy_level() == FBS_LEGACY_LEVEL_SYMBIAN_92) {
                if (header_.compression == 0) {
                    settings_.set_large(true);
                } else {
                    settings_.set_large(((((header_.bitmap_size - header_.header_len) + 3) >> 2) << 2) >= 4096);
                }
                compressed_in_ram_ = (header_.compression != epoc::bitmap_file_no_compression);
            }
        }

        struct bitmap_copy_writer : public common::wo_stream {
            std::uint8_t *dest_;
            std::uint32_t source_byte_width_;
            std::uint32_t dest_byte_width_;
            int max_line_;
            eka2l1::vec2 pos_;

            explicit bitmap_copy_writer(std::uint8_t *dest, const std::uint32_t source_bw, const std::uint32_t dest_bw,
                const int max_line)
                : dest_(dest)
                , source_byte_width_(source_bw)
                , dest_byte_width_(dest_bw)
                , max_line_(max_line) {
            }

            void seek(const std::int64_t amount, common::seek_where wh) override { assert(false); }
            bool valid() override { return (max_line_ < pos_.y); }
            std::uint64_t tell() override { return dest_byte_width_ * pos_.y + pos_.x; }
            std::uint64_t left() override { return max_line_ * dest_byte_width_ - tell(); }
            std::uint64_t size() override { return max_line_ * dest_byte_width_; }

            std::uint64_t write(const void *buf, const std::uint64_t write_size) override {
                std::uint64_t consumed = 0;
                const std::uint8_t *buf8 = reinterpret_cast<const std::uint8_t *>(buf);
                const std::uint32_t width_write = common::min<std::uint32_t>(source_byte_width_, dest_byte_width_);

                while (consumed < write_size) {
                    if (pos_.y >= max_line_)
                        break;
                    const std::int64_t write_this_line = common::min<std::int64_t>(static_cast<std::int64_t>(write_size - consumed),
                        width_write - pos_.x);

                    std::memcpy(dest_ + pos_.y * dest_byte_width_ + pos_.x, buf8 + consumed, write_this_line);
                    pos_.x += static_cast<int>(write_this_line);
                    if (pos_.x == width_write) {
                        pos_.x = 0;
                        pos_.y++;
                    }
                    consumed += write_this_line;
                }
                return consumed;
            }
        };

        int bitwise_bitmap::copy_to(std::uint8_t *dest, const eka2l1::vec2 &dest_size, fbs_server *serv) {
            const int min_pixel_height = common::min(header_.size_pixels.height(), dest_size.y);
            const int dest_byte_width = get_byte_width(dest_size.x, header_.bit_per_pixels);
            std::uint8_t *src_base = data_pointer(serv);
            std::uint8_t *dest_base = dest;
            std::uint8_t *src = src_base;
            const int min_byte_width = std::min(byte_width_, dest_byte_width);

            if (compressed_in_ram_ && (compression_type() != bitmap_file_no_compression)) {
                bitmap_copy_writer writer(dest, byte_width_, dest_byte_width, min_pixel_height);
                common::ro_buf_stream source_stream(src, data_size());

                switch (compression_type()) {
                case bitmap_file_byte_rle_compression:
                    decompress_rle<8>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;
                case bitmap_file_twelve_bit_rle_compression:
                    decompress_rle<12>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;
                case bitmap_file_sixteen_bit_rle_compression:
                    decompress_rle<16>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;
                case bitmap_file_twenty_four_bit_rle_compression:
                    decompress_rle<24>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;
                case bitmap_file_thirty_two_a_bit_rle_compression:
                case bitmap_file_thirty_two_u_bit_rle_compression:
                    decompress_rle<32>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;
                default:
                    LOG_ERROR(SERVICE_FBS, "Unknown compression type {} to get original data for resize", static_cast<int>(compression_type()));
                    break;
                }
            } else {
                for (int row = 0; row < min_pixel_height; row++) {
                    std::memcpy(dest, src, min_byte_width);
                    dest += dest_byte_width;
                    src += byte_width_;
                }
            }

            if (dest_byte_width > byte_width_) {
                const int extra_bits = (header_.size_pixels.width() * header_.bit_per_pixels) & 31;
                if (extra_bits > 0) {
                    uint32_t mask = 0xFFFFFFFF;
                    mask <<= extra_bits;
                    const int dest_word_width = dest_byte_width >> 2;
                    const int src_word_width = byte_width_ >> 2;
                    uint32_t *mask_addr = reinterpret_cast<uint32_t *>(dest_base) + src_word_width - 1;
                    for (int row = 0; row < min_pixel_height; row++) {
                        *mask_addr |= mask;
                        mask_addr += dest_word_width;
                    }
                }
            }
            return epoc::error_none;
        }

        bitmap_file_compression bitwise_bitmap::compression_type() const {
            if (header_.compression >= epoc::LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE) {
                return static_cast<bitmap_file_compression>(header_.compression - epoc::LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE);
            }
            return static_cast<bitmap_file_compression>(header_.compression);
        }

        std::uint8_t *bitwise_bitmap::data_pointer(fbs_server *ss) {
            return reinterpret_cast<std::uint8_t *>(this) + data_offset_;
        }

        std::uint32_t bitwise_bitmap::data_size() const {
            return header_.bitmap_size - header_.header_len;
        }
    }

    struct load_bitmap_arg {
        std::uint32_t bitmap_id;
        std::int32_t share;
        std::int32_t file_offset;
    };

    struct bmp_handles {
        std::int32_t handle;
        std::int32_t server_handle;
        std::int32_t address_offset;
    };

    struct bmp_specs {
        eka2l1::vec2 size;
        epoc::display_mode bpp;
        std::uint32_t handle;
        std::uint32_t server_handle;
        std::uint32_t address_offset;
    };

    struct bmp_specs_v2 {
        eka2l1::vec2 size_pixels;
        eka2l1::vec2 size_twips;
        epoc::display_mode bpp;
        std::uint32_t bitmap_uid;
        std::uint32_t data_size_force;
    };

    struct bmp_specs_legacy {
        eka2l1::vec2 size;
        epoc::display_mode bpp;
        std::uint32_t padding[130];
        std::uint32_t handle;
        std::uint32_t server_handle;
        std::uint32_t address_offset;
    };

    fbsbitmap::~fbsbitmap() {
        if (serv_)
            serv_->free_bitmap(this);
        if (clean_bitmap) {
            clean_bitmap->deref();
        }
    }

    fbsbitmap *fbsbitmap::final_clean() {
        fbsbitmap *start = this;
        while (start->clean_bitmap) {
            start = start->clean_bitmap;
        }
        return start;
    }

    void *fbs_server::load_data_to_rom(loader::mbm_file &mbmf_, const std::size_t idx_, std::size_t &size_decomp, int *err_code) {
        *err_code = fbs_load_data_err_none;
        size_decomp = 0;

        if (idx_ >= mbmf_.trailer.count) {
            *err_code = fbs_load_data_err_invalid_arg;
            return nullptr;
        }

        std::size_t size_when_compressed = epoc::get_byte_width(mbmf_.sbm_headers[idx_].size_pixels.x,
                                               mbmf_.sbm_headers[idx_].bit_per_pixels)
            * mbmf_.sbm_headers[idx_].size_pixels.y;

        if (!mbmf_.read_single_bitmap(idx_, nullptr, size_when_compressed)) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return nullptr;
        } else {
            size_decomp = size_when_compressed;
        }

        std::size_t avail_dest_size = common::align(size_when_compressed, 4);
        void *data = shared_chunk_allocator->allocate(avail_dest_size);
        *err_code = fbs_load_data_err_small_bitmap;

        if (data == nullptr) {
            *err_code = fbs_load_data_err_out_of_mem;
            return nullptr;
        }

        bool result_read = mbmf_.read_single_bitmap(idx_, reinterpret_cast<std::uint8_t *>(data), avail_dest_size);

        if (!result_read) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return nullptr;
        }

        return data;
    }

    struct fast_bitmap_load_info_kerntrans {
        epoc::filename name_;
        std::int32_t id_;
        std::int32_t share_if_loaded_;
        std::uint32_t file_offset_;
    };

    void fbscli::load_bitmap_fast(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        int name_slot = 2;
        std::optional<std::u16string> name = std::nullopt;

        if (serv->legacy_level() >= FBS_LEGACY_LEVEL_S60V1) {
            name_slot = 1;
        } else if (serv->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            std::optional<fast_bitmap_load_info_kerntrans> load_info = ctx->get_argument_data_from_descriptor<fast_bitmap_load_info_kerntrans>(1);
            if (!load_info.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }
            name = load_info->name_.to_std_string(nullptr);
            name_slot = -1;
        }

        if (name_slot != -1) {
            name = ctx->get_argument_value<std::u16string>(name_slot);
            if (!name.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }

        symfile source_file = ctx->sys->get_io_system()->open_file(name.value(), READ_MODE | BIN_MODE);
        if (!source_file) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        load_bitmap_impl(ctx, source_file.get());
    }

    void fbscli::duplicate_bitmap(service::ipc_context *ctx) {
        fbsbitmap *bmp = server<fbs_server>()->get<fbsbitmap>(*(ctx->get_argument_value<std::uint32_t>(0)));
        if (!bmp) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        bmp = get_clean_bitmap(bmp);
        const std::uint32_t handle_ret = obj_table_.add(bmp);
        const std::uint32_t server_handle = bmp->id;
        const std::uint32_t off = server<fbs_server>()->host_ptr_to_guest_shared_offset(bmp->bitmap_);
        const bool legacy_return = (ctx->get_argument_data_size(1) >= sizeof(bmp_specs_legacy));

        if (legacy_return) {
            bmp_specs_legacy specs;
            specs.handle = handle_ret;
            specs.server_handle = server_handle;
            specs.address_offset = off;
            ctx->write_data_to_descriptor_argument(1, specs);
        } else {
            bmp_handles handle_info;
            handle_info.handle = handle_ret;
            handle_info.server_handle = server_handle;
            handle_info.address_offset = off;
            ctx->write_data_to_descriptor_argument(1, handle_info);
        }
        ctx->complete(epoc::error_none);
    }

    void fbscli::load_bitmap_impl(service::ipc_context *ctx, file *source) {
        std::optional<load_bitmap_arg> load_options = std::nullopt;
        fbs_server *serv = server<fbs_server>();

        if (serv->legacy_level() >= FBS_LEGACY_LEVEL_S60V1) {
            load_options = std::make_optional<load_bitmap_arg>();
            load_options->bitmap_id = ctx->get_argument_value<std::uint32_t>(2).value();
            load_options->share = ctx->get_argument_value<std::uint32_t>(3).value();
        } else if (serv->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            std::optional<fast_bitmap_load_info_kerntrans> load_info = ctx->get_argument_data_from_descriptor<fast_bitmap_load_info_kerntrans>(1);
            if (!load_info.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }
            load_options = std::make_optional<load_bitmap_arg>();
            load_options->bitmap_id = load_info->id_;
            load_options->share = load_info->share_if_loaded_;
            load_options->file_offset = load_info->file_offset_;
        } else {
            load_options = ctx->get_argument_data_from_descriptor<load_bitmap_arg>(1);
        }

        if (!load_options) {
            ctx->complete(epoc::error_argument);
            return;
        }

        LOG_TRACE(SERVICE_FBS, "Loading bitmap from: {}, id: {}", common::ucs2_to_utf8(source->file_name()),
            load_options->bitmap_id);

        fbsbitmap *bmp = nullptr;
        fbs_server *fbss = server<fbs_server>();
        fbsbitmap_cache_info cache_info_;
        bool already_cache = false;

        if (load_options->share) {
            cache_info_.bitmap_idx = load_options->bitmap_id;
            cache_info_.path = source->file_name();

            auto shared_bitmap_ite = fbss->shared_bitmaps.find(cache_info_);
            if (shared_bitmap_ite != fbss->shared_bitmaps.end()) {
                bmp = shared_bitmap_ite->second;
                already_cache = true;
            }
        }

        if (!bmp) {
            eka2l1::ro_file_stream stream(source);
            std::unique_ptr<common::ro_stream> ref_stream;
            common::ro_stream *stream_for_mbm_read = reinterpret_cast<common::ro_stream *>(&stream);
            std::uint32_t aif_v2_uid = 0;
            static constexpr std::uint32_t expected_aifv2_uid = 0x101FB032;
            bool found_embedded = false;

            if ((stream.read(&aif_v2_uid, 4) == 4) && (aif_v2_uid == expected_aifv2_uid)) {
                std::uint32_t data_size = 0;
                stream.seek(12, common::seek_where::cur);
                if (stream.read(&data_size, 4) == 4) {
                    std::uint32_t ptr_start = data_size + (((data_size % 4) == 0) ? 0 : (4 - (data_size % 4))) + 20;
                    if (stream.size() > ptr_start) {
                        ref_stream = std::make_unique<common::ro_window_ref_stream>(stream, ptr_start,
                            stream.size() - ptr_start);
                        stream_for_mbm_read = ref_stream.get();
                        found_embedded = true;
                    }
                }
            }

            if (!found_embedded) {
                stream_for_mbm_read->seek(0, common::seek_where::beg);
            }

            loader::mbm_file mbmf_(stream_for_mbm_read);
            mbmf_.index_to_loads.push_back(load_options->bitmap_id);

            if (!mbmf_.do_read_headers()) {
                ctx->complete(epoc::error_corrupt);
                return;
            }

            if (!mbmf_.is_header_loaded(load_options->bitmap_id)) {
                ctx->complete(epoc::error_not_found);
                return;
            }

            epoc::bitwise_bitmap *bws_bmp = fbss->allocate_general_data<epoc::bitwise_bitmap>();
            int err_code = fbs_load_data_err_none;
            std::size_t size_when_decomp = 0;

            auto bmp_data = fbss->load_data_to_rom(mbmf_, load_options->bitmap_id, size_when_decomp, &err_code);
            std::uint8_t *bmp_data_base = reinterpret_cast<std::uint8_t *>(bws_bmp);

            switch (err_code) {
            case fbs_load_data_err_none:
                break;
            case fbs_load_data_err_small_bitmap:
                break;
            case fbs_load_data_err_out_of_mem: {
                LOG_ERROR(SERVICE_FBS, "Can't allocate data for storing bitmap!");
                ctx->complete(epoc::error_no_memory);
                return;
            }
            case fbs_load_data_err_read_decomp_fail: {
                LOG_ERROR(SERVICE_FBS, "Can't read or decompress bitmap data, possibly corrupted.");
                ctx->complete(epoc::error_corrupt);
                return;
            }
            default: {
                LOG_ERROR(SERVICE_FBS, "Unknown error code from loading uncompressed bitmap!");
                ctx->complete(epoc::error_general);
                return;
            }
            }

            loader::sbm_header &header_to_give = mbmf_.sbm_headers[load_options->bitmap_id];
            const epoc::display_mode dpm = epoc::get_display_mode_from_bpp(header_to_give.bit_per_pixels, header_to_give.color);

            bws_bmp->construct(header_to_give, dpm, bmp_data, bmp_data_base, support_current_display_mode, false);
            bws_bmp->offset_from_me_ = true;
            bws_bmp->header_.bitmap_size = static_cast<std::uint32_t>(bws_bmp->header_.header_len + size_when_decomp);
            bws_bmp->header_.compression = epoc::bitmap_file_no_compression;
            bws_bmp->post_construct(fbss);

            bmp = make_new<fbsbitmap>(fbss, bws_bmp, static_cast<bool>(load_options->share), support_dirty_bitmap);
        }

        if (load_options->share && !already_cache) {
            fbss->shared_bitmaps.emplace(cache_info_, bmp);
        }

        bmp_handles handle_info;
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_data_to_descriptor_argument<bmp_handles>(0, handle_info);
        ctx->complete(epoc::error_none);
    }

    static std::size_t calculate_aligned_bitmap_bytes(const eka2l1::vec2 &size, const epoc::display_mode bpp) {
        if (size.x == 0 || size.y == 0) {
            return 0;
        }
        return epoc::get_byte_width(size.x, epoc::get_bpp_from_display_mode(bpp)) * size.y;
    }

    static std::uint32_t calculate_reserved_each_side(const std::uint32_t height) {
        static constexpr std::uint32_t MAXIMUM_RESERVED_HEIGHT = 50;
        static constexpr std::uint32_t PERCENTAGE_RESERVE_HEIGHT_EACH_SIDE = 15;
        return common::min<std::uint32_t>(MAXIMUM_RESERVED_HEIGHT, height * PERCENTAGE_RESERVE_HEIGHT_EACH_SIDE / 100);
    }

    fbsbitmap *fbs_server::create_bitmap(fbs_bitmap_data_info &info, const bool alloc_data, const bool support_current_display_mode_flag, const bool support_dirty) {
        if (!shared_chunk) {
            initialize_server();
        }

        epoc::bitwise_bitmap *bws_bmp = allocate_general_data<epoc::bitwise_bitmap>();
        std::uint32_t final_reserve_each_side = calculate_reserved_each_side(info.size_.y);

        if (info.data_size_ || !alloc_data) {
            final_reserve_each_side = 0;
        }

        const std::size_t byte_width = epoc::get_byte_width(info.size_.x, epoc::get_bpp_from_display_mode(info.dpm_));
        std::size_t original_bytes = (info.data_size_ == 0) ? calculate_aligned_bitmap_bytes(info.size_, info.dpm_) : info.data_size_;
        std::size_t alloc_bytes = original_bytes + final_reserve_each_side * byte_width * 2;

        void *data = nullptr;
        std::uint8_t *base = nullptr;
        bool smol = false;

        if ((original_bytes > 0) && alloc_data) {
            std::size_t avail_dest_size = common::align(alloc_bytes, 4);

            base = reinterpret_cast<std::uint8_t *>(bws_bmp);
            data = shared_chunk_allocator->allocate(avail_dest_size);
            smol = true;

            if (!data) {
                shared_chunk_allocator->freep(bws_bmp);
                return nullptr;
            }
        }

        loader::sbm_header header;
        header.compression = info.comp_;
        header.bitmap_size = static_cast<std::uint32_t>(original_bytes + sizeof(loader::sbm_header));
        header.size_pixels = info.size_;
        header.color = get_bitmap_color_type_from_display_mode(info.dpm_);
        header.header_len = sizeof(loader::sbm_header);
        header.palette_size = 0;
        header.size_twips = info.size_ * epoc::get_approximate_pixel_to_twips_mul(kern->get_epoc_version());
        header.bit_per_pixels = epoc::get_bpp_from_display_mode(info.dpm_);

        const std::size_t byte_width_calc = epoc::get_byte_width(header.size_pixels.width(), static_cast<std::uint8_t>(header.bit_per_pixels));
        const std::size_t reserved_bytes = (byte_width_calc * final_reserve_each_side);
        if (data) {
            data = reinterpret_cast<std::uint8_t *>(data) + reserved_bytes;
        }

        bws_bmp->construct(header, info.dpm_, data, base, support_current_display_mode_flag, true);
        bws_bmp->offset_from_me_ = smol;
        bws_bmp->post_construct(this);

        if (info.data_) {
            std::memcpy(data, info.data_, common::min<std::size_t>(info.data_size_, original_bytes));
        }

        fbsbitmap *bmp = make_new<fbsbitmap>(this, bws_bmp, false, support_dirty, final_reserve_each_side);
        return bmp;
    }

    bool fbs_server::free_bitmap(fbsbitmap *bmp) {
        if (!bmp->bitmap_ || bmp->count > 0) {
            return false;
        }

        bool no_failure = true;

        if (bmp->bitmap_->data_offset_) {
            const std::size_t reserved_bytes = bmp->reserved_height_each_side_ * bmp->bitmap_->byte_width_;
            shared_chunk_allocator->freep(bmp->bitmap_->data_pointer(this) - reserved_bytes);
            no_failure = true;
        }

        if (!free_general_data(bmp->bitmap_)) {
            no_failure = true;
        }

        common::erase_elements(shared_bitmaps, [bmp](const std::pair<const eka2l1::fbsbitmap_cache_info, fbsbitmap *> &info) -> bool {
            return info.second == bmp;
        });
        return no_failure;
    }

    bool fbs_server::is_large_bitmap(const std::uint32_t compressed_size) const {
        static constexpr std::uint32_t RANGE_START_LARGE = 1 << 12;
        static constexpr std::uint32_t RANGE_START_LARGE_TRANS = 1 << 16;
        const std::uint32_t size_aligned = (((compressed_size + 3) / 4) << 2);

        switch (legacy_level()) {
        case FBS_LEGACY_LEVEL_S60V1:
        case FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION:
            return size_aligned >= RANGE_START_LARGE;
        case FBS_LEGACY_LEVEL_KERNEL_TRANSITION:
            return size_aligned >= RANGE_START_LARGE_TRANS;
        default:
            break;
        }
        return true;
    }

    void fbscli::create_bitmap(service::ipc_context *ctx) {
        bmp_specs_legacy specs;
        const bool use_spec_legacy = ctx->get_argument_data_size(0) >= sizeof(bmp_specs_legacy);
        const bool use_bmp_handles_writeback = (server<fbs_server>()->get_system()->get_symbian_version_use() >= epocver::epoc10);
        std::uint32_t assign_uid = epoc::NORMAL_BITMAP_UID_REV2;
        std::uint32_t force_size = 0;

        if (!use_spec_legacy) {
            if (use_bmp_handles_writeback) {
                std::optional<bmp_specs_v2> specs_morden_v2 = ctx->get_argument_data_from_descriptor<bmp_specs_v2>(0);
                if (!specs_morden_v2.has_value()) {
                    ctx->complete(epoc::error_argument);
                    return;
                }
                specs.size = specs_morden_v2->size_pixels;
                specs.bpp = specs_morden_v2->bpp;
                force_size = specs_morden_v2->data_size_force;
                assign_uid = specs_morden_v2->bitmap_uid;
            } else {
                std::optional<bmp_specs> specs_morden = ctx->get_argument_data_from_descriptor<bmp_specs>(0);
                if (!specs_morden) {
                    ctx->complete(epoc::error_argument);
                    return;
                }
                specs.size = specs_morden->size;
                specs.bpp = specs_morden->bpp;
            }
        } else {
            std::optional<bmp_specs_legacy> specs_legacy = ctx->get_argument_data_from_descriptor<bmp_specs_legacy>(0);
            if (!specs_legacy) {
                ctx->complete(epoc::error_argument);
                return;
            }
            specs = std::move(specs_legacy.value());
        }

        fbs_server *fbss = server<fbs_server>();
        fbs_bitmap_data_info info;
        info.size_ = specs.size;
        info.dpm_ = specs.bpp;
        info.data_size_ = force_size;

        fbsbitmap *bmp = fbss->create_bitmap(info, true, support_current_display_mode, support_dirty_bitmap);
        if (!bmp) {
            ctx->complete(epoc::error_no_memory);
            return;
        }

        if (use_bmp_handles_writeback) {
            if (assign_uid != epoc::NORMAL_BITMAP_UID_REV2) {
                bool supported = false;
                for (std::size_t i = 0; i < epoc::SUPPORTED_REV2_UID_COUNT; i++) {
                    if (epoc::SUPPORTED_REV2_UIDS[i] == assign_uid) {
                        bmp->bitmap_->uid_ = assign_uid;
                        supported = true;
                        break;
                    }
                }
                if (!supported) {
                    LOG_WARN(SERVICE_FBS, "Trying to create non-standard bitmap with UID 0x{:X}! Revisit soon!", assign_uid);
                }
            }
        }

        const std::uint32_t handle_ret = obj_table_.add(bmp);
        const std::uint32_t serv_handle = bmp->id;
        const std::uint32_t addr_off = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        if (use_spec_legacy) {
            specs.handle = handle_ret;
            specs.server_handle = serv_handle;
            specs.address_offset = addr_off;
            ctx->write_data_to_descriptor_argument(0, specs);
        } else {
            if (use_bmp_handles_writeback) {
                bmp_handles handles;
                handles.address_offset = addr_off;
                handles.server_handle = serv_handle;
                handles.handle = handle_ret;
                ctx->write_data_to_descriptor_argument<bmp_handles>(1, handles);
            } else {
                bmp_specs specs_to_write;
                specs_to_write.size = specs.size;
                specs_to_write.bpp = specs.bpp;
                specs_to_write.handle = handle_ret;
                specs_to_write.server_handle = serv_handle;
                specs_to_write.address_offset = addr_off;
                ctx->write_data_to_descriptor_argument(0, specs_to_write);
            }
        }
        ctx->complete(epoc::error_none);
    }

    fbsbitmap *fbscli::get_clean_bitmap(fbsbitmap *bmp) {
        return bmp->final_clean();
    }

    void fbscli::resize_bitmap(service::ipc_context *ctx) {
        const auto fbss = server<fbs_server>();
        const epoc::handle handle = *(ctx->get_argument_value<std::uint32_t>(0));
        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(handle);

        if (!bmp) {
            ctx->complete(epoc::error_unknown);
            return;
        }

        bmp = get_clean_bitmap(bmp);
        const vec2 new_size = { *(ctx->get_argument_value<int>(1)), *(ctx->get_argument_value<int>(2)) };

        fbsbitmap *new_bmp = nullptr;
        std::uint8_t *dest_data = nullptr;
        std::uint8_t *base = nullptr;
        const std::uint32_t reserved_each_size = calculate_reserved_each_side(new_size.y);
        bool offset_from_me_now = false;

        if ((new_size.x != 0) && (new_size.y != 0)) {
            if (fbss->legacy_level() >= FBS_LEGACY_LEVEL_SYMBIAN_92) {
                new_bmp = bmp;
                const int dest_byte_width = epoc::get_byte_width(new_size.x, bmp->bitmap_->header_.bit_per_pixels);
                const int size_total = dest_byte_width * new_size.y;
                const int size_added_reserve = size_total + reserved_each_size * dest_byte_width * 2;

                dest_data = reinterpret_cast<std::uint8_t *>(fbss->allocate_general_data_impl(size_added_reserve));
                base = reinterpret_cast<std::uint8_t *>(new_bmp->bitmap_);
                offset_from_me_now = true;
                dest_data += new_bmp->reserved_height_each_side_ * dest_byte_width;
            } else {
                const epoc::display_mode disp_mode = bmp->bitmap_->settings_.current_display_mode();
                fbs_bitmap_data_info info;
                info.size_ = new_size;
                info.dpm_ = disp_mode;

                new_bmp = fbss->create_bitmap(info, true, support_current_display_mode, support_dirty_bitmap);
                dest_data = new_bmp->bitmap_->data_pointer(fbss);
            }
            bmp->bitmap_->copy_to(dest_data, new_size, fbss);
        } else {
            dest_data = nullptr;
            base = nullptr;

            if (fbss->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
                new_bmp = bmp;
                offset_from_me_now = true;
            } else {
                const epoc::display_mode disp_mode = bmp->bitmap_->settings_.current_display_mode();
                fbs_bitmap_data_info info;
                info.size_ = new_size;
                info.dpm_ = disp_mode;

                new_bmp = fbss->create_bitmap(info, false, support_current_display_mode, support_dirty_bitmap);
            }
        }

        if (fbss->legacy_level() <= FBS_LEGACY_LEVEL_EARLY_EKA2) {
            bmp->clean_bitmap = new_bmp;
            bmp->bitmap_->settings_.dirty_bitmap(true);
            new_bmp->ref();

            bmp_handles handle_info;
            handle_info.handle = obj_table_.add(new_bmp);
            handle_info.server_handle = new_bmp->id;
            handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(new_bmp->bitmap_);

            ctx->write_data_to_descriptor_argument(3, handle_info);
            obj_table_.remove(handle);
        } else {
            loader::sbm_header old_header = new_bmp->bitmap_->header_;
            old_header.size_pixels.x = new_size.x;
            old_header.size_pixels.y = new_size.y;
            old_header.size_twips = old_header.size_pixels * epoc::get_approximate_pixel_to_twips_mul(fbss->kern->get_epoc_version());

            std::uint8_t *data = new_bmp->original_pointer(fbss);
            fbss->free_general_data_impl(data);

            new_bmp->reserved_height_each_side_ = reserved_each_size;
            new_bmp->bitmap_->construct(old_header, new_bmp->bitmap_->settings_.initial_display_mode(),
                dest_data, base, support_current_display_mode, false);
            new_bmp->bitmap_->post_construct(fbss);
        }

        new_bmp->bitmap_->offset_from_me_ = offset_from_me_now;
        ctx->complete(epoc::error_none);
    }

    void fbscli::notify_dirty_bitmap(service::ipc_context *ctx) {
        if (dirty_nof_.empty()) {
            dirty_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
        }
    }

    void fbscli::cancel_notify_dirty_bitmap(service::ipc_context *ctx) {
        dirty_nof_.complete(epoc::error_cancel);
        ctx->complete(epoc::error_none);
    }

    void fbscli::get_clean_bitmap(service::ipc_context *ctx) {
        const epoc::handle bmp_handle = *ctx->get_argument_value<epoc::handle>(0);
        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(bmp_handle);

        if (!bmp) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        bmp = get_clean_bitmap(bmp);
        bmp_handles handle_info;

        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        obj_table_.remove(bmp_handle);
        ctx->write_data_to_descriptor_argument(1, handle_info);
        ctx->complete(epoc::error_none);
    }
}