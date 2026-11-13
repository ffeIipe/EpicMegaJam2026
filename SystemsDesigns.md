# Director's Cut: documento de diseño de sistemas

Nov 12, 2026 · @Felipe

## 0. Instrucciones para Claude (leer primero)

Este documento es la fuente de verdad del juego para la UE MegaJam 2026 (tema: **Director's Cut**, entrega el domingo). Implementá exactamente lo que dice, en el orden de la sección 12, sin agregar sistemas, mecánicas ni contenido que no esté escrito acá.

1. **No inventes.** Si algo no está definido, no lo resuelvas por tu cuenta: dejá un `TODO(DESIGN):` en el Blueprint y avisá en tu respuesta. Las preguntas abiertas están en la sección 12.
2. **Ningún número va hardcodeado.** Todo valor numérico o de contenido sale de `DA_GameConfig` o de una Data Table (sección 11). Los valores de este documento son iniciales y se van a balancear.
3. **Blueprints y C++ habilitados**, Unreal Engine 5.8.4, proyecto Blank. Podés usar C++ cuando convenga (lógica compleja, rendimiento, clases base, structs y enums), pero todo parámetro que pueda necesitar ajuste tiene que quedar modificable desde el editor sin recompilar: exponelo con `UPROPERTY(EditAnywhere, BlueprintReadWrite)` o leélo de `DA_GameConfig` o de una Data Table. Las funciones que use la UI o los Blueprints van con `UFUNCTION(BlueprintCallable)`. No uses plugins de terceros salvo que el equipo lo pida (única excepción ya aprobada: FMOD).
4. **Respetá los nombres** de clases, structs, enums y carpetas de la sección 2. No renombres ni reorganices assets existentes.
5. **Un sistema por vez.** Terminá cada paso de la sección 12, dejalo compilando y testeable en PIE, y recién ahí pasá al siguiente.
6. **Texto del juego en inglés** (UI, diálogos, nombres de VHS). Comentarios y nombres de variables en inglés.
7. **Arte y audio no son tu tarea.** Usá placeholders (cubos, colores por género, textos) y dejá las referencias de mesh y sonido como variables asignables en las Data Tables.
8. Los **atributos de los clientes son invisibles para el jugador**. Solo existen en datos y lógica. El jugador los deduce por el diálogo.

## 1. Visión y loop principal

El jugador es un cineasta frustrado que atiende un videoclub. Los clientes piden películas que no existen y él las arma mezclando tres VHS en el depósito: su propio *director's cut*. Si el cliente queda conforme, paga y deja propina. Si no, llega una queja con multa. Es un juego **incremental**: se pierde si no se puede pagar la renta del local y se gana al juntar el presupuesto para filmar una película propia.

**Pilares (no negociables):**

- Hook en 15 a 20 segundos: el primer cliente aparece apenas empieza el Día 1 con un pedido de un solo atributo.
- Deducción, no adivinanza: el diálogo siempre da pistas suficientes sobre los atributos ocultos del cliente.
- Presión de tiempo: cada cliente se va a los 90 segundos y se puede formar fila.
- Estética low poly, texturas hand-painted de 1K, VHS con un color por género.

**Loop de un cliente:**

1. **Llegada:** el cliente entra, recorre un estante (cosmético) y se suma a la fila del mostrador.
2. **Pedido:** al llegar al frente, dice su pedido con frases que dejan entrever sus atributos.
3. **Elección:** el jugador va al depósito y elige 3 VHS distintos para Intro, Nudo y Desenlace.
4. **Creación:** el jugador le pone título a la película y la confirma.
5. **Entrega:** vuelve al mostrador, entrega la película y cobra el precio base del alquiler.
6. **Crítica online:** unos segundos después llega la reseña: propina si le encantó o le gustó, nada si fue neutral, multa si la odió.
7. **Reinversión:** con la plata se compran catálogos nuevos de VHS en el menú del proveedor.

**Loop del día:** Día (5 min) → Noche (5 min, clientes raros) → cobro de la renta → resumen → día siguiente. Si no alcanza para la renta: Game Over. Si la plata llega a la meta del presupuesto: Victoria.

## 2. Arquitectura técnica

Toda la lógica de reglas vive en componentes del GameMode. Los actores del mundo y la UI solo escuchan Event Dispatchers y llaman funciones de esos componentes, nunca modifican estado directamente.

**Carpetas** (raíz `/Game/DirectorsCut/`): `Core/`, `Components/`, `Data/` (structs, enums, Data Tables, Data Asset), `Characters/`, `Props/`, `UI/`, `Levels/`, `Cinematics/`, `Audio/`, `Art/`.

**Prefijos:** `BP_` Blueprint actor · `AC_` Actor Component · `WBP_` widget · `S_` struct · `E_` enum · `DT_` Data Table · `PDA_` Primary Data Asset (clase) · `DA_` instancia de Data Asset · `BPFL_` Function Library · `LS_` Level Sequence · `SM_`/`SK_`/`M_`/`T_` arte.

**Nivel:** un solo mapa `L_VideoStore` con tres zonas: local (mostrador, estantes, fila, puerta), depósito (estanterías y cajas) y un exterior simple (calle, árboles, poste).

| Clase | Responsabilidad |
| --- | --- |
| `BP_DC_GameMode` | Contiene los componentes de reglas y referencia a `DA_GameConfig`. Arranca el Día 1. |
| `AC_Economy` | Plata actual, cobros, propinas, multas, renta, meta de victoria. |
| `AC_DayNightCycle` | Fase actual (Día/Noche), timer, número de día, iluminación. |
| `AC_ClientDirector` | Spawneo de clientes según el día, fila, paciencia, generación de atributos. |
| `AC_Inventory` | VHS y catálogos que tiene el jugador, compra de catálogos. |
| `AC_FilmCrafting` | Slots Intro/Nudo/Desenlace, título, película en mano. |
| `BPFL_Scoring` | Funciones puras de puntuación (sección 6). Sin estado. |
| `BP_DC_PlayerController` | Input, mirar con el mouse, cambio de zona, abrir menús. |
| `BP_CounterCamera` | Pawn sin cuerpo con cámara fija y rotación limitada. |
| `BP_Client` | NPC: movimiento, estado, paciencia visible (opcional), diálogo. |
| `BP_ClientQueue` | Puntos de fila ordenados frente al mostrador. |
| `BP_BrowsePoint` | Punto de estante donde un cliente se queda mirando antes de hacer fila. |
| `BP_VHSTape` | VHS 3D, toma su color de `DT_Genres`. |
| `BP_HUD` + widgets | Ver sección 10. |

**Event Dispatchers principales:** `OnMoneyChanged(NewMoney, Delta)`, `OnPhaseChanged(Phase, DayIndex)`, `OnPhaseTimeTick(SecondsLeft)`, `OnClientReachedCounter(Client)`, `OnClientLeft(Client, bServed)`, `OnFilmCreated(Film)`, `OnReviewReceived(Review)`, `OnRentCharged(Amount)`, `OnGameOver()`, `OnVictory()`, `OnInventoryChanged()`.

## 3. Géneros y atributos

Cada atributo de cliente es su relación con un género: le gusta (peso positivo) o lo rechaza (peso negativo). "Miedo" es el nombre de diseño del rechazo a Terror. Los VHS tienen valores en esos mismos géneros, así que todo el sistema compara género contra género.

**Enum `E_Genre`** (7 valores; agregar uno nuevo solo requiere sumar una fila a `DT_Genres` y frases a `DT_HintLines`):

| `E_Genre` | Nombre en juego | Color del VHS | Atributo negativo (diseño) | Atributo positivo (diseño) |
| --- | --- | --- | --- | --- |
| `Horror` | Horror | Rojo oscuro | Miedo | Fan del terror |
| `Comedy` | Comedy | Amarillo | Serio | Gracioso |
| `Romance` | Romance | Rosa (del board) | Cínico | Romántico |
| `Action` | Action | Naranja | Tranquilo | Adrenalínico |
| `SciFi` | Sci-Fi | Azul | Escéptico | Nerd |
| `Drama` | Drama | Violeta | Evita llorar | Sensible |
| `Paranormal` | Paranormal | Verde (del board) | Supersticioso | Ocultista |

Los colores son propuestos (salvo rosa y verde, que están en el board) y viven en `DT_Genres`, así que el equipo de arte los puede cambiar sin tocar lógica.

**Reglas:**

- Un cliente nunca tiene dos atributos sobre el mismo género.
- Los nombres de atributo (Miedo, Romántico, etc.) son solo para diseño y debug. El jugador nunca los ve escritos.
- En builds de desarrollo, una tecla de debug (`F1`) muestra sobre cada cliente sus atributos y pesos. Se desactiva en el build de entrega.

## 4. VHS y combinación

Una película se arma con 3 VHS distintos, uno por slot (Intro, Nudo, Desenlace). Las cintas **no se gastan**: quedan en el inventario y se pueden volver a usar en la próxima película.

**Cada VHS (`S_VHSData`, fila de `DT_VHS`) tiene:**

- `Title` (Text): título de la película original, en inglés y ficticio.
- `PrimaryGenre` (`E_Genre`): define el color del VHS y la categoría en la lista del depósito.
- `GenreValues` (Array de `S_GenreValue`): de 1 a 3 entradas `{Genre, Value}`, `Value` entero de 1 a 3. Siempre incluye el `PrimaryGenre` con el valor más alto. Ejemplo: *"Love from Mars"* = Romance 3, SciFi 1.
- `Rating` (entero de 1 a 5): calidad de la cinta, visible para el jugador como estrellas.
- `CatalogId` (Name): catálogo al que pertenece.

**Qué ve el jugador de cada VHS:** título, color, género principal y estrellas. Los `GenreValues` secundarios **no se muestran** con números: se infieren por el título y el color. Si el equipo quiere mostrar íconos de géneros secundarios, se activa con `bShowSecondaryGenres` en `DA_GameConfig` (valor inicial: `false`).

**Reglas de armado (`AC_FilmCrafting`):**

1. Los tres slots deben tener VHS distintos (no se puede repetir la misma cinta).
2. Se permite repetir género entre slots (por ejemplo, dos cintas de Horror distintas).
3. Con los 3 slots llenos se habilita el campo de título (máximo `MaxFilmTitleLength` caracteres, inicial 30). Si queda vacío, se usa `"Untitled Cut #N"`.
4. Al confirmar se crea un `S_CreatedFilm` = {`Title`, `IntroRow`, `MiddleRow`, `EndingRow`, `GenreProfile`, `AverageRating`} y pasa a ser la **película en mano**.
5. Solo se puede tener `MaxHeldFilms` películas en mano a la vez (inicial 1). Si ya tenés una, el botón de confirmar queda deshabilitado hasta entregarla o descartarla.
6. Descartar la película en mano no tiene costo y libera el slot.

**Perfil de género de la película (`GenreProfile`):** para cada género, la suma de los valores de las tres cintas multiplicados por el multiplicador del slot donde están.

```latex
Profile[g] = \sum_{s \in \{Intro, Nudo, Desenlace\}} Value_{s}[g] \times SlotMultiplier_{s}
```

Los `SlotMultiplier` arrancan todos en 1.0 en `DA_GameConfig`, así que inicialmente el orden no importa. Si el equipo quiere que el Desenlace pese más, se cambia ese valor sin tocar lógica.

El título que pone el jugador **no afecta la puntuación**. Solo aparece en la entrega y en la crítica online.

## 5. Clientes (NPCs)

Hay tres categorías de cliente que se diferencian por cuántos atributos ocultos tienen, y se desbloquean a medida que avanzan los días. El sistema se inspira en el de ingredientes de Schedule I, pero más simple: atributos fijos por cliente, comparados contra el perfil de género de la película.

### 5.1 Categorías (`DT_ClientCategories`)

| Categoría (`E_ClientCategory`) | Atributos | Peso por atributo | Multiplicador de pago | Disponible desde |
| --- | --- | --- | --- | --- |
| `Casual` | 1 | ±2 | 1.0 | Día 1 |
| `Enthusiast` | 2 | ±1 a ±3 | 1.5 | Día 2 |
| `Cinephile` | 3 | ±1 a ±3 | 2.0 | Día 4 |

Todos los valores de esta tabla son iniciales. La categoría cumple el rol de la "Exquisitez" del board: cuanto más exquisito, más atributos y mejor paga.

### 5.2 Generación de atributos

Al spawnear, `AC_ClientDirector` le asigna atributos al cliente así:

1. Elige un arquetipo visual de `DT_ClientArchetypes` (mesh, nombre de debug, fase en la que aparece) según su `SpawnWeight`.
2. Elige una categoría según los pesos de `DT_DayProgression` para el día actual.
3. Si el arquetipo tiene `FixedPreferences` cargadas, usa esas. Si no, genera `NumAttributes` atributos: géneros distintos al azar dentro de `AllowedGenres` del arquetipo, cada uno con signo positivo o negativo (probabilidad `PositiveChance`, inicial 0.6) y peso dentro del rango de la categoría.
4. Guarda el resultado en `Preferences` (Array de `S_GenrePreference` = {`Genre`, `Weight`}).

Para que el Casual del Día 1 enseñe la mecánica, el **primer cliente de la partida** usa siempre el arquetipo `TutorialClient` con `FixedPreferences` = Horror −2 (el ejemplo de "Miedo" del diseño).

### 5.3 Clientes de noche

Durante la Noche spawnean arquetipos con `Phase` = `Night` o `Any`. Los de noche son los "clientes raros" del board: máscaras, look tipo *Eyes Wide Shut* y `AllowedGenres` con más peso en Paranormal, Horror y SciFi. Nada sexual ni explícito, para cumplir las reglas de la jam. Mecánicamente funcionan igual que los de día.

### 5.4 Diálogo de pistas

El pedido revela los atributos de forma indirecta, para que el jugador razone. Se arma concatenando una frase por atributo, sacada de `DT_HintLines`.

- Cada fila de `DT_HintLines` = {`Genre`, `Polarity` (`Likes`/`Dislikes`), `Text`}. Hay al menos 3 frases por combinación género y polaridad, y se elige una al azar sin repetir en el mismo día.
- Las frases nunca nombran el género. Ejemplos para cargar: Horror/Dislikes: *"I want to sleep tonight, if you know what I mean."* · Romance/Likes: *"Something to watch with my date. Make it sweet."* · Comedy/Likes: *"It's been a rough week. I need to laugh."* · SciFi/Likes: *"Anything with spaceships? Robots? I'm not picky."*
- Formato: saludo de `DT_Greetings` + frases de atributo + cierre. El jugador responde con un único botón *"Coming right up"* (no hay árbol de diálogo).
- El pedido queda escrito en un panel del HUD mientras el cliente siga en el local, para poder releerlo desde el depósito.

### 5.5 Fila, paciencia y estados

- La fila tiene `MaxQueueSize` lugares (inicial 3), definidos por `BP_ClientQueue`. Si está llena, el director no spawnea más hasta que se libere un lugar.
- La paciencia arranca en `ClientPatienceBaseSec` (inicial 60 s), sube con Criterio (sección 6.4) hasta `ClientPatienceMaxSec` (**90 s**, tope pedido por el equipo) y corre desde que el cliente **llega al mostrador**. Mientras espera en la fila no corre.
- El tiempo sigue corriendo cuando el jugador está en el depósito.
- Si la paciencia llega a 0, el cliente se va sin pagar. No hay multa (`LeavePenalty` inicial 0) y cuenta como "cliente perdido" en el resumen del día.
- Siempre se le entrega al cliente que está en el frente.

| Estado (`E_ClientState`) | Qué hace | Pasa a |
| --- | --- | --- |
| `Entering` | Camina de la puerta a un `BP_BrowsePoint` libre. | `Browsing` |
| `Browsing` | Animación de mirar el estante durante `BrowseTimeSec` (inicial 3 a 6 s, al azar). Es cosmético. | `Queued` |
| `Queued` | Ocupa el último lugar libre de la fila y avanza cuando se libera el de adelante. | `AtCounter` |
| `AtCounter` | Dice el pedido. Arranca la paciencia. | `Served` o `Leaving` |
| `Served` | Recibe la película, paga el precio base, dice una frase de despedida. | `Leaving` |
| `Leaving` | Camina a la puerta y se destruye. Si fue servido, programa la crítica. | (fin) |

Si termina la Noche, todos los clientes que siguen en el local pasan a `Leaving` sin penalidad.

## 6. Puntuación y recompensa

La reacción del cliente sale de comparar sus atributos con el perfil de género de la película. Todo se calcula en `BPFL_Scoring` con funciones puras, en el momento de la entrega.

### 6.1 Puntaje por atributo

- **Atributo positivo** (`Weight > 0`): aporta `Weight × Profile[Genre]`.
- **Atributo negativo** (`Weight < 0`): si la película tiene ese género (`Profile[Genre] > 0`) aporta `Weight × Profile[Genre]` (negativo). Si no lo tiene, aporta `AvoidBonus` (inicial +3). Así, esquivar lo que el cliente odia también se premia, y un Casual con un solo atributo negativo puede quedar conforme.

### 6.2 Puntaje final

```latex
Raw = \sum_{a \in Preferences} AttributeScore(a)
```

```latex
Score = \frac{Raw}{NumAttributes} \times RatingFactor
```

- Se divide por la cantidad de atributos para que los umbrales sirvan igual para las tres categorías.
- `RatingFactor = 1 + (AverageRating − 3) × RatingStep` (`RatingStep` inicial 0.1, o sea de 0.8 a 1.2). **Solo se aplica si `Raw > 0`**: una buena cinta mejora una película acertada pero no empeora una equivocada.

### 6.3 Reacción y plata

| Reacción (`E_Reaction`) | Condición (inicial) | Al entregar | Crítica online |
| --- | --- | --- | --- |
| `Loved` | Score ≥ 8 | `BaseRentalPrice` (10) | Propina = Score × `MoneyPerPoint` (2) × multiplicador de categoría, redondeado. +1 Criterio. |
| `Liked` | 3 ≤ Score < 8 | `BaseRentalPrice` | Propina con la misma fórmula. |
| `Neutral` | 0 ≤ Score < 3 | `BaseRentalPrice` | Sin propina. |
| `Hated` | Score < 0 | `BaseRentalPrice` | Multa = `ComplaintFine` (15) × multiplicador de categoría. |

- La crítica llega `ReviewDelaySec` después de que el cliente se va (inicial 15 s) como notificación de la página ficticia de reseñas (sección 10), con el título de la película y una frase de `DT_ReviewLines` según la reacción.
- La plata puede quedar negativa por multas. Solo se pierde en el cobro de la renta (sección 7).

**Ejemplos con valores iniciales** (Casual, peso ±2):

- Cliente con Miedo (Horror −2). Película con una cinta Horror 3: Raw = −2 × 3 = −6 → `Hated`, multa 15.
- El mismo cliente, película sin nada de Horror: Raw = +3 (AvoidBonus) → `Liked`, propina 3 × 2 × 1.0 = 6.
- Cliente Gracioso (Comedy +2). Película con Comedy 3 + Comedy 2 + Romance 3, rating promedio 4: Raw = 2 × 5 = 10, Score = 10 × 1.1 = 11 → `Loved`, propina 22.

### 6.4 Criterio

Cada `Loved` suma 1 punto de Criterio. Cada `CriterioPerLevel` puntos (inicial 5) la paciencia de todos los clientes sube `CriterioPatienceBonusSec` (inicial 10 s), sin pasar de `ClientPatienceMaxSec` (90 s). El Criterio no se gasta ni se pierde.

## 7. Economía, derrota y victoria

Una sola moneda: dólares (`Money`, entero). Entra por alquileres y propinas. Sale por multas, renta y compra de catálogos.

**Derrota:** al terminar cada Noche, `AC_Economy` cobra la renta del día (`RentAmount` de `DT_DayProgression`). Si `Money < RentAmount` en ese momento: `OnGameOver` y pantalla de derrota (*"Eviction notice"*). Si alcanza, se descuenta y sigue el resumen del día.

**Victoria:** apenas `Money ≥ MovieBudgetGoal` (inicial 400), se dispara `OnVictory`: el juego se pausa y aparece la pantalla final (*"Your Director's Cut gets greenlit"*). Se chequea cada vez que cambia la plata.

**Plata inicial:** `StartMoney` = 50.

### 7.1 Catálogos (`DT_Catalogs`)

Un catálogo es un paquete de VHS que se compra una sola vez desde el menú del proveedor (accesible desde el mostrador). Al comprarlo, sus VHS se suman al inventario al instante y aparecen cajas nuevas en el depósito (cosmético).

| Catálogo | Géneros | VHS | Precio | Disponible desde |
| --- | --- | --- | --- | --- |
| Starter Shelf | Todos | 9 | Ya lo tenés | Día 1 |
| Date Night | Romance, Comedy | 5 | 60 | Día 1 |
| Blockbusters | Action, SciFi | 5 | 80 | Día 2 |
| Tearjerkers | Drama, Romance | 5 | 80 | Día 2 |
| Midnight Collection | Horror, Paranormal | 5 | 100 | Día 3 |
| Cult Classics | Mezcla, rating 4 a 5 | 6 | 150 | Día 4 |

**Catálogo inicial (cargar tal cual en `DT_VHS`):**

| Row Name | Title | PrimaryGenre | GenreValues | Rating |
| --- | --- | --- | --- | --- |
| `VHS_Starter_01` | Love from Mars | Romance | Romance 3, SciFi 1 | 3 |
| `VHS_Starter_02` | Night of the Hollow | Horror | Horror 3 | 2 |
| `VHS_Starter_03` | Grandpa Goes Wild | Comedy | Comedy 3 | 3 |
| `VHS_Starter_04` | Fist of Thunder | Action | Action 3 | 2 |
| `VHS_Starter_05` | Orbit Zero | SciFi | SciFi 3, Action 1 | 3 |
| `VHS_Starter_06` | Rain on Sunday | Drama | Drama 3, Romance 1 | 4 |
| `VHS_Starter_07` | The Whispering House | Paranormal | Paranormal 3, Horror 1 | 3 |
| `VHS_Starter_08` | Laugh Track Massacre | Comedy | Comedy 2, Horror 2 | 2 |
| `VHS_Starter_09` | Alien Honeymoon | SciFi | SciFi 2, Romance 2, Comedy 1 | 3 |

Los VHS de los otros cinco catálogos los define el equipo. Hasta entonces, creá las filas con título `"TODO Title NN"`, el género principal del catálogo con valor 3 y rating 3.

### 7.2 Progresión por día (`DT_DayProgression`)

| Día | Clientes de día | Clientes de noche | Renta | Pesos de categoría (Casual / Enthusiast / Cinephile) |
| --- | --- | --- | --- | --- |
| 1 | 5 | 3 | 60 | 100 / 0 / 0 |
| 2 | 6 | 4 | 90 | 60 / 40 / 0 |
| 3 | 7 | 4 | 120 | 40 / 60 / 0 |
| 4 | 8 | 5 | 160 | 30 / 50 / 20 |
| 5 | 8 | 6 | 200 | 20 / 50 / 30 |

Del Día 6 en adelante se repite la fila del Día 5 y la renta sube `RentIncreaseAfterLastDay` (inicial 40) por día. Los clientes de cada fase se distribuyen parejo en el tiempo de la fase: un spawn cada `PhaseDuration / (Clientes + 1)` segundos, ±`SpawnJitterSec` (inicial 5). El primer cliente del Día 1 aparece a los `FirstClientDelaySec` (inicial 3 s) para cumplir con el hook.

## 8. Ciclo día/noche

Cada día dura 10 minutos: Día de `DayDurationSec` (300 s) y Noche de `NightDurationSec` (300 s). Lo maneja `AC_DayNightCycle` con un timer, y el tiempo nunca se pausa salvo en menús de pausa, resumen, derrota y victoria.

1. **Inicio del día:** `OnPhaseChanged(Day, N)`. Iluminación de día. Spawnean clientes de día según `DT_DayProgression`.
2. **Paso a Noche (300 s):** `OnPhaseChanged(Night, N)`. Transición de iluminación de `NightTransitionSec` (inicial 5 s). Los clientes de día que siguen en el local se quedan y terminan normalmente. Spawnean los arquetipos de noche.
3. **Cierre (600 s):** los clientes que quedan pasan a `Leaving` sin penalidad. Las críticas pendientes se resuelven en el acto.
4. **Renta:** `AC_Economy` cobra la renta (sección 7). Si no alcanza: Game Over.
5. **Resumen del día** (`WBP_DaySummary`): clientes atendidos, clientes perdidos, reacciones (Loved/Liked/Neutral/Hated), plata ganada, renta pagada, plata actual y meta. Botón *"Open the store"* para empezar el Día N+1.

**Iluminación:** un Directional Light y un Sky Light con dos presets (día y noche) guardados en `DA_GameConfig` como color e intensidad. Se interpola entre ambos durante la transición. No hay ciclo solar continuo.

**Reloj en pantalla:** muestra la hora ficticia del local, de 10:00 a 22:00 en el Día y de 22:00 a 04:00 en la Noche, mapeada al tiempo real de cada fase.

## 9. Espacios, cámara e interacción

El jugador no tiene cuerpo ni se desplaza: es una cámara en primera persona fija en dos posiciones, Mostrador y Depósito. Esto viene del board ("NO MOVERSE") y evita programar un personaje en tercera persona.

**Cámara (`BP_CounterCamera`):**

- Mira con el mouse: yaw de ±`LookYawLimit` (inicial 90°, o sea 180° en total) y pitch de ±`LookPitchLimit` (inicial 45°) respecto de la orientación base de cada posición.
- Sensibilidad en `DA_GameConfig`.
- Interacción por click izquierdo con trazado desde el centro de la pantalla. Los objetos interactuables se resaltan con un outline al apuntarlos.

**Cambio de zona:** un botón en el HUD (*"Back room"* / *"Counter"*) reproduce una Level Sequence (`LS_ToBackRoom` / `LS_ToCounter`) que gira la cámara y abre la puerta del depósito. Duración máxima `ZoneTransitionSec` (inicial 1.5 s). Durante la transición se bloquea el input pero el tiempo sigue corriendo. También se puede usar la tecla `Tab`.

**Mostrador — interactuables:**

| Objeto | Acción |
| --- | --- |
| Cliente en el frente | Abre el diálogo del pedido o, si hay película en mano, la entrega. |
| Caja registradora | Muestra la plata actual (contador 3D) y abre el menú del proveedor. |
| Puerta del depósito | Igual que el botón *"Back room"*. |

**Depósito — interactuables:**

| Objeto | Acción |
| --- | --- |
| Estanterías | Abren la lista del inventario filtrada por el género de esa estantería. |
| Reproductor VHS + TV CRT | Abren el panel de armado (3 slots, título, confirmar). Al confirmar se reproduce la animación de meter el VHS en el reproductor y un efecto de estática en la TV. |
| Cajas en el piso | Cosmético: aparecen al comprar catálogos. |

La selección de VHS se hace en la UI (lista por categoría), no agarrando cintas 3D, como propone el board para simplificar.

**Local — ambiente:** los estantes y exhibidores del salón tienen VHS decorativos. Los clientes caminan hasta un `BP_BrowsePoint` libre frente a esos estantes, miran unos segundos y después hacen fila. No afecta la puntuación ni el inventario. Los clientes se mueven con Nav Mesh y `AI MoveTo` simple, sin Behavior Tree.

## 10. UI y game juice

Toda la UI es UMG, en inglés, con estilo VHS/CRT (scanlines y tipografía tipo display de videocasetera, a definir por arte). Los widgets solo leen estado y llaman funciones de los componentes.

| Widget | Contenido | Cuándo se ve |
| --- | --- | --- |
| `WBP_HUD` | Plata, meta (barra de progreso hacia `MovieBudgetGoal`), día y fase, reloj, renta de esta noche, botón de cambio de zona, película en mano. | Siempre en juego |
| `WBP_OrderPanel` | Texto del pedido del cliente del frente y su paciencia como barra. | Mientras haya un cliente en el mostrador, también en el depósito |
| `WBP_Dialogue` | Frase del cliente y botón *"Coming right up"* o *"Here you go"* (entrega, muestra el título). | Al interactuar con el cliente |
| `WBP_Inventory` | Lista de VHS por género (pestañas con el color del género), cada fila con título y estrellas. | Al abrir una estantería o el panel de armado |
| `WBP_FilmCrafting` | Tres slots (Intro, Nudo, Desenlace) que se llenan desde el inventario, campo de título, botón confirmar, botón descartar. | En el reproductor del depósito |
| `WBP_Supplier` | Catálogos disponibles con precio, géneros y botón de compra (deshabilitado sin plata o si ya está comprado). | Desde la caja registradora |
| `WBP_ReviewToast` | Notificación de la página ficticia de reseñas (*"ReelTalk.net"*): título de la película, estrellas de 1 a 5 según la reacción, frase y +/− plata. | Al llegar cada crítica, 4 s |
| `WBP_DaySummary` | Ver sección 8. | Al cerrar cada día |
| `WBP_GameOver` / `WBP_Victory` | Pantalla final con día alcanzado y botón de reiniciar. | Al perder o ganar |
| `WBP_MainMenu` / `WBP_Pause` | Jugar, salir / continuar, reiniciar, salir. | Inicio / tecla `Esc` |

**Tutorial mínimo (hook de 15 a 20 s):** en la primera partida aparecen tres carteles contextuales, uno por vez: al llegar el primer cliente (*"Listen to what they want"*), al entrar al depósito (*"Pick 3 tapes: Intro, Middle, Ending"*) y al tener una película en mano (*"Bring it back to the counter"*). Se desactivan con `bShowTutorial`.

**Game juice (del board):**

- Camera shake leve al recibir una multa y al cobrar la renta.
- Hit stop de `HitStopSec` (inicial 0.08 s) al confirmar una película.
- Contador de plata que sube o baja animado, con color verde o rojo.
- Estática en la TV CRT y efecto de rebobinado al confirmar.
- Al entregar, el cliente no muestra ninguna reacción: el resultado se conoce solo con la crítica, para mantener la intriga.

**Audio:** el sistema elegido es **FMOD** (plugin `FMODStudio` ya integrado y probado en el proyecto). Dejá en cada evento de arriba una llamada a `PlayCue(Name)` de un `BP_AudioManager`. Por ahora `PlayCue` queda vacío; después se mapea cada `Name` a un evento FMOD (variable asignable, sin hardcodear paths).

## 11. Referencia de datos

Todo el contenido y el balance del juego viven en estos assets dentro de `/Game/DirectorsCut/Data/`. Ningún Blueprint de lógica tiene números o textos de contenido propios.

### 11.1 Enums

| Enum | Valores |
| --- | --- |
| `E_Genre` | Horror, Comedy, Romance, Action, SciFi, Drama, Paranormal |
| `E_Polarity` | Likes, Dislikes |
| `E_ClientCategory` | Casual, Enthusiast, Cinephile |
| `E_Phase` | Day, Night |
| `E_SpawnPhase` | Day, Night, Any |
| `E_ClientState` | Entering, Browsing, Queued, AtCounter, Served, Leaving |
| `E_Reaction` | Loved, Liked, Neutral, Hated |
| `E_FilmSlot` | Intro, Middle, Ending |

### 11.2 Structs

| Struct | Campos |
| --- | --- |
| `S_GenreValue` | Genre (`E_Genre`), Value (int 1 a 3) |
| `S_GenrePreference` | Genre (`E_Genre`), Weight (int, negativo = rechazo) |
| `S_CreatedFilm` | Title (Text), IntroRow, MiddleRow, EndingRow (Name), GenreProfile (Map `E_Genre` → float), AverageRating (float) |
| `S_ClientRuntime` | ArchetypeRow (Name), Category, Preferences (Array `S_GenrePreference`), OrderText (Text), PatienceLeft (float), State (`E_ClientState`) |
| `S_Review` | FilmTitle (Text), Reaction (`E_Reaction`), Score (float), MoneyDelta (int), Line (Text) |

### 11.3 Data Tables

| Data Table | Row struct | Campos |
| --- | --- | --- |
| `DT_Genres` | `S_GenreData` | Genre, DisplayName (Text), Color (Linear Color), ShelfLabel (Text) |
| `DT_VHS` | `S_VHSData` | Title (Text), PrimaryGenre, GenreValues (Array `S_GenreValue`), Rating (int 1 a 5), CatalogId (Name), MeshOverride (Static Mesh, opcional) |
| `DT_Catalogs` | `S_CatalogData` | DisplayName (Text), Description (Text), Price (int), VHSRows (Array Name), UnlockDay (int), bOwnedAtStart (bool) |
| `DT_ClientCategories` | `S_ClientCategoryData` | Category, NumAttributes (int), WeightMin (int), WeightMax (int), PositiveChance (float 0 a 1), PayMultiplier (float) |
| `DT_ClientArchetypes` | `S_ClientArchetypeData` | DebugName (Text), Mesh (Skeletal Mesh), SpawnPhase (`E_SpawnPhase`), SpawnWeight (float), AllowedGenres (Array `E_Genre`), FixedPreferences (Array `S_GenrePreference`, vacío = generar), bTutorialOnly (bool) |
| `DT_HintLines` | `S_HintLineData` | Genre, Polarity (`E_Polarity`), Text (Text) |
| `DT_Greetings` | `S_LineData` | Text, SpawnPhase (para separar saludos de día y de noche) |
| `DT_FarewellLines` | `S_LineData` | Text, SpawnPhase |
| `DT_ReviewLines` | `S_ReviewLineData` | Reaction (`E_Reaction`), Text (Text) |
| `DT_DayProgression` | `S_DayData` | DayIndex (int), DayClients (int), NightClients (int), RentAmount (int), CasualWeight, EnthusiastWeight, CinephileWeight (float) |

### 11.4 `DA_GameConfig` (instancia de `PDA_GameConfig`)

| Grupo | Variable | Inicial |
| --- | --- | --- |
| Tiempo | DayDurationSec / NightDurationSec | 300 / 300 |
| Tiempo | NightTransitionSec | 5 |
| Tiempo | FirstClientDelaySec / SpawnJitterSec | 3 / 5 |
| Clientes | ClientPatienceBaseSec / ClientPatienceMaxSec | 60 / 90 |
| Clientes | MaxQueueSize | 3 |
| Clientes | BrowseTimeMinSec / BrowseTimeMaxSec | 3 / 6 |
| Clientes | LeavePenalty | 0 |
| Criterio | CriterioPerLevel / CriterioPatienceBonusSec | 5 / 10 |
| Armado | SlotMultipliers (Map `E_FilmSlot` → float) | 1.0 / 1.0 / 1.0 |
| Armado | MaxFilmTitleLength / MaxHeldFilms | 30 / 1 |
| Armado | bShowSecondaryGenres | false |
| Puntaje | AvoidBonus / RatingStep | 3 / 0.1 |
| Puntaje | LovedThreshold / LikedThreshold / NeutralThreshold | 8 / 3 / 0 |
| Economía | StartMoney / MovieBudgetGoal | 50 / 400 |
| Economía | BaseRentalPrice / MoneyPerPoint / ComplaintFine | 10 / 2 / 15 |
| Economía | RentIncreaseAfterLastDay | 40 |
| Economía | ReviewDelaySec | 15 |
| Cámara | LookYawLimit / LookPitchLimit / MouseSensitivity | 90 / 45 / 1.0 |
| Cámara | ZoneTransitionSec | 1.5 |
| Juice | HitStopSec | 0.08 |
| UI | bShowTutorial | true |
| Luz | DayLightColor, DayLightIntensity, NightLightColor, NightLightIntensity | A definir por arte |

## 12. Orden de implementación, alcance y preguntas abiertas

Cada paso termina con algo jugable en PIE. No empieces el siguiente sin que el anterior compile y funcione.

1. **Datos:** enums, structs, todas las Data Tables vacías con su struct, `PDA_GameConfig` y `DA_GameConfig` con los valores iniciales. Cargar `DT_Genres`, el catálogo inicial en `DT_VHS`, `DT_Catalogs`, `DT_ClientCategories` y `DT_DayProgression`.
2. **Núcleo:** `BP_DC_GameMode` con sus componentes vacíos y los Event Dispatchers. `BP_CounterCamera` y `BP_DC_PlayerController` con la mirada limitada en el mostrador.
3. **Armado sin clientes:** `AC_Inventory`, `AC_FilmCrafting`, `WBP_Inventory` y `WBP_FilmCrafting`. Se puede armar y confirmar una película desde el depósito con su cambio de zona.
4. **Puntuación:** `BPFL_Scoring` completo. Probar los tres ejemplos de la sección 6.3 con un Blueprint de test y verificar que den exactamente esos resultados.
5. **Un cliente:** `BP_Client` con sus estados, `BP_ClientQueue`, diálogo de pistas y paciencia. Spawn manual del `TutorialClient`. Entrega, pago y crítica.
6. **Economía:** `AC_Economy`, `WBP_HUD`, `WBP_ReviewToast`, victoria y derrota.
7. **Día y noche:** `AC_DayNightCycle`, `AC_ClientDirector` con spawns automáticos, fila llena, clientes de noche, cobro de renta y `WBP_DaySummary`.
8. **Proveedor:** `WBP_Supplier` y compra de catálogos.
9. **Pulido:** tutorial, game juice, menú principal y pausa, puntos de `PlayCue`, debug con `F1`.

**Fuera de alcance (no implementar):**

- Personaje del jugador con cuerpo o movimiento. El modelado de un "Personaje Principal" del board queda solo como arte opcional, por ejemplo unas manos.
- IA conversacional (Convai u otras), voces generadas y árboles de diálogo.
- Guardado entre sesiones: cada partida empieza de cero.
- Destrucción de VHS como castigo (aparece en el board, pero el equipo definió que las cintas no se pierden).
- Que el título de la película afecte la puntuación.
- Multijugador, localización, opciones gráficas avanzadas.

**Preguntas abiertas para el equipo** (Claude: no las resuelvas, dejá `TODO(DESIGN):`):

- [x] ~~¿Los 90 s son tope o fijos?~~ **Resuelto:** 60 s base, 90 s es el tope tras mejorar con Criterio.
- [x] ~~Duración de una partida ganadora.~~ **Resuelto:** apuntamos a ~20 min (Día 2, máximo Día 3). Estimación: 18 clientes entre Día 1 y 2 a ~15–20 de ingreso promedio rinden ~300 más los 50 iniciales, así que la meta bajó de 800 a 400. Verificar en playtest y ajustar `MovieBudgetGoal`.
- [ ] Títulos y valores de los VHS de los cinco catálogos comprables.
- [ ] Frases finales de `DT_HintLines`, `DT_Greetings`, `DT_FarewellLines` y `DT_ReviewLines` (mínimo 3 por combinación).
- [x] ~~MetaSounds o FMOD.~~ **Resuelto:** FMOD.
- [ ] Nombre del juego para la entrega (el board lo tiene vacío). En este documento se usa *Director's Cut* como nombre de trabajo.
