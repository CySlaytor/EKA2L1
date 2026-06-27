#include <common/buffer.h>
#include <common/chunkyseri.h>
#include <dispatch/dispatcher.h>

#include <services/audio/keysound/keysound.h>
#include <services/audio/keysound/ops.h>
#include <utils/err.h>

#include <kernel/process.h>
#include <system/epoc.h>

namespace eka2l1 {
    keysound_session::parser_state::parser_state()
        : frames_(0)
        , target_freq_(44100)
        , frequency_(1)
        , ms_(0)
        , duration_unit_(0)
        , parser_pos_(0) {
    }

    void keysound_session::parser_state::set(epoc::keysound::sound_info &new_sound) {
        frames_ = 0;
        ms_ = 0;
        parser_pos_ = 0;
        frequency_ = 0;
        duration_unit_ = 0;
        sound_ = new_sound;
    }

    void keysound_session::init(service::ipc_context *ctx) {
        const bool inited = server<keysound_server>()->initialized();
        ctx->write_data_to_descriptor_argument<std::int32_t>(0, inited);

        if (!inited) {
            server<keysound_server>()->initialized(true);
        }

        app_uid_ = *ctx->get_argument_value<std::uint32_t>(1);
        ctx->complete(epoc::error_none);
    }

    void keysound_session::push_context(service::ipc_context *ctx) {
        const auto item_count = ctx->get_argument_value<std::uint32_t>(0);
        const auto uid = ctx->get_argument_value<std::uint32_t>(2);
        const auto rsc_id = ctx->get_argument_value<std::int32_t>(3);

        std::uint8_t *item_def_ptr = ctx->get_descriptor_argument_ptr(1);

        if (!item_count || !uid || !rsc_id || !item_def_ptr) {
            ctx->complete(epoc::error_argument);
            return;
        }

        static constexpr std::uint32_t SIZE_EACH_ITEM_DEF = 5;

        common::chunkyseri seri(item_def_ptr, item_count.value() * SIZE_EACH_ITEM_DEF,
            common::SERI_MODE_READ);

        contexts_.emplace_back(seri, uid.value(), rsc_id.value(), item_count.value());
        ctx->complete(epoc::error_none);
    }

    void keysound_session::play_key(service::ipc_context *ctx) {
        if (true) {
            ctx->complete(epoc::error_none);
            return;
        }
    }

    void keysound_session::add_sids(service::ipc_context *ctx) {
        std::uint8_t *sound_descriptor = ctx->get_descriptor_argument_ptr(2);
        const std::optional<std::uint32_t> sound_descriptor_size = ctx->get_argument_value<std::uint32_t>(1);
        const std::optional<std::uint32_t> uid = ctx->get_argument_value<std::uint32_t>(0);

        if (!sound_descriptor || !sound_descriptor_size || !uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        common::ro_buf_stream stream(sound_descriptor, sound_descriptor_size.value());

        std::uint16_t count = 0;
        stream.read(&count, 2);

        epoc::keysound::sound_info info;

        for (std::uint16_t i = 0; i < count; i++) {
            stream.read(&info.id_, 4);
            stream.read(&info.priority_, 2);
            stream.read(&info.preference_, 4);

            std::uint8_t type;
            stream.read(&type, 1);

            switch (type) {
            case 0: {
                info.type_ = epoc::keysound::sound_type::sound_type_file;
                LOG_TRACE(SERVICE_KEYSOUND, "Load filename sound TODO. Please notice this");
                break;
            }
            case 1: {
                info.type_ = epoc::keysound::sound_type::sound_type_tone;
                stream.read(&info.freq_, 2);
                stream.read(&info.duration_, 4);
                break;
            }
            case 2: {
                info.type_ = epoc::keysound::sound_type::sound_type_sequence;
                std::uint16_t sequence_length = 0;
                stream.read(&sequence_length, 2);
                info.sequences_.resize(sequence_length);
                stream.read(&(info.sequences_[0]), sequence_length);
                break;
            }
            default:
                break;
            }

            stream.read(&info.volume_, 1);
            server<keysound_server>()->add_sound(info);
        }

        ctx->complete(epoc::error_none);
    }

    void keysound_session::bring_to_foreground(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void keysound_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::keysound::opcode_init: {
            init(ctx);
            break;
        }

        case epoc::keysound::opcode_push_context: {
            push_context(ctx);
            break;
        }

        case epoc::keysound::opcode_add_sids: {
            add_sids(ctx);
            break;
        }

        case epoc::keysound::opcode_play_key: {
            play_key(ctx);
            break;
        }

        case epoc::keysound::opcode_bring_to_foreground: {
            bring_to_foreground(ctx);
            break;
        }

        default:
            LOG_ERROR(SERVICE_KEYSOUND, "Unimplemented keysound server opcode: {}", ctx->msg->function);
            break;
        }
    }

    static constexpr const char *SERVICE_KEYSOUND_SERVER_NAME = "KeySoundServer";

    keysound_server::keysound_server(system *sys)
        : service::typical_server(sys, SERVICE_KEYSOUND_SERVER_NAME)
        , inited_(false) {
    }

    void keysound_server::connect(service::ipc_context &context) {
        create_session<keysound_session>(&context);
        context.complete(0);
    }
}