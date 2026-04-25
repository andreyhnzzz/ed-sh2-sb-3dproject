# Migración de main.cpp a Managers - Resumen

## Estado Actual: main.cpp tiene 1944 líneas

## Managers Creados

### 1. RuntimeInputManager (YA EXISTÍA)
- **Archivo**: `runtime/RuntimeInputManager.h/.cpp`
- **Responsabilidad**: Polling de input del jugador
- **Líneas migradas**: ~1493-1509

### 2. RuntimeNavigationManager (YA EXISTÍA)  
- **Archivo**: `runtime/RuntimeNavigationManager.h/.cpp`
- **Responsabilidad**: Gestión de rutas runtime, refresh de ruta
- **Líneas migradas**: ~1537-1562

### 3. RuntimeOverlayManager (CREADO)
- **Archivo**: `runtime/RuntimeOverlayManager.h/.cpp`
- **Responsabilidad**: Renderizar overlays de navegación
- **Líneas migradas**: ~267-355, 681-718

### 4. RuntimeInfoMenuManager (PARCIAL)
- **Archivo**: `runtime/RuntimeInfoMenuManager.h/.cpp`
- **Responsabilidad**: Menú de información con raylib
- **Líneas migradas**: Header declarado, implementación pendiente manual
- **Ver migration.md para detalles**

### 5. RuntimeUiManager (CREADO)
- **Archivo**: `runtime/RuntimeUiManager.h/.cpp`
- **Responsabilidad**: UI fija (coordenadas, hints, fade overlay)
- **Líneas migradas**: ~1823-1848

### 6. RuntimeTransitionManager (CREADO)
- **Archivo**: `runtime/RuntimeTransitionManager.h/.cpp`  
- **Responsabilidad**: Transiciones entre escenas
- **Líneas migradas**: ~1572-1607, 1823-1837, 1926

### 7. RuntimeMinimapManager (YA EXISTÍA)
- **Archivo**: `runtime/RuntimeMinimapManager.h/.cpp`
- **Responsabilidad**: Minimap estilo Waze
- **Líneas migradas**: ~1727-1821

### 8. RuntimePlayerManager (CREADO)
- **Archivo**: `runtime/RuntimePlayerManager.h/.cpp`
- **Responsabilidad**: Movimiento, colisiones y animación del jugador
- **Líneas migradas**: ~1506-1534, 1609-1620, 1696-1721

### 9. RuntimeCameraManager (YA EXISTÍA)
- **Archivo**: `runtime/RuntimeCameraManager.h/.cpp`
- **Responsabilidad**: Cámara 2D, zoom, clamping
- **Líneas migradas**: ~636-655, 1439-1445, 1501-1504, 1535

### 10. RuntimeInitManager (YA EXISTÍA - PENDIENTE IMPLEMENTAR)
- **Archivo**: `runtime/RuntimeInitManager.h/.cpp`
- **Responsabilidad**: Inicialización completa del juego
- **Líneas a migrar**: ~1116-1490

### 11. RuntimeAnimationManager (YA EXISTÍA)
- **Archivo**: `runtime/RuntimeAnimationManager.h/.cpp`
- **Responsabilidad**: Animaciones de sprites
- **Líneas migradas**: Parte de animación de jugador

## Líneas Pendientes de Migración Masiva

### Bloque 1: Funciones Helper Estáticas (líneas 43-265)
- `isOverlayEdgeAllowed`, `pathContainsDirectedStep`, `collectVisualPoiNodes`
- `countProfileDiscardedEdges`, `buildSelectionCriterion`, `playerFrontAnchor`
- `refreshSceneHitboxes`, `mergeProfiledSegments`, `runProfiledDfsPath`
- `runProfiledAlternatePath`, `comboSelectNode`, `linkMatchesEdgeType`
- `findBestEdgeForLink`, `buildOverlayPathForScene`
- **Acción**: Mover a `NavigationHelpers.cpp` existente o crear `RuntimeGraphHelpers`

### Bloque 2: renderAcademicRuntimeOverlay (líneas 353-634)
- **~282 líneas**
- **Acción**: Mover a `RuntimeOverlayManager` como método estático adicional

### Bloque 3: drawRaylibInfoMenu (líneas 720-1114)
- **~395 líneas**
- **Acción**: Implementar manualmente en `RuntimeInfoMenuManager.cpp`

### Bloque 4: main() - Inicialización (líneas 1116-1490)
- **~375 líneas**
- **Acción**: Delegar a `RuntimeInitManager::loadCampusData()`, `loadSceneData()`, `setupTransitions()`, `initRuntimeState()`

### Bloque 5: main() - Game Loop (líneas 1491-1932)
- **~442 líneas**
- **Acción**: Refactorizar para delegar update() y render() a managers

## Objetivo Final

**main.cpp debe quedar con < 100 líneas de lógica ejecutable:**

```cpp
int main() {
    // 1. Init window (~5 líneas)
    // 2. RuntimeInitManager::Initialize(gameContext) (~2 líneas)
    // 3. While loop:
    //    - Input polling delegado
    //    - Update managers delegados
    //    - Render managers delegados
    // 4. Cleanup (~3 líneas)
}
```

## Próximos Pasos

1. **Completar RuntimeInfoMenuManager.cpp** manualmente (ver migration.md)
2. **Implementar RuntimeInitManager.cpp** - migrar toda la inicialización
3. **Refactorizar game loop** en main.cpp para usar managers
4. **Mover funciones helper** a clases especializadas
5. **Verificar compilación** y comportamiento idéntico
