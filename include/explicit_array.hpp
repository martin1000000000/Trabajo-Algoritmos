#ifndef EXPLICIT_ARRAY_HPP
#define EXPLICIT_ARRAY_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class ArregloExplicito {
public:
    explicit ArregloExplicito(std::vector<uint64_t> valores)
        : valores_(std::move(valores)) {}

    std::optional<size_t> buscar(uint64_t objetivo) const {
        auto it = std::lower_bound(valores_.begin(), valores_.end(), objetivo);
        if (it == valores_.end() || *it != objetivo) {
            return std::nullopt;
        }
        return static_cast<size_t>(it - valores_.begin());
    }

    size_t tamano() const {
        return valores_.size();
    }

    size_t bytes_usados() const {
        return valores_.capacity() * sizeof(uint64_t);
    }

private:
    std::vector<uint64_t> valores_;
};

#endif
