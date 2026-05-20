#ifndef ELIAS_HPP
#define ELIAS_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class BitWriter {
public:
    void write_bit(bool bit) {
        if (bit_count_ % 8 == 0) {
            bytes_.push_back(0);
        }

        if (bit) {
            bytes_.back() |= static_cast<uint8_t>(1u << (7 - (bit_count_ % 8)));
        }
        ++bit_count_;
    }

    void write_bits(uint64_t value, int count) {
        for (int i = count - 1; i >= 0; --i) {
            write_bit(((value >> i) & 1ULL) != 0);
        }
    }

    size_t bit_count() const {
        return bit_count_;
    }

    const std::vector<uint8_t>& bytes() const {
        return bytes_;
    }

private:
    std::vector<uint8_t> bytes_;
    size_t bit_count_ = 0;
};

class BitReader {
public:
    BitReader(const std::vector<uint8_t>& bytes, size_t bit_count, size_t offset = 0)
        : bytes_(bytes), bit_count_(bit_count), offset_(offset) {}

    bool read_bit() {
        if (offset_ >= bit_count_) {
            throw std::out_of_range("Lectura fuera del bitstream");
        }

        bool bit = ((bytes_[offset_ / 8] >> (7 - (offset_ % 8))) & 1U) != 0;
        ++offset_;
        return bit;
    }

    uint64_t read_bits(int count) {
        uint64_t value = 0;
        for (int i = 0; i < count; ++i) {
            value = (value << 1) | (read_bit() ? 1ULL : 0ULL);
        }
        return value;
    }

    size_t offset() const {
        return offset_;
    }

private:
    const std::vector<uint8_t>& bytes_;
    size_t bit_count_;
    size_t offset_;
};

inline int bit_length(uint64_t value) {
    int length = 0;
    do {
        ++length;
        value >>= 1;
    } while (value != 0);
    return length;
}

inline void write_gamma(BitWriter& writer, uint64_t value) {
    if (value == 0) {
        throw std::invalid_argument("Elias gamma solo codifica enteros positivos");
    }

    int length = bit_length(value);
    for (int i = 0; i < length - 1; ++i) {
        writer.write_bit(false);
    }
    writer.write_bits(value, length);
}

inline uint64_t read_gamma(BitReader& reader) {
    int zeros = 0;
    while (!reader.read_bit()) {
        ++zeros;
    }

    uint64_t suffix = zeros == 0 ? 0 : reader.read_bits(zeros);
    return (1ULL << zeros) | suffix;
}

inline void write_delta(BitWriter& writer, uint64_t value) {
    if (value == 0) {
        throw std::invalid_argument("Elias delta solo codifica enteros positivos");
    }

    int length = bit_length(value);
    write_gamma(writer, static_cast<uint64_t>(length));
    writer.write_bits(value ^ (1ULL << (length - 1)), length - 1);
}

inline uint64_t read_delta(BitReader& reader) {
    uint64_t length = read_gamma(reader);
    uint64_t suffix = length <= 1 ? 0 : reader.read_bits(static_cast<int>(length - 1));
    return (1ULL << (length - 1)) | suffix;
}

enum class EliasCodec {
    Gamma,
    Delta
};

inline std::string elias_codec_name(EliasCodec codec) {
    return codec == EliasCodec::Gamma ? "elias_gamma" : "elias_delta";
}

class EliasCompressedGaps {
public:
    EliasCompressedGaps(
        const std::vector<uint64_t>& values,
        EliasCodec codec,
        size_t sample_step = 0
    ) : codec_(codec) {
        build(values, sample_step);
    }

    std::optional<size_t> search(uint64_t target) const {
        if (size_ == 0) {
            return std::nullopt;
        }

        auto it = std::upper_bound(sample_values_.begin(), sample_values_.end(), target);
        size_t sample_index = it == sample_values_.begin()
            ? 0
            : static_cast<size_t>((it - sample_values_.begin()) - 1);

        size_t start = sample_positions_[sample_index];
        size_t end = sample_index + 1 < sample_positions_.size()
            ? sample_positions_[sample_index + 1]
            : size_;

        BitReader reader(bytes_, bit_count_, sample_bit_offsets_[sample_index]);
        uint64_t current = sample_bases_[sample_index];

        for (size_t i = start; i < end; ++i) {
            uint64_t encoded_gap = read_code(reader);
            uint64_t gap = encoded_gap - 1;
            current += gap;

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
        return bytes_.capacity() * sizeof(uint8_t)
            + sample_positions_.capacity() * sizeof(size_t)
            + sample_values_.capacity() * sizeof(uint64_t)
            + sample_bases_.capacity() * sizeof(uint64_t)
            + sample_bit_offsets_.capacity() * sizeof(size_t);
    }

    size_t bit_count() const {
        return bit_count_;
    }

    size_t size() const {
        return size_;
    }

    size_t sample_step() const {
        return sample_step_;
    }

private:
    void build(const std::vector<uint64_t>& values, size_t sample_step) {
        size_ = values.size();
        if (values.empty()) {
            sample_step_ = 1;
            return;
        }

        sample_step_ = sample_step == 0
            ? std::max<size_t>(1, static_cast<size_t>(std::sqrt(values.size())))
            : sample_step;

        sample_positions_.reserve((values.size() + sample_step_ - 1) / sample_step_);
        sample_values_.reserve(sample_positions_.capacity());
        sample_bases_.reserve(sample_positions_.capacity());
        sample_bit_offsets_.reserve(sample_positions_.capacity());

        BitWriter writer;
        uint64_t previous = 0;

        for (size_t i = 0; i < values.size(); ++i) {
            if (i % sample_step_ == 0) {
                sample_positions_.push_back(i);
                sample_values_.push_back(values[i]);
                sample_bases_.push_back(i == 0 ? 0 : previous);
                sample_bit_offsets_.push_back(writer.bit_count());
            }

            uint64_t gap = i == 0 ? values[i] : values[i] - previous;
            write_code(writer, gap + 1);
            previous = values[i];
        }

        bytes_ = writer.bytes();
        bit_count_ = writer.bit_count();
    }

    void write_code(BitWriter& writer, uint64_t value) const {
        if (codec_ == EliasCodec::Gamma) {
            write_gamma(writer, value);
        } else {
            write_delta(writer, value);
        }
    }

    uint64_t read_code(BitReader& reader) const {
        return codec_ == EliasCodec::Gamma ? read_gamma(reader) : read_delta(reader);
    }

    EliasCodec codec_;
    std::vector<uint8_t> bytes_;
    std::vector<size_t> sample_positions_;
    std::vector<uint64_t> sample_values_;
    std::vector<uint64_t> sample_bases_;
    std::vector<size_t> sample_bit_offsets_;
    size_t bit_count_ = 0;
    size_t size_ = 0;
    size_t sample_step_ = 1;
};

#endif
