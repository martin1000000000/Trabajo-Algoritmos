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

enum class Distribucion {
    Lineal,
    Normal
};

inline std::string nombre_distribucion(Distribucion distribucion) {
    return distribucion == Distribucion::Lineal ? "linear" : "normal";
}

inline std::vector<uint64_t> generar_datos_lineales(
    size_t n,
    uint32_t gap_maximo,
    uint64_t semilla
) {
    std::mt19937_64 rng(semilla);
    std::uniform_int_distribution<uint32_t> dist_gap(0, gap_maximo);

    std::vector<uint64_t> datos;
    datos.reserve(n);

    uint64_t actual = dist_gap(rng);
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) {
            actual += dist_gap(rng);
        }
        datos.push_back(actual);
    }

    return datos;
}

inline std::vector<uint64_t> generar_datos_normales(
    size_t n,
    double media_gap,
    double desviacion,
    uint64_t semilla
) {
    std::mt19937_64 rng(semilla);
    std::normal_distribution<double> dist_gap(media_gap, desviacion);

    std::vector<uint64_t> datos;
    datos.reserve(n);

    uint64_t actual = 0;
    for (size_t i = 0; i < n; ++i) {
        double gap_crudo = dist_gap(rng);
        uint64_t gap = gap_crudo <= 0.0 ? 0ULL : static_cast<uint64_t>(gap_crudo);
        actual += gap;
        datos.push_back(actual);
    }

    return datos;
}

inline std::vector<uint64_t> generar_datos(
    size_t n,
    Distribucion distribucion,
    uint64_t semilla,
    double desviacion = 25.0
) {
    if (distribucion == Distribucion::Lineal) {
        return generar_datos_lineales(n, 100, semilla);
    }

    return generar_datos_normales(n, 50.0, desviacion, semilla);
}

inline std::vector<uint64_t> leer_numeros_csv(const std::string& ruta) {
    std::ifstream entrada(ruta);
    if (!entrada) {
        throw std::runtime_error("No se pudo abrir el archivo: " + ruta);
    }

    std::vector<uint64_t> valores;
    std::string token;
    char ch = '\0';

    while (entrada.get(ch)) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            token.push_back(ch);
        } else if (!token.empty()) {
            valores.push_back(std::stoull(token));
            token.clear();
        }
    }

    if (!token.empty()) {
        valores.push_back(std::stoull(token));
    }

    std::sort(valores.begin(), valores.end());
    return valores;
}

#endif
