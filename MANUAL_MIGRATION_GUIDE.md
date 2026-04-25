# Guía de Migración Manual Restante - main.cpp

## Resumen del Estado Actual

**Archivo original**: `/workspace/src/main.cpp` (~1949 líneas)  
**Objetivo**: Reducir a < 100 líneas de lógica ejecutable en main()

---

## 📋 Tareas Pendientes de Migración Manual

### 1. RuntimeInfoMenuManager.cpp (PRIORIDAD ALTA)

**Archivo destino**: `/workspace/src/runtime/RuntimeInfoMenuManager.cpp`  
**Líneas a migrar desde main.cpp**:

| Función | Líneas | Descripción |
|---------|--------|-------------|
| `drawRayButton` | 657-673 | Helper para botones raylib (17 líneas) |
| `studentTypeToLabel` | 675-683 | Conversión StudentType a string (9 líneas) |
| `drawRaylibNavigationOverlayMenu` | 685-718 | Overlay de navegación (34 líneas) |
| `drawRaylibInfoMenu` | 720-1114 | **Menú completo de información (395 líneas)** |

**Total estimado**: ~455 líneas

**Instrucciones**:
1. Copiar cada función al .cpp
2. Cambiar parámetros para usar structs de contexto en lugar de variables sueltas
3. El header ya tiene declarada la clase con `InfoMenuContext`

---

### 2. Funciones Helper Estáticas (PRIORIDAD MEDIA)

**Archivo destino**: `/workspace/src/helpers/NavigationHelpers.cpp` (ya existe el header)  
**Líneas a migrar desde main.cpp**:

| Función | Líneas | Descripción |
|---------|--------|-------------|
| `isOverlayEdgeAllowed` | 49-53 | Filtro de aristas para overlay |
| `pathContainsDirectedStep` | 55-62 | Verifica si path contiene paso dirigido |
| `collectVisualPoiNodes` | 64-82 | Colecta nodos POI visuales |
| `countProfileDiscardedEdges` | 84-99 | Cuenta aristas descartadas por perfil |
| `buildSelectionCriterion` | 101-109 | Construye criterio de selección |
| `playerFrontAnchor` | 111-120 | Calcula anchor frontal del jugador |
| `refreshSceneHitboxes` | 122-128 | Refresca hitboxes de escena |
| `mergeProfiledSegments` | 130-144 | Fusiona segmentos de ruta |
| `runProfiledDfsPath` | 146-161 | Ejecuta DFS con perfil |
| `runProfiledAlternatePath` | 163-178 | Ejecuta ruta alterna con perfil |
| `comboSelectNode` | 180-198 | Combo ImGui para seleccionar nodo |
| `linkMatchesEdgeType` | 200-210 | Verifica si link coincide con tipo de arista |
| `findBestEdgeForLink` | 212-229 | Encuentra mejor arista para link |
| `buildOverlayPathForScene` | 231-265 | Construye path overlay para escena |

**Total estimado**: ~220 líneas

**Nota**: El archivo `NavigationHelpers.h` ya está creado con estas funciones declaradas como estáticas.

---

### 3. renderAcademicRuntimeOverlay (PRIORIDAD MEDIA)

**Archivo destino**: `/workspace/src/runtime/RuntimeOverlayManager.cpp`  
**Líneas a migrar desde main.cpp**: 353-634 (~282 líneas)

**Instrucciones**:
1. Mover toda la función `renderAcademicRuntimeOverlay` al .cpp
2. Crear un struct `AcademicOverlayContext` que agrupe todos los parámetros
3. Actualizar el header para declarar esta función adicional

---

### 4. Inicialización en main() (PRIORIDAD ALTA)

**Archivo destino**: `/workspace/src/runtime/RuntimeInitManager.cpp`  
**Líneas a migrar desde main.cpp**: 1120-1494 (~375 líneas)

**Sub-secciones**:

#### 4.1 Carga de datos del campus (líneas 1120-1174)
```cpp
// MIGRAR A: RuntimeInitManager::loadCampusData()
- Lectura de campus.json
- Load del grafo
- Export del grafo generado
```

#### 4.2 Carga de SceneData (líneas 1183-1210)
```cpp
// MIGRAR A: RuntimeInitManager::loadSceneData()
- Loop sobre allScenes
- Carga de hitboxes desde TMJ
- Carga de spawns y floor triggers
- Carga de interest zones
```

#### 4.3 Setup de transiciones (líneas 1225-1366)
```cpp
// MIGRAR A: RuntimeInitManager::setupTransitions()
- Carga de portales desde TMJ
- Construcción de FloorElevator
- Registro de sceneLinks
```

#### 4.4 Inicialización runtime (líneas 1368-1494)
```cpp
// MIGRAR A: RuntimeInitManager::initRuntimeState()
- Creación de inputManager, runtimeNavigation, etc.
- Carga de escena inicial
- Setup de playerAnim y playerPos
- Setup de cámara
- Inicialización de tabState y routeState
```

---

### 5. Game Loop Principal (PRIORIDAD CRÍTICA)

**Archivo destino**: `/workspace/src/runtime/RuntimeGameManager.cpp`  
**Líneas a migrar desde main.cpp**: 1496-1936 (~441 líneas)

**Sub-secciones**:

