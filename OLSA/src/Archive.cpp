#include "../include/Archive.h"

#include <utility>

Archive::Archive(
    std::unique_ptr<IArchiveEngine> engine,
    std::shared_ptr<ICountable> counter,
    std::shared_ptr<IArchiveHook> hook)
    : m_hook(std::move(hook)),
      m_engine(std::move(engine)) {
    m_state.input.counter = std::move(counter);
}

ArchiveState& Archive::state() noexcept {
    return m_state;
}

const ArchiveState& Archive::state() const noexcept {
    return m_state;
}

void Archive::reset_input(std::vector<std::uint8_t> buffer, std::uint32_t offset, bool finalized) {
    m_state.input.buffer = std::move(buffer);
    m_state.input.offset = offset;
    m_state.input.finalized = finalized;
    m_state.parsing.itemHeader = {};
    m_state.parsing.resolvedType.reset();
}

ItemHeader Archive::next() {
    const auto header = m_engine->next(m_state);
    m_hook->on_next(m_state, header);
    return header;
}

bool Archive::get(TypeCode::ID hopeCode, std::size_t hopeReadSize) {
    m_hook->on_get(m_state, hopeCode, hopeReadSize);
    return m_engine->get(m_state, hopeCode, hopeReadSize);
}

bool Archive::gets(std::string& str, std::size_t maxSize) {
    return m_engine->gets(m_state, str, maxSize);
}

Archive& Archive::begin() {
    get(TypeCode::ID::BeginGroup, 1);
    return *this;
}

bool Archive::end() {
    return get(TypeCode::ID::EndGroup, 1);
}

bool Archive::doGet(TypeCode::ID hopeCode) {
    return m_engine->doGet(m_state, hopeCode);
}

bool Archive::getPointer() {
    return m_engine->getPointer(m_state);
}

bool Archive::getObject() {
    return m_engine->getObject(m_state);
}

void Archive::getClassAndModuleList() {
    m_engine->getClassAndModuleList(m_state);
}

bool Archive::check_version() {
    return m_engine->check_version(m_state);
}
