#include "../include/TextArchive.h"

namespace {
class TextArchiveEngine final : public IArchiveEngine {
public:
    bool get(ArchiveState&, TypeCode::ID, std::size_t) override { return false; }
    bool gets(ArchiveState&, std::string&, std::size_t) override { return false; }
    void getClassAndModuleList(ArchiveState&) override {}
    bool getPointer(ArchiveState&) override { return false; }
    bool getObject(ArchiveState&) override { return false; }
    bool check_version(ArchiveState&) override { return false; }
    ItemHeader next(ArchiveState& state) override { return state.parsing.itemHeader; }
    bool doGet(ArchiveState&, TypeCode::ID) override { return false; }
};

[[nodiscard]] std::unique_ptr<IArchiveEngine> make_text_engine() {
    return std::make_unique<TextArchiveEngine>();
}
}  // namespace

TextArchive::TextArchive(std::shared_ptr<ICountable> counter, std::shared_ptr<IArchiveHook> hook)
    : m_archive(make_text_engine(), std::move(counter), std::move(hook)) {}

Archive& TextArchive::archive() noexcept {
    return m_archive;
}

const Archive& TextArchive::archive() const noexcept {
    return m_archive;
}

