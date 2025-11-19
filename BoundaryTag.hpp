#pragma once

#include <cstdint>

namespace TLSF {

class BoundaryTagEnd {
    size_t mMemSize;
public:
    BoundaryTagEnd(size_t size);

    void setSize( size_t size );

    size_t size() const { return mMemSize; }
};

class BoundaryTagBegin {
public:
    BoundaryTagBegin();
    BoundaryTagBegin(size_t size);

    size_t getSize() const { return m_size; }
    void setSize(size_t size) { m_size = size; }
    
    bool isUsed() const { return m_is_used; }
    void setUsed(bool used) { m_is_used = used; }

    bool merge(BoundaryTagBegin* next);
    BoundaryTagBegin* split(size_t size);

    BoundaryTagBegin* prevTag();
    BoundaryTagBegin* nextTag();
    BoundaryTagEnd* endTag();

    void setNextLink(BoundaryTagBegin* p) { m_next_free = p; }
    void setPrevLink(BoundaryTagBegin* p) { m_prev_free = p; }
    BoundaryTagBegin* nextLink() const    { return m_next_free; }
    BoundaryTagBegin* prevLink() const    { return m_prev_free; }

    void detach();

    static BoundaryTagBegin* newTag(size_t size, uint8_t* p);
    static void deleteTag(BoundaryTagBegin* b);

private:
    size_t m_size; // block size
    uint32_t m_is_used; // In-use flag (1: in use, 0: free)

    BoundaryTagBegin* m_next_free;
    BoundaryTagBegin* m_prev_free;
};

}
