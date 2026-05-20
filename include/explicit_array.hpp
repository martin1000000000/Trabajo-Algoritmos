#ifndef EXPLICIT_ARRAY_HPP
#define EXPLICIT_ARRAY_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class ExplicitArray {
public:
    explicit ExplicitArray(std::vector<uint64_t> values)
        : values_(std::move(values)) {}

    std::optional<size_t> search(uint64_t target) const {
        auto it = std::lower_bound(values_.begin(), values_.end(), target);
        if (it == values_.end() || *it != target) {
            return std::nullopt;
        }
        return static_cast<size_t>(it - values_.begin());
    }

    size_t size() const {
        return values_.size();
    }

    size_t bytes_used() const {
        return values_.capacity() * sizeof(uint64_t);
    }

private:
    std::vector<uint64_t> values_;
};

#endif
