#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../archive/Countable.h"
#include "ArchiveFacade.h"

class BinaryArchive final {
public:
    explicit BinaryArchive(
        std::shared_ptr<ICountable> counter = make_countable(),
        std::shared_ptr<IArchiveHook> hook = std::make_shared<NullArchiveHook>());

    [[nodiscard]] Archive& archive() noexcept;
    [[nodiscard]] const Archive& archive() const noexcept;

    void reset(std::vector<std::uint8_t> buffer, std::uint32_t offset = 0, bool finalized = true);
    [[nodiscard]] ItemHeader next();
    bool get(TypeCode::ID hopeCode, std::size_t hopeReadSize);
    bool gets(std::string& str, std::size_t maxSize);
    Archive& begin();
    bool end();
    bool doGet(TypeCode::ID hopeCode);
    bool getPointer();
    bool getObject();
    void getClassAndModuleList();
    bool check_version();
    void read_first_section();

private:
    Archive m_archive;
};
