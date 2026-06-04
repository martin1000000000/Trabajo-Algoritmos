#ifndef GAP_CODING_HPP
#define GAP_CODING_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class CodificacionGap {
public:
    CodificacionGap(const std::vector<uint64_t>& valores, size_t paso_muestra = 0) {
        construir(valores, paso_muestra);
    }

    std::optional<size_t> buscar(uint64_t objetivo) const {
        if (gaps_.empty()) {
            return std::nullopt;
        }

        auto it = std::upper_bound(valores_muestra_.begin(), valores_muestra_.end(), objetivo);
        size_t indice_muestra = it == valores_muestra_.begin()
            ? 0
            : static_cast<size_t>((it - valores_muestra_.begin()) - 1);

        // Si el punto de muestreo seleccionado ya es el objetivo, el primer duplicado
        // puede estar en bloques anteriores: retrocedemos el inicio de búsqueda
        // mientras el punto de muestreo sea igual al objetivo.
        size_t indice_busqueda = indice_muestra;
        while (indice_busqueda > 0 && valores_muestra_[indice_busqueda] == objetivo) {
            indice_busqueda--;
        }

        size_t inicio = posiciones_muestra_[indice_busqueda];
        size_t fin = indice_muestra + 1 < posiciones_muestra_.size()
            ? posiciones_muestra_[indice_muestra + 1]
            : gaps_.size();

        uint64_t actual = bases_muestra_[indice_busqueda];
        for (size_t i = inicio; i < fin; ++i) {
            actual += gaps_[i];
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
        return gaps_.size() * sizeof(uint64_t)
            + posiciones_muestra_.size() * sizeof(size_t)
            + valores_muestra_.size() * sizeof(uint64_t)
            + bases_muestra_.size() * sizeof(uint64_t);
    }

    size_t tamano() const {
        return gaps_.size();
    }

    size_t paso_muestra() const {
        return paso_muestra_;
    }

private:
    void construir(const std::vector<uint64_t>& valores, size_t paso_muestra) {
        gaps_.clear();
        posiciones_muestra_.clear();
        valores_muestra_.clear();
        bases_muestra_.clear();

        if (valores.empty()) {
            paso_muestra_ = 1;
            return;
        }

        paso_muestra_ = paso_muestra == 0
            ? std::max<size_t>(1, static_cast<size_t>(std::sqrt(valores.size())))
            : paso_muestra;

        gaps_.reserve(valores.size());
        posiciones_muestra_.reserve((valores.size() + paso_muestra_ - 1) / paso_muestra_);
        valores_muestra_.reserve(posiciones_muestra_.capacity());
        bases_muestra_.reserve(posiciones_muestra_.capacity());

        uint64_t anterior = 0;
        for (size_t i = 0; i < valores.size(); ++i) {
            uint64_t gap = i == 0 ? valores[i] : valores[i] - anterior;
            gaps_.push_back(gap);

            if (i % paso_muestra_ == 0) {
                posiciones_muestra_.push_back(i);
                valores_muestra_.push_back(valores[i]);
                bases_muestra_.push_back(i == 0 ? 0 : anterior);
            }

            anterior = valores[i];
        }
    }

    std::vector<uint64_t> gaps_;
    std::vector<size_t> posiciones_muestra_;
    std::vector<uint64_t> valores_muestra_;
    std::vector<uint64_t> bases_muestra_;
    size_t paso_muestra_ = 1;
};

#endif
