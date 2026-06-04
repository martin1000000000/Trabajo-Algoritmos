# Estructuras de Datos Comprimidas - Trabajo Algoritmos

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

## Modos de Uso

El programa cuenta con cuatro modos de ejecución principales para facilitar tanto la evaluación de rendimiento como la validación pedagógica:

### 1. Modo Demo (Ejecución rápida)
Realiza una demostración hardcodeada con un arreglo pequeño, mostrando los gaps y cómo se codifican en bits usando Gamma y Delta.
```bash
./main --demo
```

### 2. Modo Benchmark (Generación de métricas)
Evalúa las estructuras generando automáticamente arreglos con tamaños desde 10.000 hasta 100.000.000 de elementos en distribuciones Lineales y Normales. Exporta los resultados de tiempo y espacio.
```bash
./main --benchmark
# Opcionalmente se pueden pasar parámetros personalizados:
# ./main --benchmark --output resultados.csv --queries 10000 --sizes 10000,100000,1000000
```

### 3. Modo Archivo Interactivo (Testing manual)
Permite cargar un archivo `.csv` con números (separados por cualquier caracter no numérico) y levanta una consola interactiva para realizar búsquedas manuales sobre cualquiera de las 4 estructuras y comparar los tiempos.
```bash
./main -i archivo_de_datos.csv
```
*Tip: Una vez en la consola, introduce la estructura (`arreglo_original`, `gap`, `gamma` o `delta`) y luego el valor a buscar.*

### 4. Modo Visual (Paso a paso)
Herramienta visual que muestra detalladamente cómo el programa transforma los datos bases a gaps, cómo los comprime a nivel de bits y los pasos exactos que hace la búsqueda binaria y el escaneo por bloques de las muestras.
```bash
./main --visualizar
# También puede combinarse con un archivo CSV propio:
# ./main --visualizar -i archivo_de_datos.csv
```

## Detalles de Implementación
*   **Muestreo (Sampling):** Las estructuras comprimidas (`gap`, `gamma` y `delta`) utilizan un índice de muestreo separado cada $\lfloor\sqrt{n}\rfloor$ posiciones para mantener los tiempos de búsqueda eficientes sin comprometer demasiado el ratio de compresión.
*   **Manejo del Cero en Elias:** Dado que los códigos de Elias estandarizados no pueden representar el número 0 de forma natural, internamente se codifica el valor `gap + 1` en las estructuras Gamma y Delta para permitir la existencia de valores repetidos (gap = 0).
