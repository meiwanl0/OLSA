#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ArchiveEngine.h"
#include "ArchiveHook.h"

class Archive final {
public:
    explicit Archive(
        std::unique_ptr<IArchiveEngine> engine,
        std::shared_ptr<ICountable> counter = make_countable(),
        std::shared_ptr<IArchiveHook> hook = std::make_shared<NullArchiveHook>());

    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;
    Archive(Archive&&) noexcept = default;
    Archive& operator=(Archive&&) noexcept = default;
    ~Archive() = default;

    [[nodiscard]] ArchiveState& state() noexcept;
    [[nodiscard]] const ArchiveState& state() const noexcept;

    void reset_input(std::vector<std::uint8_t> buffer, std::uint32_t offset = 0, bool finalized = true);
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

private:
    std::shared_ptr<IArchiveHook> m_hook;
    ArchiveState m_state;
    std::unique_ptr<IArchiveEngine> m_engine;
};
