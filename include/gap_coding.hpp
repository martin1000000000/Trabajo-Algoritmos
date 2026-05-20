#ifndef GAP_CODING_HPP
#define GAP_CODING_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

class GapCoding {
public:
    GapCoding(const std::vector<uint64_t>& values, size_t sample_step = 0) {
        build(values, sample_step);
    }

    std::optional<size_t> search(uint64_t target) const {
        if (gaps_.empty()) {
            return std::nullopt;
        }

        auto it = std::upper_bound(sample_values_.begin(), sample_values_.end(), target);
        size_t sample_index = it == sample_values_.begin()
            ? 0
            : static_cast<size_t>((it - sample_values_.begin()) - 1);

        size_t start = sample_positions_[sample_index];
        size_t end = sample_index + 1 < sample_positions_.size()
            ? sample_positions_[sample_index + 1]
            : gaps_.size();

        uint64_t current = sample_bases_[sample_index];
        for (size_t i = start; i < end; ++i) {
            current += gaps_[i];
            if (current == target) {
                return i;
            }
            if (current > target) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    size_t bytes_used() const {
        return gaps_.capacity() * sizeof(uint32_t)
            + sample_positions_.capacity() * sizeof(size_t)
            + sample_values_.capacity() * sizeof(uint64_t)
            + sample_bases_.capacity() * sizeof(uint64_t);
    }

    size_t size() const {
        return gaps_.size();
    }

    size_t sample_step() const {
        return sample_step_;
    }

private:
    void build(const std::vector<uint64_t>& values, size_t sample_step) {
        gaps_.clear();
        sample_positions_.clear();
        sample_values_.clear();
        sample_bases_.clear();

        if (values.empty()) {
            sample_step_ = 1;
            return;
        }

        sample_step_ = sample_step == 0
            ? std::max<size_t>(1, static_cast<size_t>(std::sqrt(values.size())))
            : sample_step;

        gaps_.reserve(values.size());
        sample_positions_.reserve((values.size() + sample_step_ - 1) / sample_step_);
        sample_values_.reserve(sample_positions_.capacity());
        sample_bases_.reserve(sample_positions_.capacity());

        uint64_t previous = 0;
        for (size_t i = 0; i < values.size(); ++i) {
            uint64_t gap = i == 0 ? values[i] : values[i] - previous;
            if (gap > std::numeric_limits<uint32_t>::max()) {
                throw std::overflow_error("Gap-Coding requiere gaps que quepan en uint32_t");
            }
            gaps_.push_back(static_cast<uint32_t>(gap));

            if (i % sample_step_ == 0) {
                sample_positions_.push_back(i);
                sample_values_.push_back(values[i]);
                sample_bases_.push_back(i == 0 ? 0 : previous);
            }

            previous = values[i];
        }
    }

    std::vector<uint32_t> gaps_;
    std::vector<size_t> sample_positions_;
    std::vector<uint64_t> sample_values_;
    std::vector<uint64_t> sample_bases_;
    size_t sample_step_ = 1;
};

#endif