#### 5.1 Update Loop (líneas 1496-1620)
```cpp
// MIGRAR A: RuntimeGameManager::update()
- Polling de input
- Toggle de menús
- Zoom de cámara
- Movimiento del jugador con colisiones
- Update de animación
- Refresh de ruta
- Update de transiciones
```

#### 5.2 Render Loop (líneas 1622-1930)
```cpp
// MIGRAR A: RuntimeGameManager::render()
- BeginDrawing / ClearBackground
- BeginMode2D
- Draw del mapa con hitboxes
- Draw de interest zones
- Draw de navigation overlay
- Draw de route path
- Draw de overlays DFS/alterno
- Draw de triggers (debug)
- Draw del jugador
- EndMode2D
- Minimap
- Fade overlay
- Prompt "Presiona E"
- HUD de coordenadas
- drawRaylibNavigationOverlayMenu
- drawRaylibInfoMenu
- rlImGuiBegin/End
- EndDrawing
```

#### 5.3 Cleanup (líneas 1934-1941)
```cpp
// MIGRAR A: RuntimeInitManager::unload() o directamente en main() simplificado
- UnloadTexture de mapData
- UnloadTexture de playerAnim
- rlImGuiShutdown
- CloseWindow
```

---

## ✅ Managers Ya Implementados

Estos archivos YA están creados y funcionales:

| Manager | Archivo | Estado |
|---------|---------|--------|
| RuntimeInputManager | `runtime/RuntimeInputManager.h/.cpp` | ✅ Completo |
| RuntimeNavigationManager | `runtime/RuntimeNavigationManager.h/.cpp` | ✅ Completo |
| RuntimeCameraManager | `runtime/RuntimeCameraManager.h/.cpp` | ✅ Completo |
| RuntimeMinimapManager | `runtime/RuntimeMinimapManager.h/.cpp` | ✅ Completo |
| RuntimeUiManager | `runtime/RuntimeUiManager.h/.cpp` | ✅ Completo |
| RuntimeTransitionManager | `runtime/RuntimeTransitionManager.h/.cpp` | ✅ Completo |
| RuntimePlayerManager | `runtime/RuntimePlayerManager.h/.cpp` | ✅ Completo |
| NavigationHelpers | `helpers/NavigationHelpers.h` | ⚠️ Header solo, falta .cpp |
| RuntimeOverlayManager | `runtime/RuntimeOverlayManager.h/.cpp` | ⚠️ Parcial (faltan funciones) |
| RuntimeInfoMenuManager | `runtime/RuntimeInfoMenuManager.h` | ⚠️ Header solo, falta .cpp |
| RuntimeInitManager | `runtime/RuntimeInitManager.h` | ⚠️ Header solo, falta .cpp |
| RuntimeGameManager | `runtime/RuntimeGameManager.h` | ⚠️ Header solo, falta .cpp |

---

## 🎯 Estructura Final Deseada de main.cpp

```cpp
#include <raylib.h>
#include "runtime/RuntimeInitManager.h"
#include "runtime/RuntimeGameManager.h"
// ... includes mínimos ...

int main(int argc, char* argv[]) {
    // 1. Setup inicial de ventana (~5 líneas)
    InitWindow(...);
    SetTargetFPS(60);
    rlImGuiSetup(true);
    
    // 2. Inicializar contexto del juego (~3 líneas)
    GameContext ctx;
    GameState state;
    RuntimeInitManager::Initialize(ctx, state, argc, argv);
    
    // 3. Game loop delegado (~10 líneas)
    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        const auto input = ctx.inputManager.poll(state.infoMenuOpen);
        RuntimeGameManager::Update(state, input, ctx, dt);
        RuntimeGameManager::Render(state, ctx);
    }
    
    // 4. Cleanup (~3 líneas)
    RuntimeInitManager::Unload(ctx, state);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
```

**Total líneas ejecutables estimadas**: ~25-30 líneas  
**Total incluyendo includes y declaraciones**: ~50-60 líneas

---

## 📝 Orden Recomendado de Implementación

1. **Primero**: `RuntimeInfoMenuManager.cpp` (es el bloque más grande aislado)
2. **Segundo**: `NavigationHelpers.cpp` (funciones helper reutilizables)
3. **Tercero**: `RuntimeInitManager.cpp` (prepara el terreno para main() delgado)
4. **Cuarto**: `RuntimeGameManager.cpp` (orquesta update/render)
5. **Quinto**: Refactorizar `main()` para usar los managers
6. **Sexto**: Mover `renderAcademicRuntimeOverlay` a `RuntimeOverlayManager`
7. **Séptimo**: Testing y ajuste fino

---

## ⚠️ Consideraciones Importantes

1. **NO modificar lógica de negocio**: Solo mover código, no cambiar comportamiento
2. **Mantener firmas originales**: Las funciones deben comportarse igual
3. **Comentarios de trazabilidad**: Cada función migrada debe incluir comentario "// MIGRADO DESDE main.cpp:LÍNEAS"
4. **No tocar core/graph/, repositories/, services/**: Solo consumir interfaces públicas
5. **Testing continuo**: Verificar compilación después de cada migración grande

---

## 🔍 Verificación Post-Migración

Después de completar todas las migraciones:

```bash
# Contar líneas ejecutables en main.cpp (excluyendo includes, comentarios, declaraciones)
grep -v "^[[:space:]]*//" main.cpp | grep -v "^[[:space:]]*$" | grep -v "^#" | grep -v "^[a-zA-Z_].*{.*}$" | wc -l

# Debe ser < 100
```
