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

using Reloj = std::chrono::high_resolution_clock;

struct ResultadoTiempo {
    double milisegundos;
};

template <typename Fn>
ResultadoTiempo medir(Fn&& fn) {
    auto inicio = Reloj::now();
    fn();
    auto fin = Reloj::now();
    std::chrono::duration<double, std::milli> transcurrido = fin - inicio;
    return {transcurrido.count()};
}

inline double milisegundos_desde(Reloj::time_point inicio) {
    auto fin = Reloj::now();
    std::chrono::duration<double, std::milli> transcurrido = fin - inicio;
    return transcurrido.count();
}

template <typename Estructura>
double medir_busquedas(const Estructura& estructura, const std::vector<uint64_t>& consultas) {
    size_t encontrados = 0;
    auto resultado = medir([&]() {
        for (uint64_t consulta : consultas) {
            if (estructura.buscar(consulta).has_value()) {
                ++encontrados;
            }
        }
    });

    volatile size_t mantener_resultado = encontrados;
    (void)mantener_resultado;
    return resultado.milisegundos;
}

std::vector<uint64_t> construir_consultas(
    const std::vector<uint64_t>& datos,
    size_t cantidad_consultas,
    uint64_t semilla
) {
    std::vector<uint64_t> consultas;
    consultas.reserve(cantidad_consultas);

    if (datos.empty()) {
        return consultas;
    }

    std::mt19937_64 rng(semilla);
    std::uniform_int_distribution<size_t> dist_indice(0, datos.size() - 1);
    std::uniform_int_distribution<uint64_t> dist_valor(datos.front(), datos.back() + 100);

    for (size_t i = 0; i < cantidad_consultas; ++i) {
        consultas.push_back(i % 2 == 0 ? datos[dist_indice(rng)] : dist_valor(rng));
    }

    return consultas;
}

void escribir_metrica(
    std::ofstream& salida,
    const std::string& distribucion,
    size_t n,
    const std::string& estructura,
    double construccion_ms,
    double busqueda_ms,
    size_t bytes,
    size_t bits,
    size_t consultas
) {
    salida << distribucion << ','
           << n << ','
           << estructura << ','
           << construccion_ms << ','
           << busqueda_ms << ','
           << bytes << ','
           << bits << ','
           << consultas << '\n';
}

void ejecutar_benchmark(
    const std::string& ruta_salida,
    const std::vector<size_t>& tamanos,
    size_t cantidad_consultas
) {
    std::ofstream salida(ruta_salida);
    if (!salida) {
        throw std::runtime_error("No se pudo crear el archivo de salida: " + ruta_salida);
    }

    salida << "distribucion,n,estructura,construccion_ms,busqueda_ms,bytes,bits,busquedas\n";

    uint64_t semilla = 1452026;
    struct CasoBenchmark {
        Distribucion distribucion;
        uint32_t gap_maximo;
        double desviacion;
    };

    std::vector<CasoBenchmark> casos = {
        {Distribucion::Lineal, 100, 0.0},
        {Distribucion::Normal, 0, 30.0},
    };

    for (const CasoBenchmark& caso : casos) {
        for (size_t n : tamanos) {
            std::vector<uint64_t> datos = caso.distribucion == Distribucion::Lineal
                ? generar_datos_lineales(n, caso.gap_maximo, semilla++)
                : generar_datos_normales(n, 50.0, caso.desviacion, semilla++);
            std::vector<uint64_t> consultas = construir_consultas(datos, cantidad_consultas, semilla++);
            size_t paso_muestra = std::max<size_t>(1, static_cast<size_t>(std::sqrt(n)));

            {
                auto inicio = Reloj::now();
                ArregloExplicito arreglo_explicito(datos);
                double tiempo_construccion = milisegundos_desde(inicio);
                double tiempo_busqueda = medir_busquedas(arreglo_explicito, consultas);
                escribir_metrica(
                    salida,
                    nombre_distribucion(caso.distribucion),
                    n,
                    "arreglo_original",
                    tiempo_construccion,
                    tiempo_busqueda,
                    arreglo_explicito.bytes_usados(),
                    arreglo_explicito.bytes_usados() * 8,
                    cantidad_consultas
                );
            }

            {
                auto inicio = Reloj::now();
                CodificacionGap codificacion_gap(datos, paso_muestra);
                double tiempo_construccion = milisegundos_desde(inicio);
                double tiempo_busqueda = medir_busquedas(codificacion_gap, consultas);
                escribir_metrica(
                    salida,
                    nombre_distribucion(caso.distribucion),
                    n,
                    "gap_coding",
                    tiempo_construccion,
                    tiempo_busqueda,
                    codificacion_gap.bytes_usados(),
                    codificacion_gap.bytes_usados() * 8,
                    cantidad_consultas
                );
            }

            for (CodecElias codec : {CodecElias::Gamma, CodecElias::Delta}) {
                auto inicio = Reloj::now();
                GapsComprimidosElias comprimido(datos, codec, paso_muestra);
                double tiempo_construccion = milisegundos_desde(inicio);
                double tiempo_busqueda = medir_busquedas(comprimido, consultas);
                escribir_metrica(
                    salida,
                    nombre_distribucion(caso.distribucion),
                    n,
                    nombre_codec_elias(codec),
                    tiempo_construccion,
                    tiempo_busqueda,
                    comprimido.bytes_usados(),
                    comprimido.cantidad_bits(),
                    cantidad_consultas
                );
            }

            std::cout << "Benchmark listo: " << nombre_distribucion(caso.distribucion)
                      << " n=" << n << '\n';
        }
    }

    std::cout << "Metricas guardadas en " << ruta_salida << '\n';
}

