#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "../archive/Compress.h"
#include "../archive/Countable.h"
#include "../compat/ftml_core.h"

struct ArchiveState {
    struct Dictionaries {
        std::vector<ArchiveUtil::ClassItem> classList;
        std::vector<ArchiveUtil::ModuleItem> moduleList;
        std::unordered_map<std::uint64_t, std::uint32_t> taggedObjectIndex;
        std::unordered_map<std::uint32_t, std::size_t> classIndexById;
        std::unordered_map<std::uint32_t, std::size_t> moduleIndexById;
    } dictionaries;

    struct Input {
        std::vector<std::uint8_t> buffer;
        std::uint32_t offset{};
        bool finalized{};
        std::shared_ptr<ICountable> counter;
    } input;

    struct Parsing {
        ItemHeader itemHeader{};
        Compress compress;
        std::optional<OLSA::Container::ResolvedTypeInfo> resolvedType;
    } parsing;
};
