#pragma once

#include <cstddef>
#include <memory>

class ICountable {
public:
    virtual ~ICountable() = default;
    virtual void add(std::size_t delta) = 0;
    virtual std::size_t value() const = 0;
    virtual void reset() = 0;
};

class Countable final : public ICountable {
public:
    void add(std::size_t delta) override;
    std::size_t value() const override;
    void reset() override;

private:
    std::size_t m_value{};
};

[[nodiscard]] std::shared_ptr<ICountable> make_countable();

