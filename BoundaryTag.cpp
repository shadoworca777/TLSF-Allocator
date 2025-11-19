#include "BoundaryTag.hpp"

namespace TLSF {

BoundaryTagEnd::BoundaryTagEnd(size_t size) {
    setSize(size);
}

void BoundaryTagEnd::setSize(size_t size) {
    mMemSize = size;
}

BoundaryTagBegin::BoundaryTagBegin()
    : m_size(0)
    , m_is_used(0)
    , m_next_free(nullptr)
    , m_prev_free(nullptr)
{
    setSize( 0 );
}

BoundaryTagBegin::BoundaryTagBegin(size_t size)
    : m_size(size)
    , m_is_used(0)
    , m_next_free(nullptr)
    , m_prev_free(nullptr)
{
    setSize(size);
}

bool BoundaryTagBegin::merge(BoundaryTagBegin* next_block) {
    if (!next_block || next_block->isUsed()) return false;

    next_block->detach();

    uint32_t added_size = next_block->getSize() + sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd);
    m_size += added_size;

    BoundaryTagEnd* footer = this->endTag();
    footer->setSize(m_size);

    return true;
}

BoundaryTagBegin* BoundaryTagBegin::split(size_t size) {
    if (m_size < size + sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd)) return nullptr;

    size_t remaining_size = m_size - size - (sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd));

    this->setSize(size);
    this->endTag()->setSize(size);

    BoundaryTagBegin* next_block = this->nextTag();

    next_block->setSize(remaining_size);
    next_block->setUsed(false);
    next_block->setNextLink(nullptr);
    next_block->setPrevLink(nullptr);

    next_block->endTag()->setSize(remaining_size);

    return next_block;
}

BoundaryTagBegin* BoundaryTagBegin::prevTag() {
    uint8_t* addr = reinterpret_cast<uint8_t*>(this);
    BoundaryTagEnd* prev_end = reinterpret_cast<BoundaryTagEnd*>(addr - sizeof(BoundaryTagEnd));
    size_t prev_size = prev_end->size();
    addr -= sizeof(BoundaryTagEnd) + prev_size + sizeof(BoundaryTagBegin);
    return reinterpret_cast<BoundaryTagBegin*>(addr);
}

BoundaryTagBegin* BoundaryTagBegin::nextTag() {
    uint8_t* addr = reinterpret_cast<uint8_t*>(this);
    addr += sizeof(BoundaryTagBegin) + m_size + sizeof(BoundaryTagEnd);
    return reinterpret_cast<BoundaryTagBegin*>(addr);
}

BoundaryTagEnd* BoundaryTagBegin::endTag() {
    uint8_t* addr = reinterpret_cast<uint8_t*>(this);
    addr += sizeof(BoundaryTagBegin) + m_size;
    return reinterpret_cast<BoundaryTagEnd*>(addr);
}

void BoundaryTagBegin::detach() {
    if (m_prev_free) m_prev_free->setNextLink(m_next_free);
    if (m_next_free) m_next_free->setPrevLink(m_prev_free);
    m_prev_free = nullptr;
    m_next_free = nullptr;
}

BoundaryTagBegin* BoundaryTagBegin::newTag(size_t size, uint8_t* p) {
    if (size <= (sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd))) {
        return nullptr;
    }

    size_t usable_size = size - (sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd));

    BoundaryTagBegin* block = reinterpret_cast<BoundaryTagBegin*>(p);

    block->setSize(usable_size);
    block->setUsed(false);
    block->setNextLink(nullptr);
    block->setPrevLink(nullptr);

    block->endTag()->setSize(usable_size);

    return block;
}

void BoundaryTagBegin::deleteTag(BoundaryTagBegin* b) {
    if (!b) return;

    BoundaryTagEnd* e = b->endTag();

    b->~BoundaryTagBegin();
    e->~BoundaryTagEnd();
}

};
