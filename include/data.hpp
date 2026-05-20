#ifndef DATA_HPP
#define DATA_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

enum class Distribution {
    Linear,
    Normal
};

inline std::string distribution_name(Distribution distribution) {
    return distribution == Distribution::Linear ? "linear" : "normal";
}

inline std::vector<uint64_t> generate_linear_data(
    size_t n,
    uint32_t max_gap,
    uint64_t seed
) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> gap_dist(0, max_gap);

    std::vector<uint64_t> data;
    data.reserve(n);

    uint64_t current = gap_dist(rng);
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) {
            current += gap_dist(rng);
        }
        data.push_back(current);
    }

    return data;
}

inline std::vector<uint64_t> generate_normal_data(
    size_t n,
    double mean_gap,
    double stddev,
    uint64_t seed
) {
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> gap_dist(mean_gap, stddev);

    std::vector<uint64_t> data;
    data.reserve(n);

    uint64_t current = 0;
    for (size_t i = 0; i < n; ++i) {
        double raw_gap = gap_dist(rng);
        uint64_t gap = raw_gap <= 0.0 ? 0ULL : static_cast<uint64_t>(raw_gap);
        current += gap;
        data.push_back(current);
    }

    return data;
}

inline std::vector<uint64_t> generate_data(
    size_t n,
    Distribution distribution,
    uint64_t seed,
    double stddev = 25.0
) {
    if (distribution == Distribution::Linear) {
        return generate_linear_data(n, 100, seed);
    }

    return generate_normal_data(n, 50.0, stddev, seed);
}

inline std::vector<uint64_t> read_csv_numbers(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("No se pudo abrir el archivo: " + path);
    }

    std::vector<uint64_t> values;
    std::string token;
    char ch = '\0';

    while (input.get(ch)) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            token.push_back(ch);
        } else if (!token.empty()) {
            values.push_back(std::stoull(token));
            token.clear();
        }
    }

    if (!token.empty()) {
        values.push_back(std::stoull(token));
    }

    std::sort(values.begin(), values.end());
    return values;
}

#endif