void ejecutar_modo_archivo(const std::string& ruta_entrada) {
    std::vector<uint64_t> datos = leer_numeros_csv(ruta_entrada);
    if (datos.empty()) {
        throw std::runtime_error("El archivo no contiene numeros validos");
    }

    size_t paso_muestra = std::max<size_t>(1, static_cast<size_t>(std::sqrt(datos.size())));
    ArregloExplicito arreglo_explicito(datos);
    CodificacionGap codificacion_gap(datos, paso_muestra);
    GapsComprimidosElias gamma(datos, CodecElias::Gamma, paso_muestra);
    GapsComprimidosElias delta(datos, CodecElias::Delta, paso_muestra);

    std::cout << "Datos cargados: " << datos.size() << " valores ordenados.\n";
    std::cout << "Estructuras: arreglo_original, gap, gamma, delta. Escriba salir para terminar.\n";

    while (true) {
        std::string estructura;
        std::cout << "\nEstructura> ";
        if (!(std::cin >> estructura) || estructura == "salir" || estructura == "exit") {
            break;
        }

        uint64_t objetivo = 0;
        std::cout << "Valor> ";
        if (!(std::cin >> objetivo)) {
            break;
        }

        std::optional<size_t> posicion;
        double tiempo_busqueda = 0.0;

        if (estructura == "arreglo_original" || estructura == "explicit") {
            tiempo_busqueda = medir([&]() { posicion = arreglo_explicito.buscar(objetivo); }).milisegundos;
        } else if (estructura == "gap") {
            tiempo_busqueda = medir([&]() { posicion = codificacion_gap.buscar(objetivo); }).milisegundos;
        } else if (estructura == "gamma") {
            tiempo_busqueda = medir([&]() { posicion = gamma.buscar(objetivo); }).milisegundos;
        } else if (estructura == "delta") {
            tiempo_busqueda = medir([&]() { posicion = delta.buscar(objetivo); }).milisegundos;
        } else {
            std::cout << "Estructura no reconocida.\n";
            continue;
        }

        if (posicion.has_value()) {
            std::cout << "Encontrado en posicion " << *posicion;
        } else {
            std::cout << "No encontrado";
        }
        std::cout << " (" << tiempo_busqueda << " ms)\n";
    }
}

std::string valor_argumento(int argc, char** argv, const std::string& clave, const std::string& defecto) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == clave) {
            return argv[i + 1];
        }
    }
    return defecto;
}

std::vector<size_t> parsear_tamanos(const std::string& texto) {
    std::vector<size_t> tamanos;
    std::stringstream flujo(texto);
    std::string token;

    while (std::getline(flujo, token, ',')) {
        if (!token.empty()) {
            tamanos.push_back(static_cast<size_t>(std::stoull(token)));
        }
    }

    if (tamanos.empty()) {
        throw std::runtime_error("La lista de tamanos esta vacia");
    }

    return tamanos;
}

bool tiene_argumento(int argc, char** argv, const std::string& clave) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == clave) {
            return true;
        }
    }
    return false;
}

void imprimir_uso() {
    std::cout << "Uso:\n"
              << "  ./main --benchmark [--output metricas.csv] [--queries 1000] [--sizes 10000,100000,1000000]\n"
              << "  ./main -i archivo.csv\n";
}

int main(int argc, char** argv) {
    try {
        if (argc <= 1) {
            imprimir_uso();
            return 0;
        }



        if (tiene_argumento(argc, argv, "--benchmark")) {
            std::string salida = valor_argumento(argc, argv, "--output", "benchmark_resultados.csv");
            size_t consultas = static_cast<size_t>(std::stoull(valor_argumento(argc, argv, "--queries", "1000")));
            std::vector<size_t> tamanos = parsear_tamanos(valor_argumento(argc, argv, "--sizes", "10000,100000,1000000"));
            ejecutar_benchmark(salida, tamanos, consultas);
            return 0;
        }



        std::string entrada = valor_argumento(argc, argv, "-i", "");
        if (!entrada.empty()) {
            ejecutar_modo_archivo(entrada);
            return 0;
        }

        imprimir_uso();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
