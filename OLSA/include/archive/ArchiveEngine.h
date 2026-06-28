#pragma once

#include <cstddef>
#include <string>

#include "ArchiveState.h"

class IArchiveEngine {
public:
    virtual ~IArchiveEngine() = default;
    virtual bool get(ArchiveState& state, TypeCode::ID hopeCode, std::size_t hopeReadSize) = 0;
    virtual bool gets(ArchiveState& state, std::string& str, std::size_t maxSize) = 0;
    virtual void getClassAndModuleList(ArchiveState& state) = 0;
    virtual bool getPointer(ArchiveState& state) = 0;
    virtual bool getObject(ArchiveState& state) = 0;
    virtual bool check_version(ArchiveState& state) = 0;
    virtual ItemHeader next(ArchiveState& state) = 0;
    virtual bool doGet(ArchiveState& state, TypeCode::ID hopeCode) = 0;
};

