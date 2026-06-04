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

class EscritorBits {
public:
    void escribir_bit(bool bit) {
        if (cantidad_bits_ % 8 == 0) {
            bytes_.push_back(0);
        }

        if (bit) {
            bytes_.back() |= static_cast<uint8_t>(1u << (7 - (cantidad_bits_ % 8)));
        }
        ++cantidad_bits_;
    }

    void escribir_bits(uint64_t valor, int cantidad) {
        for (int i = cantidad - 1; i >= 0; --i) {
            escribir_bit(((valor >> i) & 1ULL) != 0);
        }
    }

    size_t cantidad_bits() const {
        return cantidad_bits_;
    }

    const std::vector<uint8_t>& bytes() const {
        return bytes_;
    }

private:
    std::vector<uint8_t> bytes_;
    size_t cantidad_bits_ = 0;
};

class LectorBits {
public:
    LectorBits(const std::vector<uint8_t>& bytes, size_t cantidad_bits, size_t desplazamiento = 0)
        : bytes_(bytes), cantidad_bits_(cantidad_bits), desplazamiento_(desplazamiento) {}

    bool leer_bit() {
        if (desplazamiento_ >= cantidad_bits_) {
            throw std::out_of_range("Lectura fuera del bitstream");
        }

        bool bit = ((bytes_[desplazamiento_ / 8] >> (7 - (desplazamiento_ % 8))) & 1U) != 0;
        ++desplazamiento_;
        return bit;
    }

    uint64_t leer_bits(int cantidad) {
        uint64_t valor = 0;
        for (int i = 0; i < cantidad; ++i) {
            valor = (valor << 1) | (leer_bit() ? 1ULL : 0ULL);
        }
        return valor;
    }

    size_t desplazamiento() const {
        return desplazamiento_;
    }

private:
    const std::vector<uint8_t>& bytes_;
    size_t cantidad_bits_;
    size_t desplazamiento_;
};

inline int longitud_bits(uint64_t valor) {
    int longitud = 0;
    do {
        ++longitud;
        valor >>= 1;
    } while (valor != 0);
    return longitud;
}

inline void escribir_gamma(EscritorBits& escritor, uint64_t valor) {
    if (valor == 0) {
        throw std::invalid_argument("Elias gamma solo codifica enteros positivos");
    }

    int longitud = longitud_bits(valor);
    for (int i = 0; i < longitud - 1; ++i) {
        escritor.escribir_bit(false);
    }
    escritor.escribir_bits(valor, longitud);
}

inline uint64_t leer_gamma(LectorBits& lector) {
    int ceros = 0;
    while (!lector.leer_bit()) {
        ++ceros;
    }

    uint64_t sufijo = ceros == 0 ? 0 : lector.leer_bits(ceros);
    return (1ULL << ceros) | sufijo;
}

inline void escribir_delta(EscritorBits& escritor, uint64_t valor) {
    if (valor == 0) {
        throw std::invalid_argument("Elias delta solo codifica enteros positivos");
    }

    int longitud = longitud_bits(valor);
    escribir_gamma(escritor, static_cast<uint64_t>(longitud));
    escritor.escribir_bits(valor ^ (1ULL << (longitud - 1)), longitud - 1);
}

inline uint64_t leer_delta(LectorBits& lector) {
    uint64_t longitud = leer_gamma(lector);
    uint64_t sufijo = longitud <= 1 ? 0 : lector.leer_bits(static_cast<int>(longitud - 1));
    return (1ULL << (longitud - 1)) | sufijo;
}

enum class CodecElias {
    Gamma,
    Delta
};

inline std::string nombre_codec_elias(CodecElias codec) {
    return codec == CodecElias::Gamma ? "elias_gamma" : "elias_delta";
}

class GapsComprimidosElias {
public:
    GapsComprimidosElias(
        const std::vector<uint64_t>& valores,
        CodecElias codec,
        size_t paso_muestra = 0
    ) : codec_(codec) {
        construir(valores, paso_muestra);
    }

