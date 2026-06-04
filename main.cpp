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

std::string bits_desde_escritor(const EscritorBits& escritor) {
    std::string bits;
    bits.reserve(escritor.cantidad_bits());

    const std::vector<uint8_t>& bytes = escritor.bytes();
    for (size_t i = 0; i < escritor.cantidad_bits(); ++i) {
        bool bit = ((bytes[i / 8] >> (7 - (i % 8))) & 1U) != 0;
        bits.push_back(bit ? '1' : '0');
    }

    return bits;
}

std::string bits_gamma(uint64_t valor) {
    EscritorBits escritor;
    escribir_gamma(escritor, valor);
    return bits_desde_escritor(escritor);
}

std::string bits_delta(uint64_t valor) {
    EscritorBits escritor;
    escribir_delta(escritor, valor);
    return bits_desde_escritor(escritor);
}

void ejecutar_demo() {
    std::vector<uint64_t> valores = {2, 7, 10, 12, 12, 16};
    std::vector<uint64_t> gaps;
    gaps.reserve(valores.size());

    uint64_t anterior = 0;
    for (size_t i = 0; i < valores.size(); ++i) {
        uint64_t gap = i == 0 ? valores[i] : valores[i] - anterior;
        gaps.push_back(gap);
        anterior = valores[i];
    }

    std::cout << "Demo con arreglo pequeno\n";
    std::cout << "A: ";
    for (uint64_t valor : valores) {
        std::cout << valor << ' ';
    }
    std::cout << "\nGaps: ";
    for (uint64_t gap : gaps) {
        std::cout << gap << ' ';
    }
    std::cout << "\n\n";

    std::cout << "Elias codifica enteros positivos, por eso usamos gap + 1.\n";
    std::cout << "gap,gap+1,gamma,bits_gamma,delta,bits_delta\n";

    EscritorBits flujo_gamma;
    EscritorBits flujo_delta;
    for (uint64_t gap : gaps) {
        uint64_t valor_codificado = gap + 1;
        std::string gamma = bits_gamma(valor_codificado);
        std::string delta = bits_delta(valor_codificado);

        escribir_gamma(flujo_gamma, valor_codificado);
        escribir_delta(flujo_delta, valor_codificado);

        std::cout << gap << ','
                  << valor_codificado << ','
                  << gamma << ','
                  << gamma.size() << ','
                  << delta << ','
                  << delta.size() << '\n';
    }

    std::cout << "\nBitstream gamma completo: " << bits_desde_escritor(flujo_gamma) << '\n';
    std::cout << "Bitstream delta completo: " << bits_desde_escritor(flujo_delta) << '\n';
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

void imprimir_vista_previa_vector(const std::vector<uint64_t>& valores, size_t limite = 20) {
    std::cout << '[';
    size_t mostrados = std::min(valores.size(), limite);
    for (size_t i = 0; i < mostrados; ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << valores[i];
    }
    if (valores.size() > mostrados) {
        std::cout << ", ...";
    }
    std::cout << "]\n";
}

std::vector<uint64_t> construir_gaps(const std::vector<uint64_t>& valores) {
    std::vector<uint64_t> gaps;
    gaps.reserve(valores.size());

    uint64_t anterior = 0;
    for (size_t i = 0; i < valores.size(); ++i) {
        uint64_t gap = i == 0 ? valores[i] : valores[i] - anterior;
        gaps.push_back(gap);
        anterior = valores[i];
    }

    return gaps;
}

void imprimir_vista_estructura(
    const std::vector<uint64_t>& valores,
    const std::string& estructura,
    size_t paso_muestra
) {
    std::cout << "\nArreglo ordenado (" << valores.size() << " valores): ";
    imprimir_vista_previa_vector(valores);

    if (estructura == "arreglo_original") {
        std::cout << "Vista arreglo_original: se almacena el arreglo completo.\n";
        return;
    }

    std::vector<uint64_t> gaps = construir_gaps(valores);
    std::cout << "Gaps: ";
    imprimir_vista_previa_vector(gaps);
    std::cout << "Sample step: " << paso_muestra << '\n';

    if (estructura == "gamma" || estructura == "delta") {
        std::cout << "Codificacion por gap + 1:\n";
        size_t mostrados = std::min<size_t>(gaps.size(), 12);
        for (size_t i = 0; i < mostrados; ++i) {
            uint64_t codificado = gaps[i] + 1;
            std::cout << "  i=" << i
                      << " gap=" << gaps[i]
                      << " valor_codificado=" << codificado
                      << " bits=" << (estructura == "gamma" ? bits_gamma(codificado) : bits_delta(codificado))
                      << '\n';
        }
        if (gaps.size() > mostrados) {
            std::cout << "  ...\n";
        }
    }
}

std::optional<size_t> busqueda_visual_explicita(
    const std::vector<uint64_t>& valores,
    uint64_t objetivo
) {
    size_t izquierda = 0;
    size_t derecha = valores.size();
    size_t paso = 1;

    while (izquierda < derecha) {
        size_t medio = izquierda + (derecha - izquierda) / 2;
        std::cout << "Paso " << paso++ << ". rango=["
                  << izquierda << ", " << (derecha - 1)
                  << "], medio=" << medio
                  << ", valor=" << valores[medio] << '\n';

        if (valores[medio] < objetivo) {
            izquierda = medio + 1;
        } else {
            derecha = medio;
        }
    }

    if (izquierda < valores.size() && valores[izquierda] == objetivo) {
        std::cout << "Paso " << paso << ". primer valor >= objetivo esta en posicion "
                  << izquierda << ", valor=" << valores[izquierda] << '\n';
        return izquierda;
    }

    return std::nullopt;
}

std::optional<size_t> busqueda_visual_gap(
    const std::vector<uint64_t>& valores,
    uint64_t objetivo,
    size_t paso_muestra
) {
    if (valores.empty()) {
        return std::nullopt;
    }

    std::vector<uint64_t> gaps = construir_gaps(valores);
    std::vector<size_t> muestras;
    for (size_t i = 0; i < valores.size(); i += paso_muestra) {
        muestras.push_back(i);
    }

    size_t indice_muestra = 0;
    for (size_t i = 0; i < muestras.size(); ++i) {
        std::cout << "Paso " << (i + 1)
                  << ". sample posicion=" << muestras[i]
                  << ", valor=" << valores[muestras[i]] << '\n';
        if (valores[muestras[i]] <= objetivo) {
            indice_muestra = i;
        } else {
            break;
        }
    }

    size_t inicio = muestras[indice_muestra];
    size_t fin = indice_muestra + 1 < muestras.size() ? muestras[indice_muestra + 1] : valores.size();
    uint64_t actual = inicio == 0 ? 0 : valores[inicio - 1];

    std::cout << "Bloque elegido: posiciones " << inicio << " a " << (fin - 1) << '\n';
    for (size_t i = inicio; i < fin; ++i) {
        actual += gaps[i];
        std::cout << "  decodifico gap[" << i << "]=" << gaps[i]
                  << " -> valor=" << actual << '\n';
        if (actual == objetivo) {
            return i;
        }
        if (actual > objetivo) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<size_t> busqueda_visual_elias(
    const std::vector<uint64_t>& valores,
    uint64_t objetivo,
    CodecElias codec,
    size_t paso_muestra
) {
    if (valores.empty()) {
        return std::nullopt;
    }

    std::vector<uint64_t> gaps = construir_gaps(valores);
    size_t indice_muestra = 0;
    for (size_t i = 0; i < valores.size(); i += paso_muestra) {
        std::cout << "Paso sample. posicion=" << i << ", valor=" << valores[i] << '\n';
        if (valores[i] <= objetivo) {
            indice_muestra = i / paso_muestra;
        } else {
            break;
        }
    }

    size_t inicio = indice_muestra * paso_muestra;
    size_t fin = std::min(valores.size(), inicio + paso_muestra);
    uint64_t actual = inicio == 0 ? 0 : valores[inicio - 1];
    std::string nombre_codec = codec == CodecElias::Gamma ? "gamma" : "delta";

    std::cout << "Bloque elegido: posiciones " << inicio << " a " << (fin - 1)
              << " usando Elias " << nombre_codec << '\n';
    for (size_t i = inicio; i < fin; ++i) {
        uint64_t codificado = gaps[i] + 1;
        std::string bits = codec == CodecElias::Gamma ? bits_gamma(codificado) : bits_delta(codificado);
        actual += gaps[i];
        std::cout << "  gap[" << i << "]=" << gaps[i]
                  << ", guardado como " << codificado
                  << ", bits=" << bits
                  << " -> valor=" << actual << '\n';
        if (actual == objetivo) {
            return i;
        }
        if (actual > objetivo) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

void construccion_visual(
    const std::vector<uint64_t>& valores,
    const std::string& estructura,
    size_t paso_muestra
) {
    std::cout << "\n=== Creacion de estructura ===\n";
    size_t bytes = 0;
    size_t bits = 0;

    auto resultado = medir([&]() {
        if (estructura == "arreglo_original") {
            ArregloExplicito arreglo_explicito(valores);
            bytes = arreglo_explicito.bytes_usados();
            bits = bytes * 8;
        } else if (estructura == "gap") {
            CodificacionGap codificacion_gap(valores, paso_muestra);
            bytes = codificacion_gap.bytes_usados();
            bits = bytes * 8;
        } else if (estructura == "gamma") {
            GapsComprimidosElias comprimido(valores, CodecElias::Gamma, paso_muestra);
            bytes = comprimido.bytes_usados();
            bits = comprimido.cantidad_bits();
        } else if (estructura == "delta") {
            GapsComprimidosElias comprimido(valores, CodecElias::Delta, paso_muestra);
            bytes = comprimido.bytes_usados();
            bits = comprimido.cantidad_bits();
        }
    });

    std::cout << "Paso 1. Datos base: ";
    imprimir_vista_previa_vector(valores);
    std::cout << "Paso 2. Representacion interna:\n";
    imprimir_vista_estructura(valores, estructura, paso_muestra);
    std::cout << "Paso 3. Tiempo de creacion: "
              << std::fixed << std::setprecision(6)
              << resultado.milisegundos << " ms\n";
    std::cout << "Paso 4. Espacio utilizado: "
              << bytes << " bytes, " << bits << " bits\n";
}

std::optional<size_t> busqueda_visual(
    const std::vector<uint64_t>& valores,
    const std::string& estructura,
    uint64_t objetivo,
    size_t paso_muestra
) {
    if (estructura == "arreglo_original") {
        return busqueda_visual_explicita(valores, objetivo);
    }
    if (estructura == "gap") {
        return busqueda_visual_gap(valores, objetivo, paso_muestra);
    }
    if (estructura == "gamma") {
        return busqueda_visual_elias(valores, objetivo, CodecElias::Gamma, paso_muestra);
    }
    return busqueda_visual_elias(valores, objetivo, CodecElias::Delta, paso_muestra);
}

std::vector<uint64_t> datos_visuales_defecto() {
    return {2, 7, 10, 12, 12, 16, 23, 31, 31, 45};
}

const std::vector<std::string>& orden_estructuras_visuales() {
    static const std::vector<std::string> estructuras = {"arreglo_original", "gap", "gamma", "delta"};
    return estructuras;
}

std::string titulo_estructura_visual(const std::string& estructura) {
    if (estructura == "arreglo_original") {
        return "Caso 1: arreglo explicito";
    }
    if (estructura == "gap") {
        return "Caso 2: Gap-Coding con sample";
    }
    if (estructura == "gamma") {
        return "Caso 3: Elias Gamma sobre gaps";
    }
    return "Caso 3: Elias Delta sobre gaps";
}

void mostrar_todas_estructuras(const std::vector<uint64_t>& datos, size_t paso_muestra) {
    std::cout << "\n==============================\n";
    std::cout << "Flujo de representaciones\n";
    std::cout << "==============================\n";

    for (const std::string& estructura : orden_estructuras_visuales()) {
        std::cout << "\n--- " << titulo_estructura_visual(estructura) << " ---\n";
        construccion_visual(datos, estructura, paso_muestra);
    }
}

void buscar_todas_estructuras(
    const std::vector<uint64_t>& datos,
    uint64_t objetivo,
    size_t paso_muestra
) {
    std::cout << "\n==============================\n";
    std::cout << "Busqueda del valor " << objetivo << '\n';
    std::cout << "==============================\n";

    for (const std::string& estructura : orden_estructuras_visuales()) {
        std::cout << "\n--- " << titulo_estructura_visual(estructura) << " ---\n";

        std::optional<size_t> posicion;
        auto resultado = medir([&]() {
            posicion = busqueda_visual(datos, estructura, objetivo, paso_muestra);
        });

        if (posicion.has_value()) {
            std::cout << "Resultado: encontrado en posicion " << *posicion << '\n';
        } else {
            std::cout << "Resultado: no encontrado\n";
        }
        std::cout << "Tiempo de busqueda visualizada: "
                  << std::fixed << std::setprecision(6)
                  << resultado.milisegundos << " ms\n";
    }
}

void ejecutar_modo_visual(const std::string& ruta_entrada) {
    std::vector<uint64_t> datos = ruta_entrada.empty()
        ? datos_visuales_defecto()
        : leer_numeros_csv(ruta_entrada);

    if (datos.empty()) {
        throw std::runtime_error("No hay datos para visualizar");
    }

    size_t paso_muestra = std::max<size_t>(1, static_cast<size_t>(std::sqrt(datos.size())));

    std::cout << "Modo visualizar\n";
    std::cout << "Muestra automaticamente: arreglo explicito, gap, gamma y delta.\n";
    mostrar_todas_estructuras(datos, paso_muestra);

    while (true) {
        std::string entrada;
        std::cout << "\nValor a buscar (o salir)> ";
        if (!(std::cin >> entrada) || entrada == "salir" || entrada == "exit") {
            break;
        }

        uint64_t objetivo = std::stoull(entrada);
        buscar_todas_estructuras(datos, objetivo, paso_muestra);
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
              << "  ./main --demo\n"
              << "  ./main --visualizar [-i archivo.csv]\n"
              << "  ./main -i archivo.csv\n";
}

int main(int argc, char** argv) {
    try {
        if (argc <= 1) {
            imprimir_uso();
            return 0;
        }

        if (tiene_argumento(argc, argv, "--demo")) {
            ejecutar_demo();
            return 0;
        }

        if (tiene_argumento(argc, argv, "--benchmark")) {
            std::string salida = valor_argumento(argc, argv, "--output", "benchmark_resultados.csv");
            size_t consultas = static_cast<size_t>(std::stoull(valor_argumento(argc, argv, "--queries", "1000")));
            std::vector<size_t> tamanos = parsear_tamanos(valor_argumento(argc, argv, "--sizes", "10000,100000,1000000"));
            ejecutar_benchmark(salida, tamanos, consultas);
            return 0;
        }

        if (tiene_argumento(argc, argv, "--visualizar")) {
            std::string entrada = valor_argumento(argc, argv, "-i", "");
            ejecutar_modo_visual(entrada);
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
