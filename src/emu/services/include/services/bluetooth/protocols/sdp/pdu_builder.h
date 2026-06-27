#pragma once

#include <cstdint>
#include <vector>

namespace eka2l1::epoc::bt {
    struct pdu_builder {
    private:
        std::vector<char> buffer_;
        std::uint16_t trans_id_counter_;

    public:
        static constexpr std::uint32_t PDU_ID_OFFSET = 0;
        static constexpr std::uint32_t TRANS_ID_OFFSET = 1;
        static constexpr std::uint32_t PARAM_LEN_OFFSET = 3;
        static constexpr std::uint32_t PDU_HEADER_SIZE = 5;

    public:
        explicit pdu_builder();
        void new_packet();

        void set_pdu_id(const std::uint8_t id);
        void put_data(const char *data, const std::size_t data_size);
        void put_be32(const std::uint32_t data);
        void put_be16(const std::uint16_t data);
        void put_byte(const std::uint8_t data);

        const char *finalize(std::uint32_t &packet_size);
        const std::uint16_t current_transmission_id() const {
            return trans_id_counter_;
        }
    };
}