# EcoCampusNav

Sistema de navegacion inteligente para campus universitario implementado en C++17 con `raylib` + `rlImGui`.

## Caracteristicas principales

- Menu academico con 8 pestanas activado con `M`
- Comparacion BFS vs DFS con nodos visitados y tiempo en microsegundos
- Busqueda de rutas perfiladas y rutas DFS
- Simulacion de bloqueos de nodos y conexiones
- Recalculo de rutas en tiempo real
- Perfiles de estudiante: nuevo, veterano y discapacitado
- Visualizacion del grafo sobre el mapa del campus

## Estructura

```text
EcoCampusNav/
|- CMakeLists.txt
|- CMakePresets.json
|- campus.json
|- assets/
|- external/
|  `- rlImGui/
`- src/
   |- core/
   |- repositories/
   |- runtime/
   |- services/
   `- ui/
      |- TabManager.cpp
      `- TabManager.h
```

Nota: el frontend Qt legado fue retirado del arbol activo. `src/ui/TabManager.*` se conserva porque hoy no depende de Qt y sigue aportando estado/utilidades al runtime Raylib.

## Dependencias

- CMake 3.20+
- Compilador compatible con C++17
- `raylib`
- `nlohmann/json`
- `imgui`
- `external/rlImGui`

## Compilacion

```bash
cmake --preset debug
cmake --build --preset debug
```

## Uso

1. Ejecuta `EcoCampusNav`.
2. Presiona `M` para abrir el menu de analisis.
3. Navega por las pestanas:
   - `Mapa`
   - `DFS`
   - `BFS`
   - `Conexo`
   - `Camino`
   - `Escenarios`
   - `Complejidad`
   - `Fallos`

## Controles

- `W/A/S/D`: movimiento
- `Shift`: sprint
- Rueda del mouse: zoom
- `M`: abrir o cerrar el menu de informacion

## Estado tecnico

- El runtime principal vive en `src/main.cpp`.
- La interfaz activa usa `raylib` y `rlImGui`.
- El menu academico compara algoritmos, muestra conectividad, simula fallos y controla perfiles.