    std::optional<size_t> buscar(uint64_t objetivo) const {
        if (tamano_ == 0) {
            return std::nullopt;
        }

        auto it = std::upper_bound(valores_muestra_.begin(), valores_muestra_.end(), objetivo);
        size_t indice_muestra = it == valores_muestra_.begin()
            ? 0
            : static_cast<size_t>((it - valores_muestra_.begin()) - 1);

        size_t inicio = posiciones_muestra_[indice_muestra];
        size_t fin = indice_muestra + 1 < posiciones_muestra_.size()
            ? posiciones_muestra_[indice_muestra + 1]
            : tamano_;

        LectorBits lector(bytes_, cantidad_bits_, desplazamientos_bits_muestra_[indice_muestra]);
        uint64_t actual = bases_muestra_[indice_muestra];

        for (size_t i = inicio; i < fin; ++i) {
            uint64_t gap_codificado = leer_codigo(lector);
            uint64_t gap = gap_codificado - 1;
            actual += gap;

            if (actual == objetivo) {
                return i;
            }
            if (actual > objetivo) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    size_t bytes_usados() const {
        return bytes_.capacity() * sizeof(uint8_t)
            + posiciones_muestra_.capacity() * sizeof(size_t)
            + valores_muestra_.capacity() * sizeof(uint64_t)
            + bases_muestra_.capacity() * sizeof(uint64_t)
            + desplazamientos_bits_muestra_.capacity() * sizeof(size_t);
    }

    size_t cantidad_bits() const {
        return cantidad_bits_;
    }

    size_t tamano() const {
        return tamano_;
    }

    size_t paso_muestra() const {
        return paso_muestra_;
    }

private:
    void construir(const std::vector<uint64_t>& valores, size_t paso_muestra) {
        tamano_ = valores.size();
        if (valores.empty()) {
            paso_muestra_ = 1;
            return;
        }

        paso_muestra_ = paso_muestra == 0
            ? std::max<size_t>(1, static_cast<size_t>(std::sqrt(valores.size())))
            : paso_muestra;

        posiciones_muestra_.reserve((valores.size() + paso_muestra_ - 1) / paso_muestra_);
        valores_muestra_.reserve(posiciones_muestra_.capacity());
        bases_muestra_.reserve(posiciones_muestra_.capacity());
        desplazamientos_bits_muestra_.reserve(posiciones_muestra_.capacity());

        EscritorBits escritor;
        uint64_t anterior = 0;

        for (size_t i = 0; i < valores.size(); ++i) {
            if (i % paso_muestra_ == 0) {
                posiciones_muestra_.push_back(i);
                valores_muestra_.push_back(valores[i]);
                bases_muestra_.push_back(i == 0 ? 0 : anterior);
                desplazamientos_bits_muestra_.push_back(escritor.cantidad_bits());
            }

            uint64_t gap = i == 0 ? valores[i] : valores[i] - anterior;
            escribir_codigo(escritor, gap + 1);
            anterior = valores[i];
        }

        bytes_ = escritor.bytes();
        cantidad_bits_ = escritor.cantidad_bits();
    }

    void escribir_codigo(EscritorBits& escritor, uint64_t valor) const {
        if (codec_ == CodecElias::Gamma) {
            escribir_gamma(escritor, valor);
        } else {
            escribir_delta(escritor, valor);
        }
    }

    uint64_t leer_codigo(LectorBits& lector) const {
        return codec_ == CodecElias::Gamma ? leer_gamma(lector) : leer_delta(lector);
    }

    CodecElias codec_;
    std::vector<uint8_t> bytes_;
    std::vector<size_t> posiciones_muestra_;
    std::vector<uint64_t> valores_muestra_;
    std::vector<uint64_t> bases_muestra_;
    std::vector<size_t> desplazamientos_bits_muestra_;
    size_t cantidad_bits_ = 0;
    size_t tamano_ = 0;
    size_t paso_muestra_ = 1;
};

#endif
