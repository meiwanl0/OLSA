#include "../include/archive/Countable.h"

void Countable::add(std::size_t delta) {
    m_value += delta;
}

std::size_t Countable::value() const {
    return m_value;
}

void Countable::reset() {
    m_value = 0;
}

std::shared_ptr<ICountable> make_countable() {
    return std::make_shared<Countable>();
}
