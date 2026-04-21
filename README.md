# EcoCampusNav

EcoCampusNav es un proyecto en C++17 orientado a simulacion de navegacion en campus.
Actualmente el proyecto esta en una etapa de juego 2D top-down con `raylib`, usando mapas
de Tiled (`.tmj`) y sprites animados, mientras conserva la capa de logica de grafos para
DFS, BFS, conectividad, rutas y resiliencia.

## Estado actual del proyecto

El runtime principal esta en `src/main.cpp` y hoy incluye:

- Render de mapa `Paradadebus.png` (escenario top-down)
- Carga de hitboxes desde `Paradadebus.tmj` (layer `Hitboxes`)
- Personaje con sprites `idle/walk` y animacion por direccion
- Movimiento con `W/A/S/D`
- Sprint con `Shift`
- Camara `Camera2D` con seguimiento al personaje y zoom con rueda
- HUD con coordenadas del personaje (esquina superior derecha)
- Panel de control con operaciones del grafo (DFS, BFS, camino, conectividad, etc.)

## Arquitectura del codigo

La estructura sigue separada por capas:

- `src/core/graph/`
  - `CampusGraph`, `Node`, `Edge`, `Algorithms`
- `src/repositories/`
  - `JsonGraphRepository` (carga de `campus.json`)
- `src/services/`
  - `NavigationService`
  - `ScenarioManager`
  - `ComplexityAnalyzer`
  - `ResilienceService`
- `src/main.cpp`
  - Integracion de escena, mapa, personaje, camara e interfaz

## Estructura del repo

```text
ed-sb-2dproject/
|- CMakeLists.txt
|- CMakePresets.json
|- campus.json
|- settings.json
|- assets/
|  |- maps/
|  |- sprites/
|  `- tilesets/
|- external/
|  `- rlImGui/
`- src/
   |- core/
   |- repositories/
   |- services/
   |- ui/        (codigo legado Qt, no es el frontend activo)
   `- main.cpp
```

## Dependencias

- CMake >= 3.20
- Compilador C++17 (MSVC, Clang o GCC)
- `raylib` (via `find_package(raylib CONFIG REQUIRED)`)
- `nlohmann/json` (FetchContent)
- `imgui` (FetchContent)
- `external/rlImGui` (wrapper local)

## Compilar

### Con presets

```bash
cmake --preset debug
cmake --build --preset debug
```

o

```bash
cmake --preset release
cmake --build --preset release
```

### Manual

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Ejecutar

Desde la carpeta de build donde quede el ejecutable:

```bash
./EcoCampusNav
```

Nota: `CMakeLists.txt` copia automaticamente `assets/` al directorio runtime del ejecutable.

## Controles actuales

- `W/A/S/D`: movimiento
- `Shift`: sprint
- Rueda del mouse: zoom de camara

## Datos de mapa y colision

- Mapa base activo: `assets/maps/Paradadebus.png`
- Mapa Tiled activo: `assets/maps/Paradadebus.tmj`
- Colision: objetos del layer `Hitboxes` en el `.tmj`

## Observaciones tecnicas

- El panel de control usa ImGui.
- La logica de input de ImGui esta implementada en `external/rlImGui/rlImGui.cpp`.
- Existe codigo Qt en `src/ui/`, pero actualmente no es la ruta de ejecucion principal.

## Proximos pasos sugeridos

- Clasificar hitboxes por tipo (solidas vs decorativas)
- Definir puntos de spawn por objetos nombrados en Tiled
- Suavizar seguimiento de camara (lerp)
- Unificar documentacion de la capa UI para evitar confusion con Qt legado
