#include "Allocator.hpp"

#define MAX(a,b) ((a)>(b)?(a):(b))

namespace TLSF {

uint32_t Allocator::getMSB(uint64_t v) {
    if (v == 0) return 64;

    static const int index64[64] = {
         0,  1, 48,  2, 57, 49, 28,  3,
        61, 58, 50, 42, 38, 29, 17,  4,
        62, 55, 59, 36, 53, 51, 43, 22,
        45, 39, 33, 30, 24, 18, 12,  5,
        63, 47, 56, 27, 60, 41, 37, 16,
        54, 35, 52, 21, 44, 32, 23, 11,
        46, 26, 40, 15, 34, 20, 31, 10,
        25, 14, 19,  9, 13,  8,  7,  6
    };

    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;

    uint64_t isolated = v - (v >> 1);
    int msb = index64[(isolated * 0x03F79D71B4CB0A89ULL) >> 58];

    return 63 - msb;
}

uint32_t Allocator::getLSB(uint64_t v) {
    if (v == 0) return 64;

    static const int Table[64] = {
        63,  0, 58,  1, 59, 47, 53,  2,
        60, 39, 48, 27, 54, 33, 42,  3,
        61, 51, 37, 40, 49, 18, 28, 20,
        55, 30, 34, 11, 43, 14, 22,  4,
        62, 57, 46, 52, 38, 26, 32, 41,
        50, 36, 17, 19, 29, 10, 13, 21,
        56, 45, 25, 31, 35, 16,  9, 12,
        44, 24, 15,  8, 23,  7,  6,  5
    };

    uint64_t isolated = v & -v;
    return Table[(isolated * 0x07EDD5E59A4E28C2ULL) >> 58];
}

uint32_t Allocator::getSecondLevelIndex(size_t size, uint32_t msb) {
    if (msb < m_sl_index_shift) return 0;
    return (size >> (msb - m_sl_index_shift)) & (kSLLevels - 1);
}

void Allocator::getIndex(size_t size, uint32_t* fli, uint32_t* sli) {
    if (size < m_min_block_size) {
        *fli = 0;
        *sli = 0;
        return;
    }
    *fli = getMSB(size);
    *sli = getSecondLevelIndex(size, *fli);
}

size_t Allocator::getAlignmentedSize(size_t size) {
    size_t res = (size + (kAlignment - 1)) & ~(kAlignment - 1);
    return MAX(res, m_min_block_size);
}

void Allocator::setBit(uint32_t fli, uint32_t sli) {
    m_sl_bitmaps[fli] |= (1U << sli);
    m_fl_bitmap |= (1ULL << fli);
}

void Allocator::clearBit(uint32_t fli, uint32_t sli) {
    m_sl_bitmaps[fli] &= ~(1U << sli);
    if (m_sl_bitmaps[fli] == 0) {
        m_fl_bitmap &= ~(1ULL << fli);
    }
}

void Allocator::registerFreeBlock(BoundaryTagBegin* b) {
    uint32_t fli, sli;
    getIndex(b->getSize(), &fli, &sli);

    BoundaryTagBegin* head = m_free_blocks[fli][sli];
    
    b->setNextLink(head);
    b->setPrevLink(nullptr);
    
    if (head) head->setPrevLink(b);
    m_free_blocks[fli][sli] = b;

    setBit(fli, sli);
}

void Allocator::unregisterFreeBlock(BoundaryTagBegin* b) {
    uint32_t fli, sli;
    getIndex(b->getSize(), &fli, &sli);

    BoundaryTagBegin* next = b->nextLink();
    BoundaryTagBegin* prev = b->prevLink();

    if (prev) {
        prev->setNextLink(next);
    }
    else {
        m_free_blocks[fli][sli] = next;
    }

    if (next) next->setPrevLink(prev);

    b->setNextLink(nullptr);
    b->setPrevLink(nullptr);

    if (m_free_blocks[fli][sli] == nullptr)
        clearBit(fli, sli);
}

BoundaryTagBegin* Allocator::searchFreeBlock(size_t size, uint32_t* out_fli, uint32_t* out_sli) {
    uint32_t fli, sli;
    getIndex(size, &fli, &sli);

    uint32_t sl_map = m_sl_bitmaps[fli] & (~0U << sli);

    if (sl_map == 0) {
        uint64_t fl_map = m_fl_bitmap & (~0ULL << (fli + 1));
        
        if (fl_map == 0) return nullptr;

        fli = getLSB(fl_map);
        sl_map = m_sl_bitmaps[fli];
    }

    sli = getLSB(sl_map);
    
    *out_fli = fli;
    *out_sli = sli;

    BoundaryTagBegin* b = m_free_blocks[fli][sli];
    unregisterFreeBlock(b);
    
    return b;
}

BoundaryTagBegin* Allocator::mergeFreeBlockPrevNext(BoundaryTagBegin* b) {
    BoundaryTagBegin* next = b->nextTag();
    if (next && !next->isUsed()) {
        unregisterFreeBlock(next);
        b->merge(next);
    }

    BoundaryTagBegin* prev = b->prevTag();
    if (reinterpret_cast<uint8_t*>(prev) >= m_pool_start && prev != b) {
        if (!prev->isUsed()) {
            unregisterFreeBlock(prev);
            prev->merge(b);
            b = prev;
        }
    }

    return b;
}

Allocator::Allocator(size_t buffer_size) : m_fl_bitmap(0) {
    m_pool_start = new uint8_t[buffer_size];
    m_pool_total_size = buffer_size;

    // SL=32 (2^5) -> shift=5
    m_sl_index_shift = 5;

    m_min_block_size = sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd)+ sizeof(void*) * 2;
    m_min_block_size = getAlignmentedSize(m_min_block_size);

    for (size_t i = 0; i < kFLLevels; ++i) {
        m_sl_bitmaps[i] = 0;
        for (size_t j = 0; j < kSLLevels; ++j) {
            m_free_blocks[i][j] = nullptr;
        }
    }

    if (buffer_size > (sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd))) {
        BoundaryTagBegin* first_block = BoundaryTagBegin::newTag(buffer_size, m_pool_start);
        if (first_block) {
            registerFreeBlock(first_block);
        }
    }
}

Allocator::~Allocator() {
    delete[] m_pool_start;
}

void* Allocator::allocate(size_t size) {
    size_t adjust_size = getAlignmentedSize(size);

    uint32_t fli, sli;
    BoundaryTagBegin* block = searchFreeBlock(adjust_size, &fli, &sli);

    if (!block) return nullptr;
    block->setUsed(true);

    BoundaryTagBegin* remaining = block->split(adjust_size);
    if (remaining) registerFreeBlock(remaining);

    uint8_t* ptr = reinterpret_cast<uint8_t*>(block) + sizeof(BoundaryTagBegin);
    return static_cast<void*>(ptr);
}

void Allocator::deallocate(void* ptr) {
    if (!ptr) return;

    uint8_t* p = static_cast<uint8_t*>(ptr);
    BoundaryTagBegin* block = reinterpret_cast<BoundaryTagBegin*>(p - sizeof(BoundaryTagBegin));
    block->setUsed(false);
    block = mergeFreeBlockPrevNext(block);

    registerFreeBlock(block);
}

};
