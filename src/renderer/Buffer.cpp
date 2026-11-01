#include "Buffer.hpp"

using namespace geode::prelude;

Buffer::~Buffer() {
    glDeleteBuffers(1, &id);
    if (mappedData)
        delete[] mappedData;
}

void Buffer::read(void* data, usize size, usize offset) {
    assert(data != nullptr);
    assert((offset + size) <= this->size);

#if BISMUTH_GL_ES
    // Not linked on mobile; resolved at runtime.
    if (!glGetBufferSubData)
        return;
#endif

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, id);
    glGetBufferSubData(GL_ARRAY_BUFFER, offset, size, data);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);
}

void Buffer::write(void* data, usize size, usize offset) {
    assert(data != nullptr);
    assert((offset + size) <= this->size);

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);
}

void* Buffer::map(Access access) {
    if (mappedData) return nullptr;

    if (shouldMap) {
#if BISMUTH_GL_ES
        if (!glMapBufferRange || !glUnmapBuffer) {
            shouldMap = false;
            mappedData = new u8[size];
            return mappedData;
        }
#endif

        bindAs(GL_ARRAY_BUFFER);

        GLbitfield flags = GL_MAP_FLUSH_EXPLICIT_BIT;
        if (access == Access::ReadWrite)
            flags |= GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
        else
            flags |= GL_MAP_WRITE_BIT;

        mappedData = (u8*)glMapBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)size, flags);

        // Some drivers refuse the mapping (e.g. out of memory); fall back to a
        // shadow copy so the renderer keeps working.
        if (!mappedData) {
            log::warn("Failed to map buffer '{}', falling back to a CPU copy", name);
            shouldMap = false;
            mappedData = new u8[size];
        }
    } else {
        mappedData = new u8[size];
    }
    return mappedData;
}

void Buffer::unmap() {
    if (!mappedData) return;

    if (shouldMap) {
        bindAs(GL_ARRAY_BUFFER);

        /*
            The mapping uses GL_MAP_FLUSH_EXPLICIT_BIT, so anything that was not
            flushed explicitly has undefined contents after unmapping. Flushing
            the whole range here is cheap and makes the API forgiving.
        */
        if (glFlushMappedBufferRange)
            glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)size);

        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        delete[] mappedData;
    }

    mappedData = nullptr;
}

void Buffer::flush(usize offset, usize size) {
    if (!mappedData) return;

    if (shouldMap) {
        bindAs(GL_ARRAY_BUFFER);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, offset, size);
    } else {
        write(mappedData + offset, size, offset);
    }
}

UPtr<Buffer> Buffer::create(const char* name, usize size, GLenum usage) {
    u32 buffer;
    glGenBuffers(1, &buffer);

    glObjectLabel(GL_BUFFER, buffer, strlen(name), name);

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, usage);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);

    auto ret = std::make_unique<Buffer>();
    ret->name = name;
    ret->id = buffer;
    ret->size = size;
    ret->shouldMap = Mod::get()->getSettingValue<bool>("use_mapped_buffers");

    return ret;
}