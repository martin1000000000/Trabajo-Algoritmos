#include "include/data.hpp"
#include "include/elias.hpp"
#include "include/explicit_array.hpp"
#include "include/gap_coding.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

struct TimerResult {
    double milliseconds;
};

template <typename Fn>
TimerResult measure(Fn&& fn) {
    auto start = Clock::now();
    fn();
    auto end = Clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return {elapsed.count()};
}

inline double milliseconds_since(Clock::time_point start) {
    auto end = Clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

template <typename Structure>
double measure_searches(const Structure& structure, const std::vector<uint64_t>& queries) {
    size_t found = 0;
    auto result = measure([&]() {
        for (uint64_t query : queries) {
            if (structure.search(query).has_value()) {
                ++found;
            }
        }
    });

    volatile size_t keep_result = found;
    (void)keep_result;
    return result.milliseconds;
}

std::string bits_from_writer(const BitWriter& writer) {
    std::string bits;
    bits.reserve(writer.bit_count());

    const std::vector<uint8_t>& bytes = writer.bytes();
    for (size_t i = 0; i < writer.bit_count(); ++i) {
        bool bit = ((bytes[i / 8] >> (7 - (i % 8))) & 1U) != 0;
        bits.push_back(bit ? '1' : '0');
    }

    return bits;
}

std::string gamma_bits(uint64_t value) {
    BitWriter writer;
    write_gamma(writer, value);
    return bits_from_writer(writer);
}

std::string delta_bits(uint64_t value) {
    BitWriter writer;
    write_delta(writer, value);
    return bits_from_writer(writer);
}

void run_demo() {
    std::vector<uint64_t> values = {2, 7, 10, 12, 12, 16};
    std::vector<uint64_t> gaps;
    gaps.reserve(values.size());

    uint64_t previous = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        uint64_t gap = i == 0 ? values[i] : values[i] - previous;
        gaps.push_back(gap);
        previous = values[i];
    }

    std::cout << "Demo con arreglo pequeno\n";
    std::cout << "A: ";
    for (uint64_t value : values) {
        std::cout << value << ' ';
    }
    std::cout << "\nGaps: ";
    for (uint64_t gap : gaps) {
        std::cout << gap << ' ';
    }
    std::cout << "\n\n";

    std::cout << "Elias codifica enteros positivos, por eso usamos gap + 1.\n";
    std::cout << "gap,gap+1,gamma,bits_gamma,delta,bits_delta\n";

    BitWriter gamma_stream;
    BitWriter delta_stream;
    for (uint64_t gap : gaps) {
        uint64_t encoded_value = gap + 1;
        std::string gamma = gamma_bits(encoded_value);
        std::string delta = delta_bits(encoded_value);

        write_gamma(gamma_stream, encoded_value);
        write_delta(delta_stream, encoded_value);

        std::cout << gap << ','
                  << encoded_value << ','
                  << gamma << ','
                  << gamma.size() << ','
                  << delta << ','
                  << delta.size() << '\n';
    }

    std::cout << "\nBitstream gamma completo: " << bits_from_writer(gamma_stream) << '\n';
    std::cout << "Bitstream delta completo: " << bits_from_writer(delta_stream) << '\n';
}

std::vector<uint64_t> build_queries(
    const std::vector<uint64_t>& data,
    size_t query_count,
    uint64_t seed
) {
    std::vector<uint64_t> queries;
    queries.reserve(query_count);

    if (data.empty()) {
        return queries;
    }

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> index_dist(0, data.size() - 1);
    std::uniform_int_distribution<uint64_t> value_dist(data.front(), data.back() + 100);

    for (size_t i = 0; i < query_count; ++i) {
        queries.push_back(i % 2 == 0 ? data[index_dist(rng)] : value_dist(rng));
    }

    return queries;
}

void write_metric(
    std::ofstream& output,
    const std::string& distribution,
    size_t n,
    const std::string& structure,
    double build_ms,
    double search_ms,
    size_t bytes,
    size_t bits,
    size_t queries
) {
    output << distribution << ','
           << n << ','
           << structure << ','
           << build_ms << ','
           << search_ms << ','
           << bytes << ','
           << bits << ','
           << queries << '\n';
}

