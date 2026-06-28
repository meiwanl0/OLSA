#pragma once

#include <memory>

#include "../archive/Countable.h"
#include "ArchiveFacade.h"

class TextArchive final {
public:
    explicit TextArchive(
        std::shared_ptr<ICountable> counter = make_countable(),
        std::shared_ptr<IArchiveHook> hook = std::make_shared<NullArchiveHook>());

    [[nodiscard]] Archive& archive() noexcept;
    [[nodiscard]] const Archive& archive() const noexcept;

private:
    Archive m_archive;
};
