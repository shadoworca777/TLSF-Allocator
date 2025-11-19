#pragma once

#include <cstdint>
#include <cassert>

#include "BoundaryTag.hpp"

namespace TLSF {

static constexpr size_t kAlignment = 8; 
static constexpr size_t kFLLevels  = 64; 
static constexpr size_t kSLLevels  = 32;

class Allocator {
    uint32_t m_sl_index_shift; 
    size_t   m_min_block_size;

    uint8_t* m_pool_start;
    size_t   m_pool_total_size;

    uint64_t m_fl_bitmap;

    uint32_t m_sl_bitmaps[kFLLevels];

    BoundaryTagBegin* m_free_blocks[kFLLevels][kSLLevels];

    uint32_t getMSB(uint64_t v);
    uint32_t getLSB(uint64_t v);

    uint32_t getSecondLevelIndex(size_t size, uint32_t msb);
    void getIndex(size_t size, uint32_t* fli, uint32_t* sli);
    size_t getAlignmentedSize(size_t size);

    void setBit(uint32_t fli, uint32_t sli);
    void clearBit(uint32_t fli, uint32_t sli);

    BoundaryTagBegin* searchFreeBlock(size_t size, uint32_t* out_fli, uint32_t* out_sli);
    void registerFreeBlock(BoundaryTagBegin* b);
    void unregisterFreeBlock(BoundaryTagBegin* b);
    BoundaryTagBegin* mergeFreeBlockPrevNext(BoundaryTagBegin* b);
public:
    Allocator(size_t buffer_size);
    ~Allocator();

    void* allocate(size_t size);
    void deallocate(void* ptr);
};

};