void run_benchmark(
    const std::string& output_path,
    const std::vector<size_t>& sizes,
    size_t query_count
) {
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("No se pudo crear el archivo de salida: " + output_path);
    }

    output << "distribution,n,structure,build_ms,search_ms,bytes,bits,queries\n";

    uint64_t seed = 1452026;
    for (Distribution distribution : {Distribution::Linear, Distribution::Normal}) {
        for (size_t n : sizes) {
            std::vector<uint64_t> data = generate_data(n, distribution, seed++, 30.0);
            std::vector<uint64_t> queries = build_queries(data, query_count, seed++);
            size_t sample_step = std::max<size_t>(1, static_cast<size_t>(std::sqrt(n)));

            {
                auto start = Clock::now();
                ExplicitArray explicit_array(data);
                double explicit_build = milliseconds_since(start);
                double explicit_search = measure_searches(explicit_array, queries);
                write_metric(
                    output,
                    distribution_name(distribution),
                    n,
                    "explicit",
                    explicit_build,
                    explicit_search,
                    explicit_array.bytes_used(),
                    explicit_array.bytes_used() * 8,
                    query_count
                );
            }

            {
                auto start = Clock::now();
                GapCoding gap_coding(data, sample_step);
                double gap_build = milliseconds_since(start);
                double gap_search = measure_searches(gap_coding, queries);
                write_metric(
                    output,
                    distribution_name(distribution),
                    n,
                    "gap_coding",
                    gap_build,
                    gap_search,
                    gap_coding.bytes_used(),
                    gap_coding.bytes_used() * 8,
                    query_count
                );
            }

            for (EliasCodec codec : {EliasCodec::Gamma, EliasCodec::Delta}) {
                auto start = Clock::now();
                EliasCompressedGaps compressed(data, codec, sample_step);
                double build_ms = milliseconds_since(start);
                double search_ms = measure_searches(compressed, queries);
                write_metric(
                    output,
                    distribution_name(distribution),
                    n,
                    elias_codec_name(codec),
                    build_ms,
                    search_ms,
                    compressed.bytes_used(),
                    compressed.bit_count(),
                    query_count
                );
            }

            std::cout << "Benchmark listo: " << distribution_name(distribution)
                      << " n=" << n << '\n';
        }
    }

    std::cout << "Metricas guardadas en " << output_path << '\n';
}

void run_file_mode(const std::string& input_path) {
    std::vector<uint64_t> data = read_csv_numbers(input_path);
    if (data.empty()) {
        throw std::runtime_error("El archivo no contiene numeros validos");
    }

    size_t sample_step = std::max<size_t>(1, static_cast<size_t>(std::sqrt(data.size())));
    ExplicitArray explicit_array(data);
    GapCoding gap_coding(data, sample_step);
    EliasCompressedGaps gamma(data, EliasCodec::Gamma, sample_step);
    EliasCompressedGaps delta(data, EliasCodec::Delta, sample_step);

    std::cout << "Datos cargados: " << data.size() << " valores ordenados.\n";
    std::cout << "Estructuras: explicit, gap, gamma, delta. Escriba salir para terminar.\n";

    while (true) {
        std::string structure;
        std::cout << "\nEstructura> ";
        if (!(std::cin >> structure) || structure == "salir" || structure == "exit") {
            break;
        }

        uint64_t target = 0;
        std::cout << "Valor> ";
        if (!(std::cin >> target)) {
            break;
        }

        std::optional<size_t> position;
        double search_ms = 0.0;

        if (structure == "explicit") {
            search_ms = measure([&]() { position = explicit_array.search(target); }).milliseconds;
        } else if (structure == "gap") {
            search_ms = measure([&]() { position = gap_coding.search(target); }).milliseconds;
        } else if (structure == "gamma") {
            search_ms = measure([&]() { position = gamma.search(target); }).milliseconds;
        } else if (structure == "delta") {
            search_ms = measure([&]() { position = delta.search(target); }).milliseconds;
        } else {
            std::cout << "Estructura no reconocida.\n";
            continue;
        }

        if (position.has_value()) {
            std::cout << "Encontrado en posicion " << *position;
        } else {
            std::cout << "No encontrado";
        }
        std::cout << " (" << search_ms << " ms)\n";
    }
}

std::string arg_value(int argc, char** argv, const std::string& key, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == key) {
            return argv[i + 1];
        }
    }
    return fallback;
}

std::vector<size_t> parse_sizes(const std::string& text) {
    std::vector<size_t> sizes;
    std::stringstream stream(text);
    std::string token;

    while (std::getline(stream, token, ',')) {
        if (!token.empty()) {
            sizes.push_back(static_cast<size_t>(std::stoull(token)));
        }
    }

    if (sizes.empty()) {
        throw std::runtime_error("La lista de tamanos esta vacia");
    }

    return sizes;
}

bool has_arg(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == key) {
            return true;
        }
    }
    return false;
}

void print_usage() {
    std::cout << "Uso:\n"
              << "  ./main --benchmark [--output metricas.csv] [--queries 1000] [--sizes 10000,100000,1000000]\n"
              << "  ./main --demo\n"
              << "  ./main -i archivo.csv\n";
}

int main(int argc, char** argv) {
    try {
        if (argc <= 1) {
            print_usage();
            return 0;
        }

        if (has_arg(argc, argv, "--demo")) {
            run_demo();
            return 0;
        }

        if (has_arg(argc, argv, "--benchmark")) {
            std::string output = arg_value(argc, argv, "--output", "benchmark_results.csv");
            size_t queries = static_cast<size_t>(std::stoull(arg_value(argc, argv, "--queries", "1000")));
            std::vector<size_t> sizes = parse_sizes(arg_value(argc, argv, "--sizes", "10000,100000,1000000"));
            run_benchmark(output, sizes, queries);
            return 0;
        }

        std::string input = arg_value(argc, argv, "-i", "");
        if (!input.empty()) {
            run_file_mode(input);
            return 0;
        }

        print_usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
