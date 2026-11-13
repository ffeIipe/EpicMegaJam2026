# Director's Cut: notas para agentes

- Reglas del juego y contenido de cada pantalla: `SystemsDesigns.md` (la UI está en la sección 10). Texto del juego en inglés; comentarios y nombres en inglés.
- Engine en `C:\UnrealVersions\UE_5.8`. Compilar con el editor cerrado:
  `C:\UnrealVersions\UE_5.8\Engine\Build\BatchFiles\Build.bat EpicMegaJam2026Editor Win64 Development -Project=<ruta>\EpicMegaJam2026.uproject -WaitMutex`

## UI: CommonUI, todo armado en C++

### Dónde está cada cosa

| Archivo | Contenido |
|---|---|
| `Source/EpicMegaJam2026/UI/DCCommonUI.h/.cpp` | Piezas compartidas: paleta `DCPalette`, estilos `UDCText_*` / `UDCBorder_*` / `UDCButton_*`, acciones de input `UDCUIInputData`, botón `UDCButton`, hint de tecla `UDCActionButton`, base de pantallas `UDCScreen`, `UDCRootLayout` (stack de pantallas + barra de teclas) y los helpers `DCUI::` |
| `UI/DCWidgets.*` | Inventario (pestañas + filas/grilla), Combine VHS, diálogo, panel de pedido |
| `UI/DCMenuWidgets.*` | Proveedor, estantería, resumen del día, menú principal, pausa, cartel de tutorial |
| `UI/DCHudWidgets.*` | HUD (con hotbar), toast de reseñas, pantallas finales, etiqueta 3D de los props |
| `Core/DCPlayerController.*` | Crea los widgets, abre y cierra pantallas |
| `/Game/DirectorsCut/UI/Fonts` | Press Start 2P y VT323 (fuente: `SourceArt/Fonts`, licencia OFL) |
| `/Game/DirectorsCut/UI/Textures` | Texturas 9-slice generadas por `SourceArt/UI/gen_ui_textures.js` |
| `/Game/DirectorsCut/UI/WBP_*` | Subclases vacías de las clases C++, asignadas en `BP_DC_PlayerController` |

### Reglas

1. **Armar widgets solo con las piezas compartidas.** El árbol se construye en `NativeOnInitialized` con `DCUI::Panel`, `Header`, `Text`, `Border`, `Button` y `Bar`. No usar `UTextBlock`, `UButton` ni `UBorder` sueltos, ni colores o fuentes literales: siempre `UCommonTextBlock`, `UCommonBorder` o `UDCButton` con un estilo, y colores de `DCPalette`.
2. **Si falta un estilo, crear uno nuevo** (`UDCText_X`, `UDCBorder_X`, `UDCButton_X` en `DCCommonUI`, valores en el constructor). No modificar un estilo existente para resolver un caso puntual: lo usa toda la UI.
3. **Elegir la base según el tipo de widget:**
   - **Pantalla** (se abre por una interacción y se cierra con Back): heredar de `UDCScreen`. Poner `bPausesGame = true` en los menús y `bIsBackHandler = false` si Esc no debe cerrarla. Se abre desde el player controller con `PushScreen<T>(RootLayout, TClass)`, que reemplaza la pantalla actual. Se cierra con `DeactivateWidget()` desde el widget o con `CloseScreen()` desde el controller. Agregar en el controller un `TSubclassOf<T>` con `EditAnywhere` para poder asignar un `WBP_`.
   - **Overlay pasivo** (HUD, toasts, carteles): `UUserWidget` creado con `MakeWidget` en `ADCPlayerController::BeginPlay`, con visibilidad `HitTestInvisible` o `SelfHitTestInvisible`.
   - **Pieza reutilizable dentro de una pantalla** (como el inventario): `UCommonUserWidget`, que puede registrar acciones de input.
   - El HUD es el widget activable que devuelve el input al juego cuando no hay pantallas: no desactivarlo ni moverlo al stack.
4. **Las pantallas se configuran solas.** En `NativeOnInitialized` leen el GameMode (`GetWorld()->GetAuthGameMode<ADCGameMode>()`) y se suscriben a los dispatchers de los componentes. Nunca modifican estado directamente: llaman funciones de los componentes. El stack reutiliza instancias, así que:
   - `NativeOnInitialized` corre una sola vez por instancia.
   - Los datos que cambian se refrescan en `NativeOnActivated`.
   - Los delegates que el controller conecta al abrir la pantalla van con `AddUniqueDynamic`.
