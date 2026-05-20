#include "include/data.hpp"
#include "include/elias.hpp"
#include "include/explicit_array.hpp"
#include "include/gap_coding.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
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
    const std::string& parameter,
    size_t n,
    const std::string& structure,
    double build_ms,
    double search_ms,
    size_t bytes,
    size_t bits,
    size_t queries
) {
    output << distribution << ','
           << parameter << ','
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

    output << "distribucion,parametro,n,estructura,construccion_ms,busqueda_ms,bytes,bits,busquedas\n";

    uint64_t seed = 1452026;
    struct BenchmarkCase {
        Distribution distribution;
        std::string parameter;
        uint32_t max_gap;
        double stddev;
    };

    std::vector<BenchmarkCase> cases = {
        {Distribution::Linear, "max_gap=10", 10, 0.0},
        {Distribution::Linear, "max_gap=100", 100, 0.0},
        {Distribution::Linear, "max_gap=1000", 1000, 0.0},
        {Distribution::Normal, "stddev=10", 0, 10.0},
        {Distribution::Normal, "stddev=30", 0, 30.0},
        {Distribution::Normal, "stddev=100", 0, 100.0},
    };

    for (const BenchmarkCase& benchmark_case : cases) {
        for (size_t n : sizes) {
            std::vector<uint64_t> data = benchmark_case.distribution == Distribution::Linear
                ? generate_linear_data(n, benchmark_case.max_gap, seed++)
                : generate_normal_data(n, 50.0, benchmark_case.stddev, seed++);
            std::vector<uint64_t> queries = build_queries(data, query_count, seed++);
            size_t sample_step = std::max<size_t>(1, static_cast<size_t>(std::sqrt(n)));

            {
                auto start = Clock::now();
                ExplicitArray explicit_array(data);
                double explicit_build = milliseconds_since(start);
                double explicit_search = measure_searches(explicit_array, queries);
                write_metric(
                    output,
                    distribution_name(benchmark_case.distribution),
                    benchmark_case.parameter,
                    n,
                    "arreglo_original",
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
                    distribution_name(benchmark_case.distribution),
                    benchmark_case.parameter,
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
                    distribution_name(benchmark_case.distribution),
                    benchmark_case.parameter,
                    n,
                    elias_codec_name(codec),
                    build_ms,
                    search_ms,
                    compressed.bytes_used(),
                    compressed.bit_count(),
                    query_count
                );
            }

            std::cout << "Benchmark listo: " << distribution_name(benchmark_case.distribution)
                      << " " << benchmark_case.parameter
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
    std::cout << "Estructuras: arreglo_original, gap, gamma, delta. Escriba salir para terminar.\n";

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

        if (structure == "arreglo_original" || structure == "explicit") {
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

void print_vector_preview(const std::vector<uint64_t>& values, size_t limit = 20) {
    std::cout << '[';
    size_t shown = std::min(values.size(), limit);
    for (size_t i = 0; i < shown; ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << values[i];
    }
    if (values.size() > shown) {
        std::cout << ", ...";
    }
    std::cout << "]\n";
}

std::vector<uint64_t> build_gaps(const std::vector<uint64_t>& values) {
    std::vector<uint64_t> gaps;
    gaps.reserve(values.size());

    uint64_t previous = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        uint64_t gap = i == 0 ? values[i] : values[i] - previous;
        gaps.push_back(gap);
        previous = values[i];
    }

    return gaps;
}

void print_structure_view(
    const std::vector<uint64_t>& values,
    const std::string& structure,
    size_t sample_step
) {
    std::cout << "\nArreglo ordenado (" << values.size() << " valores): ";
    print_vector_preview(values);

    if (structure == "arreglo_original") {
        std::cout << "Vista arreglo_original: se almacena el arreglo completo.\n";
        return;
    }

    std::vector<uint64_t> gaps = build_gaps(values);
    std::cout << "Gaps: ";
    print_vector_preview(gaps);
    std::cout << "Sample step: " << sample_step << '\n';

    if (structure == "gamma" || structure == "delta") {
        std::cout << "Codificacion por gap + 1:\n";
        size_t shown = std::min<size_t>(gaps.size(), 12);
        for (size_t i = 0; i < shown; ++i) {
            uint64_t encoded = gaps[i] + 1;
            std::cout << "  i=" << i
                      << " gap=" << gaps[i]
                      << " valor_codificado=" << encoded
                      << " bits=" << (structure == "gamma" ? gamma_bits(encoded) : delta_bits(encoded))
                      << '\n';
        }
        if (gaps.size() > shown) {
            std::cout << "  ...\n";
        }
    }
}

std::optional<size_t> visual_search_explicit(
    const std::vector<uint64_t>& values,
    uint64_t target
) {
    size_t left = 0;
    size_t right = values.size();
    size_t step = 1;

    while (left < right) {
        size_t middle = left + (right - left) / 2;
        std::cout << "Paso " << step++ << ". rango=["
                  << left << ", " << (right - 1)
                  << "], medio=" << middle
                  << ", valor=" << values[middle] << '\n';

        if (values[middle] < target) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }

    if (left < values.size() && values[left] == target) {
        std::cout << "Paso " << step << ". primer valor >= objetivo esta en posicion "
                  << left << ", valor=" << values[left] << '\n';
        return left;
    }

    return std::nullopt;
}

std::optional<size_t> visual_search_gap(
    const std::vector<uint64_t>& values,
    uint64_t target,
    size_t sample_step
) {
    if (values.empty()) {
        return std::nullopt;
    }

    std::vector<uint64_t> gaps = build_gaps(values);
    std::vector<size_t> samples;
    for (size_t i = 0; i < values.size(); i += sample_step) {
        samples.push_back(i);
    }

    size_t sample_index = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        std::cout << "Paso " << (i + 1)
                  << ". sample posicion=" << samples[i]
                  << ", valor=" << values[samples[i]] << '\n';
        if (values[samples[i]] <= target) {
            sample_index = i;
        } else {
            break;
        }
    }

    size_t start = samples[sample_index];
    size_t end = sample_index + 1 < samples.size() ? samples[sample_index + 1] : values.size();
    uint64_t current = start == 0 ? 0 : values[start - 1];

    std::cout << "Bloque elegido: posiciones " << start << " a " << (end - 1) << '\n';
    for (size_t i = start; i < end; ++i) {
        current += gaps[i];
        std::cout << "  decodifico gap[" << i << "]=" << gaps[i]
                  << " -> valor=" << current << '\n';
        if (current == target) {
            return i;
        }
        if (current > target) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<size_t> visual_search_elias(
    const std::vector<uint64_t>& values,
    uint64_t target,
    EliasCodec codec,
    size_t sample_step
) {
    if (values.empty()) {
        return std::nullopt;
    }

    std::vector<uint64_t> gaps = build_gaps(values);
    size_t sample_index = 0;
    for (size_t i = 0; i < values.size(); i += sample_step) {
        std::cout << "Paso sample. posicion=" << i << ", valor=" << values[i] << '\n';
        if (values[i] <= target) {
            sample_index = i / sample_step;
        } else {
            break;
        }
    }

    size_t start = sample_index * sample_step;
    size_t end = std::min(values.size(), start + sample_step);
    uint64_t current = start == 0 ? 0 : values[start - 1];
    std::string codec_name = codec == EliasCodec::Gamma ? "gamma" : "delta";

    std::cout << "Bloque elegido: posiciones " << start << " a " << (end - 1)
              << " usando Elias " << codec_name << '\n';
    for (size_t i = start; i < end; ++i) {
        uint64_t encoded = gaps[i] + 1;
        std::string bits = codec == EliasCodec::Gamma ? gamma_bits(encoded) : delta_bits(encoded);
        current += gaps[i];
        std::cout << "  gap[" << i << "]=" << gaps[i]
                  << ", guardado como " << encoded
                  << ", bits=" << bits
                  << " -> valor=" << current << '\n';
        if (current == target) {
            return i;
        }
        if (current > target) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

void visual_build(
    const std::vector<uint64_t>& values,
    const std::string& structure,
    size_t sample_step
) {
    std::cout << "\n=== Creacion de estructura ===\n";
    size_t bytes = 0;
    size_t bits = 0;

    auto result = measure([&]() {
        if (structure == "arreglo_original") {
            ExplicitArray explicit_array(values);
            bytes = explicit_array.bytes_used();
            bits = bytes * 8;
        } else if (structure == "gap") {
            GapCoding gap_coding(values, sample_step);
            bytes = gap_coding.bytes_used();
            bits = bytes * 8;
        } else if (structure == "gamma") {
            EliasCompressedGaps compressed(values, EliasCodec::Gamma, sample_step);
            bytes = compressed.bytes_used();
            bits = compressed.bit_count();
        } else if (structure == "delta") {
            EliasCompressedGaps compressed(values, EliasCodec::Delta, sample_step);
            bytes = compressed.bytes_used();
            bits = compressed.bit_count();
        }
    });

    std::cout << "Paso 1. Datos base: ";
    print_vector_preview(values);
    std::cout << "Paso 2. Representacion interna:\n";
    print_structure_view(values, structure, sample_step);
    std::cout << "Paso 3. Tiempo de creacion: "
              << std::fixed << std::setprecision(6)
              << result.milliseconds << " ms\n";
    std::cout << "Paso 4. Espacio utilizado: "
              << bytes << " bytes, " << bits << " bits\n";
}

std::optional<size_t> visual_search(
    const std::vector<uint64_t>& values,
    const std::string& structure,
    uint64_t target,
    size_t sample_step
) {
    if (structure == "arreglo_original") {
        return visual_search_explicit(values, target);
    }
    if (structure == "gap") {
        return visual_search_gap(values, target, sample_step);
    }
    if (structure == "gamma") {
        return visual_search_elias(values, target, EliasCodec::Gamma, sample_step);
    }
    return visual_search_elias(values, target, EliasCodec::Delta, sample_step);
}

std::vector<uint64_t> default_visual_data() {
    return {2, 7, 10, 12, 12, 16, 23, 31, 31, 45};
}

const std::vector<std::string>& visual_structure_order() {
    static const std::vector<std::string> structures = {"arreglo_original", "gap", "gamma", "delta"};
    return structures;
}

std::string visual_structure_title(const std::string& structure) {
    if (structure == "arreglo_original") {
        return "Caso 1: arreglo explicito";
    }
    if (structure == "gap") {
        return "Caso 2: Gap-Coding con sample";
    }
    if (structure == "gamma") {
        return "Caso 3: Elias Gamma sobre gaps";
    }
    return "Caso 3: Elias Delta sobre gaps";
}

void visual_show_all_structures(const std::vector<uint64_t>& data, size_t sample_step) {
    std::cout << "\n==============================\n";
    std::cout << "Flujo de representaciones\n";
    std::cout << "==============================\n";

    for (const std::string& structure : visual_structure_order()) {
        std::cout << "\n--- " << visual_structure_title(structure) << " ---\n";
        visual_build(data, structure, sample_step);
    }
}

void visual_search_all_structures(
    const std::vector<uint64_t>& data,
    uint64_t target,
    size_t sample_step
) {
    std::cout << "\n==============================\n";
    std::cout << "Busqueda del valor " << target << '\n';
    std::cout << "==============================\n";

    for (const std::string& structure : visual_structure_order()) {
        std::cout << "\n--- " << visual_structure_title(structure) << " ---\n";

        std::optional<size_t> position;
        auto result = measure([&]() {
            position = visual_search(data, structure, target, sample_step);
        });

        if (position.has_value()) {
            std::cout << "Resultado: encontrado en posicion " << *position << '\n';
        } else {
            std::cout << "Resultado: no encontrado\n";
        }
        std::cout << "Tiempo de busqueda visualizada: "
                  << std::fixed << std::setprecision(6)
                  << result.milliseconds << " ms\n";
    }
}

void run_visual_mode(const std::string& input_path) {
    std::vector<uint64_t> data = input_path.empty()
        ? default_visual_data()
        : read_csv_numbers(input_path);

    if (data.empty()) {
        throw std::runtime_error("No hay datos para visualizar");
    }

    size_t sample_step = std::max<size_t>(1, static_cast<size_t>(std::sqrt(data.size())));

    std::cout << "Modo visualizar\n";
    std::cout << "Muestra automaticamente: arreglo explicito, gap, gamma y delta.\n";
    visual_show_all_structures(data, sample_step);

    while (true) {
        std::string input;
        std::cout << "\nValor a buscar (o salir)> ";
        if (!(std::cin >> input) || input == "salir" || input == "exit") {
            break;
        }

        uint64_t target = std::stoull(input);
        visual_search_all_structures(data, target, sample_step);
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
              << "  ./main --visualizar [-i archivo.csv]\n"
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
            std::string output = arg_value(argc, argv, "--output", "benchmark_resultados.csv");
            size_t queries = static_cast<size_t>(std::stoull(arg_value(argc, argv, "--queries", "1000")));
            std::vector<size_t> sizes = parse_sizes(arg_value(argc, argv, "--sizes", "10000,100000,1000000"));
            run_benchmark(output, sizes, queries);
            return 0;
        }

        if (has_arg(argc, argv, "--visualizar")) {
            std::string input = arg_value(argc, argv, "-i", "");
            run_visual_mode(input);
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
