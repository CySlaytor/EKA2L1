#include <common/algorithm.h>
#include <kernel/chunk.h>
#include <services/allocator.h>

namespace eka2l1::epoc {
    chunk_allocator::chunk_allocator(chunk_ptr de_chunk)
        : block_allocator(reinterpret_cast<std::uint8_t *>(de_chunk->host_base()), de_chunk->committed())
        , target_chunk(std::move(de_chunk)) {
    }

    bool chunk_allocator::expand(std::size_t target) {
        return target_chunk->adjust(common::min<std::size_t>(target_chunk->max_size(), target));
    }

    eka2l1::address chunk_allocator::to_address(void *ptr, kernel::process *pr) {
        return target_chunk->base(pr).ptr_address() + static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t *>(ptr) - reinterpret_cast<std::uint8_t *>(target_chunk->host_base()));
    }

    void *chunk_allocator::to_pointer(eka2l1::address addr, kernel::process *pr) {
        return reinterpret_cast<std::uint8_t *>(target_chunk->host_base()) + (addr - target_chunk->base(pr).ptr_address());
    }
}