5. **El input de la UI va por CommonUI, nunca por `BindKey`.**
   - Las acciones se definen en el constructor de `UDCUIInputData`: nombre, texto, tecla, botón de gamepad y texto del keycap.
   - Se registran con `RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("Nombre"), bMostrarEnBarra, delegate))`.
   - La barra de abajo a la izquierda muestra sola las acciones de la pantalla activa, y Back ya viene incluido en `UDCScreen`.
   - Para CommonUI, las acciones se llaman `Legacy_Actions_<Fila>`.
   - El cursor, la captura del mouse y el bloqueo del input de juego los maneja `GetDesiredInputConfig`: no tocarlos a mano.
6. **Assets que se leen al arrancar.** Los constructores de estilos, las fuentes y las texturas se cargan una sola vez cuando arranca el editor. Si cambian, hay que reiniciarlo: Live Coding no los aplica. Los cambios en cuerpos de funciones sí entran por Live Coding, pero se pierden al cerrar el editor hasta que se haga una compilación normal.

### Trampas de CommonUI que ya nos pasaron

- `SetIsSelected(false)` no deselecciona un botón que no es *toggleable*: usar `ClearSelection()` (helper `SetSelected` en `DCWidgets.cpp`).
- `UDCButton` arma su árbol (`Content` + `Label`) antes de que `UCommonButtonBase` lo envuelva. Para agregar contenido (portada, estrellas), meterlo en `Btn->Content`. El estilo de texto se aplica en `NativeOnCurrentTextStyleChanged`.
- El stack de pantallas no tiene transición, a propósito: así `ClearWidgets` + `AddWidget` reemplazan la pantalla en el momento y la anterior no reaparece abajo.
- `ActionButtonClass` de `UCommonBoundActionBar` es privado: se asigna por reflexión en `UDCRootLayout`.
- Incluir `Input/CommonUIInputTypes.h` exige el módulo `GameplayTags` en `Build.cs` (ya está).

### Look

- **Tipografía.** Press Start 2P para títulos, botones, estrellas ★ y flechas ◀▶ (VT323 no tiene esos glifos); VT323 para el cuerpo. Con `FontDPI=72` los tamaños son píxeles, y Press Start 2P se ve nítida en múltiplos de 8.
- **Texturas 9-slice.** Se usan en `UDCBorder_Panel`, `Header` e `Inset` y en `UDCButton_Primary` y `Tab`. La plaqueta es gris y se tiñe con el color del estilo. Si cambiás el generador, los tamaños y márgenes de `SliceBrush` y `Plate` en `DCCommonUI.cpp` tienen que coincidir con él.
  - El padding del contenido tiene que ser mayor o igual al grosor del marco: 10 px en el panel, 8 px en la plaqueta.
  - El alto tiene que ser mayor o igual al doble del margen: 40 px en el panel, 28 px en la plaqueta (por eso los botones tienen `MinHeight = 30`).
  - La placa de título es de una sola línea y usa `DCUI::HeaderPadding`.
- **Layout.** La referencia es 1080p. Las pantallas van centradas y con un ancho máximo de unos 1250 px, para no tapar la caja de plata del HUD (arriba a la derecha) ni la barra de teclas (abajo a la izquierda).
- **WBP_.** Son subclases vacías. Si alguien arma un árbol en el diseñador UMG, el `NativeOnInitialized` de C++ lo reemplaza.

### Checklist para un widget nuevo

1. Elegir el archivo `UI/DC*Widgets.h` que corresponda y la clase base según la regla 3.
2. Armar el árbol con `DCUI::` en `NativeOnInitialized`, con texto en inglés.
3. Si es una pantalla: agregar el `TSubclassOf` y el `PushScreen` en `ADCPlayerController`, crear el `WBP_` vacío y asignarlo en `BP_DC_PlayerController`.
4. Si necesita teclas propias: agregarlas en `UDCUIInputData` y registrarlas en el widget.
5. Compilar y probarlo en PIE (abrir, cerrar con Back, que el juego vuelva a tener el mouse). En PIE dentro del editor, Escape termina la sesión: para probar Back, usar el botón B del gamepad o hacer click en el hint de la barra.
