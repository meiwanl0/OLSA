#pragma once

#include <cstddef>

#include "ArchiveState.h"

class IArchiveHook {
public:
    virtual ~IArchiveHook() = default;
    virtual void on_next(const ArchiveState& state, const ItemHeader& header) = 0;
    virtual void on_get(const ArchiveState& state, TypeCode::ID hopeCode, std::size_t hopeReadSize) = 0;
};

class NullArchiveHook final : public IArchiveHook {
public:
    void on_next(const ArchiveState&, const ItemHeader&) override {}
    void on_get(const ArchiveState&, TypeCode::ID, std::size_t) override {}
};

