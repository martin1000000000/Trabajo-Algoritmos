# Trabajo Diseño y Análisis de Algoritmos

**Integrantes:**
* Diego Mora
* Martin Arrigo
* Rodrigo Almonacid

Este repositorio contiene la implementación en C++17 de un sistema de búsqueda sobre secuencias de números enteros crecientes utilizando distintas estrategias de compresión. Se comparan el uso de arreglos explícitos contra enfoques compactos utilizando **Gap Coding** y **Codificación de Elias (Gamma y Delta)**.

## Estructura del Proyecto

```text
├── include/
│   ├── data.hpp           # Generación de distribuciones (Normal y Lineal) y lectura de CSV
│   ├── explicit_array.hpp # Implementación baseline (arreglo original no comprimido)
│   ├── gap_coding.hpp     # Implementación de compresión Gap-Coding con muestreo (sqrt(n))
│   └── elias.hpp          # Codificador/Decodificador bit a bit para Elias Gamma y Delta
├── main.cpp               # Punto de entrada, modos de ejecución y benchmarks
├── Makefile               # Script de compilación para Linux/WSL
└── README.md              # Este archivo
```

## Requisitos y Compilación

El proyecto está diseñado para ser compilado en un entorno **Linux (o WSL en Windows)**. Requiere un compilador compatible con **C++17** (ej. `g++`).

Para compilar el proyecto, simplemente ejecuta en la raíz del proyecto:
```bash
make
```
Esto generará el ejecutable `main` (o `main.exe` en Windows). Para limpiar los binarios, puedes usar `make clean`.

## Tipos de Datos y Rango Aceptado
Cumpliendo con los requisitos de la evaluación, este programa ha sido implementado utilizando el tipo de dato **`uint64_t`** (enteros sin signo de 64 bits). 
Para la lectura y conversión de caracteres numéricos desde los archivos CSV a la memoria, el código utiliza la función **`std::stoull`** de la librería estándar de C++17 (el equivalente moderno y robusto a `atoll`).

Gracias a esta arquitectura, el rango de los números aceptados y procesados por el programa va desde **`0`** hasta **`18,446,744,073,709,551,615`** ($2^{64}-1$). No se aceptan números negativos.

> **Nota:** Las estructuras Elias (Gamma y Delta) codifican internamente `gap + 1` para soportar duplicados. Por ello, no pueden representar un gap igual a $2^{64}-1$ (`UINT64_MAX`), ya que la suma desbordaría a cero y la construcción lanzará un error. Esto afecta únicamente a las estructuras Elias (`arreglo_original` y `gap_coding` sí manejan este valor). En la práctica, este error solo ocurre si el arreglo contiene el valor `18,446,744,073,709,551,615` como primer elemento, o si la diferencia entre dos elementos consecutivos es exactamente ese valor; en los benchmarks de este proyecto no afecta ya que los gaps generados son pequeños.

## Modos de Uso

El programa cuenta con dos modos de ejecución principales para facilitar tanto la evaluación de rendimiento como la validación pedagógica:

### 1. Modo Benchmark (Generación de métricas)
Evalúa las estructuras sobre arreglos en distribuciones Lineal y Normal. Para la distribución Normal se prueban **tres desviaciones estándar** distintas (`std5`, `std30`, `std100`), reflejadas en la columna `distribucion` del CSV de salida. Exporta los resultados de tiempo y espacio.

Por defecto ejecuta los **tres tamaños base** (10.000, 100.000, 1.000.000). Para tamaños mayores se puede usar `--sizes` de forma personalizada.
```bash
./main --benchmark
# Por defecto usa: --sizes 10000,100000,1000000 --queries 1000

# Para incluir tamaños grandes (puede tardar varios minutos):
# ./main --benchmark --output resultados.csv --queries 10000 --sizes 10000,100000,1000000,10000000,100000000
```

### 2. Modo Archivo Interactivo (Testing manual)
Permite cargar un archivo `.csv` con números (separados por cualquier caracter no numérico) y levanta una consola interactiva para realizar búsquedas manuales sobre cualquiera de las 4 estructuras y comparar los tiempos.
```bash
./main -i archivo_de_datos.csv
```
*Tip: Una vez en la consola, introduce la estructura (`arreglo_original`, `gap`, `gamma` o `delta`) y luego el valor a buscar.*

## Detalles de Implementación
*   **Muestreo (Sampling):** Las estructuras comprimidas (`gap`, `gamma` y `delta`) utilizan un índice de muestreo separado cada $\lfloor\sqrt{n}\rfloor$ posiciones para mantener los tiempos de búsqueda eficientes sin comprometer demasiado el ratio de compresión.
*   **Manejo del Cero en Elias:** Dado que los códigos de Elias estandarizados no pueden representar el número 0 de forma natural, internamente se codifica el valor `gap + 1` en las estructuras Gamma y Delta para permitir la existencia de valores repetidos (gap = 0).
*   **Desviaciones Estándar en Normal:** El benchmark evalúa la distribución Normal con tres valores de $\sigma$ (`std5`, `std30`, `std100`), representando gaps pequeños (valores agrupados), medios y amplios respectivamente. Esto permite observar el impacto del tamaño de los gaps en el ratio de compresión y los tiempos de acceso de Elias y Gap Coding.
