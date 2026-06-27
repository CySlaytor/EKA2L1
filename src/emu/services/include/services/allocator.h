#pragma once

#include <common/allocator.h>

namespace eka2l1::kernel {
    class chunk;
    class process;
}

namespace eka2l1 {
    using address = std::uint32_t;
    using chunk_ptr = kernel::chunk *;
}

namespace eka2l1::epoc {
    class chunk_allocator : public common::block_allocator {
        chunk_ptr target_chunk;

    public:
        explicit chunk_allocator(chunk_ptr de_chunk);
        virtual bool expand(std::size_t target) override;

        eka2l1::address to_address(void *ptr, kernel::process *pr);
        void *to_pointer(eka2l1::address addr, kernel::process *pr);

        template <typename T, typename... Args>
        T *allocate_struct(Args... construct_args) {
            T *obj = reinterpret_cast<T *>(allocate(sizeof(T)));
            new (obj) T(construct_args...);

            return obj;
        }
    };
}