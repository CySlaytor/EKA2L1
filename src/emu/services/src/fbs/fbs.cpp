#include <common/cvt.h>
#include <common/log.h>
#include <common/thread.h>
#include <common/vecx.h>
#include <config/config.h>
#include <kernel/chunk.h>
#include <kernel/kernel.h>
#include <kernel/libmanager.h>
#include <services/fbs/fbs.h>
#include <services/fs/std.h>
#include <system/epoc.h>
#include <utils/cppabi.h>
#include <utils/err.h>
#include <utils/system.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    namespace epoc {
        bool does_client_use_pointer_instead_of_offset(fbscli *cli) {
            const epocver current_sys_ver = cli->server<fbs_server>()->get_system()->get_symbian_version_use();
            return (cli->client_version().build <= epoc::RETURN_POINTER_NOT_OFFSET_BUILD_LIMIT) && (current_sys_ver < epocver::epoc95);
        }

        std::string get_fbs_server_name_by_epocver(const epocver ver) {
            if (ver < epocver::epoc81a) {
                return "Fontbitmapserver";
            }
            return "!Fontbitmapserver";
        }

        void query_fbs_feature_support(fbs_server *fbss, bool &support_current_display_mode, bool &support_dirty_bitmap) {
            support_dirty_bitmap = true;

            if (fbss->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
                if (fbss->legacy_level() == FBS_LEGACY_LEVEL_KERNEL_TRANSITION)
                    support_current_display_mode = true;
                else
                    support_current_display_mode = false;
                support_dirty_bitmap = false;
            } else if (fbss->legacy_level() == FBS_LEGACY_LEVEL_SYMBIAN_92) {
                support_dirty_bitmap = false;
                support_current_display_mode = true;
            }
        }
    }

    fbscli::~fbscli() {
        if (!dirty_nof_.empty()) {
            dirty_nof_.complete(epoc::error_cancel);
        }
        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            server<fbs_server>()->session_cache_link->remove(this);
        } else {
            server<fbs_server>()->session_cache_list->erase_cache(this);
        }
        if (glyph_info_for_legacy_return_) {
            server<fbs_server>()->free_general_data(glyph_info_for_legacy_return_);
        }
    }

    void fbscli::fetch(service::ipc_context *ctx) {
        if (ctx->sys->get_symbian_version_use() < epocver::eka2) {
            switch (ctx->msg->function) {
            case fbs_set_pixel_size_in_twips + 1:
                ctx->msg->function = fbs_nearest_font_design_height_in_twips;
                break;
            case fbs_set_pixel_size_in_twips + 2:
                ctx->msg->function = fbs_nearest_font_design_height_in_pixels;
                break;
            default: {
                if ((ctx->msg->function > fbs_set_pixel_size_in_twips + 2) && (ctx->msg->function < fbs_nearest_font_design_height_in_twips + 2)) {
                    ctx->msg->function -= 2;
                }
                break;
            }
            }
        }

        switch (ctx->msg->function) {
        case fbs_init: {
            connection_id_ = server<fbs_server>()->init();
            if (ctx->sys->get_symbian_version_use() <= epocver::epoc93fp2) {
                connection_id_ = ctx->msg->msg_session->get_associated_handle();
                ctx->complete(epoc::error_none);
            } else {
                ctx->complete(connection_id_);
            }
            break;
        }
        case fbs_num_typefaces: {
            num_typefaces(ctx);
            break;
        }
        case fbs_typeface_support: {
            typeface_support(ctx);
            break;
        }
        case fbs_nearest_font_design_height_in_pixels:
        case fbs_nearest_font_design_height_in_twips:
        case fbs_nearest_font_max_height_in_pixels:
        case fbs_nearest_font_max_height_in_twips: {
            get_nearest_font(ctx);
            break;
        }
        case fbs_rasterize: {
            rasterize_glyph(ctx);
            break;
        }
        case fbs_bitmap_load_fast: {
            load_bitmap_fast(ctx);
            break;
        }
        case fbs_font_dup: {
            duplicate_font(ctx);
            break;
        }
        case fbs_bitmap_dup: {
            duplicate_bitmap(ctx);
            break;
        }
        case fbs_bitmap_create: {
            create_bitmap(ctx);
            break;
        }
        case fbs_bitmap_resize: {
            resize_bitmap(ctx);
            break;
        }
        case fbs_bitmap_notify_dirty: {
            notify_dirty_bitmap(ctx);
            break;
        }
        case fbs_bitmap_cancel_notify_dirty: {
            cancel_notify_dirty_bitmap(ctx);
            break;
        }
        case fbs_bitmap_clean: {
            get_clean_bitmap(ctx);
            break;
        }
        case fbs_close: {
            if (!obj_table_.remove(*ctx->get_argument_value<std::uint32_t>(0))) {
                ctx->complete(epoc::error_bad_handle);
                break;
            }
            ctx->complete(epoc::error_none);
            break;
        }
        default: {
            LOG_ERROR(SERVICE_FBS, "Unhandled FBScli opcode 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }
        }
    }

    fbs_server::fbs_server(eka2l1::system *sys)
        : service::typical_server(sys, epoc::get_fbs_server_name_by_epocver(sys->get_symbian_version_use()))
        , persistent_font_store(sys->get_io_system())
        , shared_chunk(nullptr)
        , fntstr_seg(nullptr)
        , bmp_font_vtab(0)
        , session_cache_list(nullptr) {
    }

    int fbs_server::legacy_level() const {
        if (kern->get_epoc_version() <= epocver::epoc6)
            return FBS_LEGACY_LEVEL_S60V1;
        if (kern->get_epoc_version() <= epocver::epoc7)
            return FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION;
        if (kern->get_epoc_version() <= epocver::epoc81b)
            return FBS_LEGACY_LEVEL_KERNEL_TRANSITION;
        if (kern->get_epoc_version() == epocver::epoc93fp1)
            return FBS_LEGACY_LEVEL_SYMBIAN_92;
        return FBS_LEGACY_LEVEL_MORDEN;
    }

    void fbs_server::initialize_server() {
        shared_chunk = kern->create_and_add<kernel::chunk>(
                               kernel::owner_type::kernel,
                               kern->get_memory_system(),
                               nullptr,
                               "FbsSharedChunk",
                               0,
                               0x10000,
                               (kern->is_eka1() ? 0x600000 : 0x200000),
                               prot_read_write,
                               kernel::chunk_type::normal,
                               kernel::chunk_access::global,
                               kernel::chunk_attrib::none)
                           .second;

        if (!shared_chunk) {
            LOG_CRITICAL(SERVICE_FBS, "Can't create shared chunk of FBS, exiting");
            return;
        }

        base_shared_chunk = reinterpret_cast<std::uint8_t *>(shared_chunk->host_base());
        shared_chunk_allocator = std::make_unique<epoc::chunk_allocator>(shared_chunk);

        if ((fntstr_seg = sys->get_lib_manager()->load(u"fntstr.dll"))) {
            if (kern->is_eka1()) {
                std::uint8_t *addr = nullptr;
                fntstr_seg->get_code_run_addr(nullptr, &addr);
                utils::cpp_gcc98_abi_analyser analyser(addr, fntstr_seg->get_text_size());

                std::vector<address> clues;
                clues.push_back(fntstr_seg->lookup_no_relocate(51));
                clues.push_back(fntstr_seg->lookup_no_relocate(52));

                bmp_font_vtab = static_cast<std::uint32_t>(analyser.search_vtable(clues));
            } else {
                bmp_font_vtab = fntstr_seg->lookup_no_relocate(97) + 2 * 4;
            }

            if (bmp_font_vtab == 0) {
                LOG_ERROR(SERVICE_FBS, "Unable to find vtable address of CBitmapFont!");
            }
            if (kern->is_eka1()) {
                bmp_font_vtab += fntstr_seg->get_code_base();
            }
        }

        load_fonts(sys->get_io_system());
        fs_server = kern->get_by_name<service::server>(epoc::fs::get_server_name_through_epocver(kern->get_epoc_version()));

        session_cache_list = allocate_general_data<epoc::open_font_session_cache_list>();
        session_cache_link = allocate_general_data<epoc::open_font_session_cache_link>();
        session_cache_list->init();

        shared_chunk_allocator->allocate(4);
    }

    void fbs_server::connect(service::ipc_context &context) {
        if (!shared_chunk) {
            initialize_server();
        }
        create_session<fbscli>(&context);
        context.complete(epoc::error_none);
    }

    service::uid fbs_server::init() {
        return ++connection_id_counter;
    }

    void *fbs_server::allocate_general_data_impl(const std::size_t s) {
        if (!shared_chunk || !shared_chunk_allocator) {
            LOG_CRITICAL(SERVICE_FBS, "FBS server hasn't initialized yet");
            return nullptr;
        }
        return shared_chunk_allocator->allocate(s);
    }

    bool fbs_server::free_general_data_impl(const void *ptr) {
        if (!shared_chunk || !shared_chunk_allocator) {
            LOG_CRITICAL(SERVICE_FBS, "FBS server hasn't initialized yet");
            return false;
        }
        return shared_chunk_allocator->freep(ptr);
    }

    fbs_server::~fbs_server() {
        sessions.clear();
        font_obj_container.clear();
        obj_con.clear();

        if (session_cache_list) {
            session_cache_list->~open_font_session_cache_list();
        }
        if (shared_chunk)
            kern->destroy(shared_chunk);
    }

    drivers::graphics_driver *fbs_server::get_graphics_driver() {
        return sys->get_graphics_driver();
    }

    fbscli::fbscli(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , glyph_info_for_legacy_return_(nullptr)
        , glyph_info_for_legacy_return_addr_(0) {
        fbs_server *fbss = reinterpret_cast<fbs_server *>(serv);
        epoc::query_fbs_feature_support(fbss, support_current_display_mode, support_dirty_bitmap);

        if (fbss->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            glyph_info_for_legacy_return_ = fbss->allocate_general_data<epoc::open_font_glyph_v1_use_for_fbs>();
            glyph_info_for_legacy_return_addr_ = fbss->host_ptr_to_guest_general_data(glyph_info_for_legacy_return_).ptr_address();
        }
    }
}