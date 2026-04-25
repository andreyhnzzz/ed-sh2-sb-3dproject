# Especificación de Migración: RuntimeInfoMenuManager.cpp

## Funciones a migrar desde main.cpp

### 1. `drawRayButton` (Líneas 657-673)
**Responsabilidad:** Dibuja un botón rectangular con estados (normal, hover, active) y retorna si fue presionado.

```cpp
// MIGRADO DESDE main.cpp:657-673
static bool drawRayButton(const Rectangle& r, const char* label, int fontSize,
                          Color base, Color hover, Color active, Color textColor)
```

**Parámetros:**
- `Rectangle& r`: Posición y tamaño del botón
- `const char* label`: Texto del botón
- `int fontSize`: Tamaño de fuente
- `Color base, hover, active, textColor`: Colores para cada estado

**Retorna:** `bool` - true si el botón fue presionado

---

### 2. `studentTypeToLabel` (Líneas 675-683)
**Responsabilidad:** Convierte StudentType a string legible.

```cpp
// MIGRADO DESDE main.cpp:675-683
static const char* studentTypeToLabel(StudentType studentType)
```

---

### 3. `drawRaylibNavigationOverlayMenu` (Líneas 685-718)
**Responsabilidad:** Dibuja el overlay de navegación en la esquina superior izquierda cuando showNavigationGraph es true.

```cpp
// MIGRADO DESDE main.cpp:685-718
static void drawRaylibNavigationOverlayMenu(
    bool showNavigationGraph,
    bool infoMenuOpen,
    const std::string& currentSceneName,
    bool mobilityReduced,
    StudentType studentType,
    bool routeActive,
    float routeProgressPct,
    bool resilienceConnected,
    const TabManagerState& state,
    const std::vector<std::string>& blockedNodes)
```

**Muestra:**
- Nombre de escena actual
- Perfil del estudiante
- Estado de movilidad reducida
- Estado de conectividad
- Estado de ruta activa
- Progreso de ruta
- Nodos bloqueados
- Peso del último path

---

### 4. `drawRaylibInfoMenu` (Líneas 720-1114)
**Responsabilidad:** Dibuja el menú de información completo con dos paneles (izquierdo: ruta/navegación, derecho: control académico).

```cpp
// MIGRADO DESDE main.cpp:720-1114
static void drawRaylibInfoMenu(
    bool& isOpen,
    int screenWidth,
    int screenHeight,
    int& selectedRouteSceneIdx,
    bool& routeActive,
    std::string& routeTargetScene,
    float& routeProgressPct,
    float& routeTravelElapsed,
    bool& routeTravelCompleted,
    float& routeLegStartDistance,
    std::string& routeLegSceneId,
    std::string& routeLegNextSceneId,
    std::vector<std::string>& routeScenePlan,
    std::vector<Vector2>& routePathPoints,
    std::string& routeNextHint,
    float& routeRefreshCooldown,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    const std::function<std::string(const std::string&)>& sceneDisplayName,
    const CampusGraph& graph,
    const TraversalResult& dfsTraversal,
    const TraversalResult& bfsTraversal,
    int& rubricViewMode,
    int& graphPage,
    int& dfsPage,
    int& bfsPage,
    bool& showNavigationGraph,
    TabManagerState& state,
    const std::string& currentSceneName,
    bool showHitboxes,
    bool showTriggers,
    bool showInterestZones,
    ScenarioManager& scenarioManager,
    RuntimeBlockerService& runtimeBlockerService,
    const DestinationCatalog& destinationCatalog,
    int& selectedBlockedNodeIdx,
    int& selectedBlockedEdgeIdx,
    ResilienceService& resilienceService)
```

**Panel Izquierdo - "Route and Navigation":**
- Selector de destino con botones prev/next
- Botón "Draw Route"
- Botón "Clear"
- Lista del plan de escenas de la ruta

**Panel Derecho - "Academic Control":**
- Información de escena actual y toggles visuales
- Toggle de grafo de navegación
- Selectores de perfil de estudiante (New/Veteran/Disabled)
- Selector de nodo a bloquear
- Selector de edge a bloquear
- Botones: Bloquear Nodo, Bloquear Conexión, Limpiar Bloqueos
- Origen y destino académicos
- Información de resiliencia y traversal
- Resumen de ruta
- Pestañas de evidencia de rúbrica (Graph/DFS/BFS)
- Paginación de líneas de evidencia

---

## Dependencias requeridas en RuntimeInfoMenuManager.h

```cpp
#include <raylib.h>
#include <functional>
#include <string>
#include <vector>
#include "core/graph/CampusGraph.h"
#include "services/ScenarioManager.h"
#include "services/RuntimeBlockerService.h"
#include "services/DestinationCatalog.h"
#include "services/ResilienceService.h"
#include "ui/TabManager.h"

struct InfoMenuContext {
    // Estado mutable del menú
    bool isOpen = false;
    int selectedRouteSceneIdx = 0;
    int rubricViewMode = 0;  // 0=Graph, 1=DFS, 2=BFS
    int graphPage = 0;
    int dfsPage = 0;
    int bfsPage = 0;
    int selectedBlockedNodeIdx = 0;
    int selectedBlockedEdgeIdx = 0;
};

class RuntimeInfoMenuManager {
public:
    static bool drawRayButton(const Rectangle& r, const char* label, int fontSize,
                              Color base, Color hover, Color active, Color textColor);
    
    static const char* studentTypeToLabel(StudentType studentType);
    
    static void drawNavigationOverlay(
        bool showNavigationGraph,
        bool infoMenuOpen,
        const std::string& currentSceneName,
        bool mobilityReduced,
        StudentType studentType,
        bool routeActive,
        float routeProgressPct,
        bool resilienceConnected,
        const TabManagerState& state,
        const std::vector<std::string>& blockedNodes);
    
    static void drawInfoMenu(InfoMenuContext& ctx, /* ... todos los parámetros ... */);
};
```

---

## Líneas totales a extraer: ~395 líneas

| Función | Líneas inicio | Líneas fin | Total líneas |
|---------|---------------|------------|--------------|
| drawRayButton | 657 | 673 | 17 |
| studentTypeToLabel | 675 | 683 | 9 |
| drawRaylibNavigationOverlayMenu | 685 | 718 | 34 |
| drawRaylibInfoMenu | 720 | 1114 | 395 |
| **TOTAL** | | | **~455** |

---

## Notas de implementación

1. **No modificar lógica:** Copiar exactamente la lógica existente, solo cambiar `static` por métodos estáticos de clase.
2. **Colores hardcodeados:** Mantener los mismos valores RGB/alpha existentes.
3. **Funciones raylib:** Todas las llamadas a `DrawRectangle`, `DrawText`, `MeasureText`, etc. se mantienen igual.
4. **Referencias mutables:** Los parámetros como `bool& isOpen`, `int& selectedRouteSceneIdx` deben mantenerse como referencias para permitir modificación.
5. **Lambda `px`:** La función lambda `px` dentro de `drawRaylibInfoMenu` debe mantenerse idéntica para escalado UI.
