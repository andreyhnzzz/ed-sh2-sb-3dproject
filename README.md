# EcoCampusNav

> Navegacion inteligente para campus universitario en `C++17` con `raylib`, `ImGui` y arquitectura modular orientada a runtime.
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![raylib](https://img.shields.io/badge/raylib-2D%20Runtime-000000?style=for-the-badge&logo=raylib&logoColor=white)
![ImGui](https://img.shields.io/badge/ImGui-Dear%20ImGui-5A4FCF?style=for-the-badge)
![JSON](https://img.shields.io/badge/JSON-nlohmann-000000?style=for-the-badge&logo=json&logoColor=white)
![CLion](https://img.shields.io/badge/CLion-IDE-000000?style=for-the-badge&logo=clion&logoColor=white)

## Vista General

| Aspecto | Detalle |
| --- | --- |
| Proposito | Simular navegacion academica e inclusiva dentro de un campus |
| Stack | `C++17`, `CMake`, `raylib`, `rlImGui`, `nlohmann/json` |
| Motor visual | Render 2D con mapa por escenas y overlays interactivos |
| Enfoque | Rutas, perfiles de usuario, resiliencia y analisis de grafos |
| Estado | Listo para compilacion `Debug` y `Release` |

## Caracteristicas

| Modulo | Capacidades |
| --- | --- |
| Navegacion | BFS, DFS, ruta perfilada y ruta alterna |
| Escenarios | Estudiante nuevo, veterano y movilidad reducida |
| Resiliencia | Bloqueo de nodos y aristas con recalculo de ruta |
| Runtime | Cambio entre escenas, spawns, triggers y transiciones |
| UI | Menu principal, overlay informativo y panel academico |
| Analitica | Comparacion empirica BFS vs DFS y validacion basica del grafo |

## Arquitectura

```text
EcoCampusNav/
|- assets/                     # mapas, sprites, audio y datos runtime
|- external/rlImGui/           # integracion local raylib + ImGui
|- src/
|  |- core/
|  |  `- application/          # sesion, ventana y audio
|  |- repositories/            # acceso a datos JSON
|  |- runtime/                 # loop de juego, menu y escenas
|  |- services/                # reglas de negocio y utilidades
|  `- ui/                      # estado y componentes de tabs
|- campus.json
|- CMakeLists.txt
`- CMakePresets.json
```

## Flujo de Aplicacion

| Etapa | Responsable |
| --- | --- |
| Bootstrap | `ApplicationSession` |
| Inicializacion de ventana/audio | `WindowInitializer`, `AudioInitializer` |
| Carga de escenas | `SceneBootstrap`, `SceneManager` |
| Menu de inicio | `StartMenuController` |
| Gameplay | `GameplayLoopController` |
| Navegacion runtime | `RuntimeNavigationManager` |

## Controles

| Entrada | Accion |
| --- | --- |
| `W/A/S/D` | Movimiento |
| `Shift` | Sprint |
| Rueda del mouse | Zoom |
| `M` | Abrir o cerrar menu de informacion |
| `W/S` o flechas | Navegar menu principal |
| `Enter` / clic izquierdo | Confirmar opcion |

## Compilacion

### Debug

```bash
cmake --preset debug
cmake --build --preset debug
```

### Release

```bash
cmake --preset release
cmake --build --preset release
```

## Ejecucion

| Modo | Binario esperado |
| --- | --- |
| Debug | `build-debug/Debug/EcoCampusNav.exe` |
| Release | `build/Release/EcoCampusNav.exe` |

## Dependencias

| Dependencia | Uso |
| --- | --- |
| `raylib` | render, input, audio base |
| `ImGui` | paneles y tooling academico |
| `rlImGui` | puente entre raylib e ImGui |
| `nlohmann/json` | parseo de datos del campus |

## Estado Tecnico

| Estado | Nota |
| --- | --- |
| `main.cpp` | Deliberadamente delgado: delega a `ApplicationSession` |
| Runtime | Separado en controladores de menu y gameplay |
| Build | Presets `debug` y `release` configurados |
| Entrega | `.gitignore` preparado para artefactos comunes de CMake y Visual Studio |

## Entrega Recomendada

1. Compilar `Release`.
2. Verificar carga de escenas y audio.
3. Confirmar transiciones entre pisos.
4. Subir el repo sin artefactos locales ignorados.

## Creditos

| Rol | Nombre |
| --- | --- |
| Desarrollo | Javier Mendoza Gonzalez |
| Desarrollo | Andrey Hernandez Salazar |
