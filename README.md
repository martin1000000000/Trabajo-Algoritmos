# Grupo 3 del trabajo de diseno y analisis de algoritmos

## Integrantes

- Diego Mora
- Martin Arrigo
- Rodrigo Almonacid

## Proyecto

Implementacion para el proyecto semestral de INFO145. El programa compara tres formas de representar y buscar en arreglos ordenados:

- Caso 1: arreglo explicito ordenado.
- Caso 2: Gap-Coding con indice de muestreo (`sample`).
- Caso 3: compresion de gaps con codificacion Elias Gamma y Elias Delta.

Como Elias Gamma y Delta solo codifican enteros positivos, cada gap se almacena como `gap + 1` y al decodificar se resta `1`. Esto permite representar gaps iguales a cero.

## Compilacion

Con Makefile:

```bash
make
```

Compilacion manual equivalente:

```bash
g++ -std=c++17 -O3 -Wall -Wextra -pedantic main.cpp -o main
```

En Windows, si el ejecutable queda como `main.exe`, los ejemplos se ejecutan usando `./main.exe`.

## Modo benchmark

Ejecuta generacion de datos, construccion de estructuras, busquedas y medicion de espacio.

```bash
./main --benchmark
```

Argumentos opcionales:

```bash
./main --benchmark --output metricas.csv --queries 5000 --sizes 10000,100000
```

Salida:

- `benchmark_resultados.csv` por defecto, o el archivo indicado con `--output`.
- Columnas: `distribucion,n,estructura,construccion_ms,busqueda_ms,bytes,bits,busquedas`.

El benchmark prueba distribucion lineal y normal para tamanos `10000`, `100000` y `1000000`.
Puede cambiar esos tamanos con `--sizes`, separandolos por coma y sin espacios.

## Modo demo

Muestra un ejemplo pequeno con el arreglo, sus gaps y la codificacion Elias Gamma/Delta de cada gap.

```bash
./main --demo
```

En Windows/MSYS:

```bash
./main.exe --demo
```

## Modo archivo

Lee un archivo `.csv` con numeros enteros no negativos, los ordena, construye las estructuras y permite buscar interactivamente.

```bash
./main -i ruta/del/archivo.csv
```

Luego ingresar una estructura y un valor:

```text
Estructura> gamma
Valor> 12345
```

Estructuras aceptadas:

- `arreglo_original`
- `gap`
- `gamma`
- `delta`

Tambien se acepta `explicit` como alias de `arreglo_original`.

Para terminar, escribir `salir` o `exit`.

## Modo visualizar

Sirve para entender y explicar el flujo de trabajo de los Casos 1, 2 y 3 sin tener que revisar el codigo fuente. Es un modo didactico: muestra paso a paso como se representa el mismo arreglo como arreglo explicito, Gap-Coding con sample, Elias Gamma sobre gaps y Elias Delta sobre gaps.

Al iniciar, muestra automaticamente las cuatro representaciones en este orden:

1. Arreglo explicito.
2. Gap-Coding con sample.
3. Elias Gamma sobre gaps.
4. Elias Delta sobre gaps.

Para cada estructura muestra la construccion, el arreglo/gaps correspondientes, el tiempo de construccion y el espacio utilizado. Puede trabajar con datos de ejemplo o con un archivo `.csv`.

```bash
./main --visualizar
```

Con archivo:

```bash
./main --visualizar -i entrada.csv
```

Luego el programa pide un valor a buscar:

```text
Valor a buscar (o salir)> 12
```

Con ese unico valor muestra la busqueda paso a paso en las cuatro estructuras. Para terminar, escribir `salir` o `exit`.

Este modo es util para demostraciones, depuracion y grabacion del video. Para mediciones experimentales del informe se debe usar el modo benchmark, ya que `--visualizar` imprime muchos pasos por pantalla y esos `cout` afectan los tiempos.

## Rango de datos

El programa trabaja con `uint64_t` y lee los numeros con `std::stoull`, por lo que acepta enteros no negativos en el rango `0` a `18446744073709551615`, siempre que las sumas internas no desborden ese tipo.

## Archivos principales

- `main.cpp`: entrada del programa, modos de ejecucion y mediciones.
- `include/data.hpp`: generacion de datos lineales/normales y lectura CSV.
- `include/explicit_array.hpp`: busqueda binaria sobre arreglo explicito.
- `include/gap_coding.hpp`: Gap-Coding con sample.
- `include/elias.hpp`: bitstream, Elias Gamma, Elias Delta y busqueda sobre gaps comprimidos.
- `Makefile`: compilacion.
