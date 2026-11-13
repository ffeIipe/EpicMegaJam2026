# Director's Cut: documento de diseño de sistemas

Nov 12, 2026 · @Felipe · **Rediseño v2 (cintas con skills): Nov 13, 2026**

> **Rediseño v2 (13 nov).** El sistema de combinación cambió. Ya no se compara género contra género: cada VHS tiene 5 *skills* numéricas, cada pedido tiene un objetivo oculto en esas mismas skills y la reacción sale de la distancia entre la suma de las cintas y el pedido (referencia: ingredientes de Schedule I). Las cintas se **consumen** (son ingredientes), hay 3 slots que suben a 5, se empieza con 3 géneros y el resto se desbloquea desde una computadora. Las secciones 3 a 7, 11 y 12 están reescritas; las demás se ajustaron donde hacía falta. **El código existente en `Source/EpicMegaJam2026/` implementa el sistema anterior**: leé la sección 12.1 antes de tocar nada.

## 0. Instrucciones para Claude (leer primero)

Este documento es la fuente de verdad del juego para la UE MegaJam 2026 (tema: **Director's Cut**, entrega el domingo). Implementá exactamente lo que dice, en el orden de la sección 12, sin agregar sistemas, mecánicas ni contenido que no esté escrito acá.

1. **No inventes.** Si algo no está definido, no lo resuelvas por tu cuenta: dejá un `TODO(DESIGN):` en el Blueprint y avisá en tu respuesta. Las preguntas abiertas están en la sección 12.
2. **Ningún número va hardcodeado.** Todo valor numérico o de contenido sale de `DA_GameConfig` o de una Data Table (sección 11). Los valores de este documento son iniciales y se van a balancear.
3. **Blueprints y C++ habilitados**, Unreal Engine 5.8.4, proyecto Blank. Podés usar C++ cuando convenga (lógica compleja, rendimiento, clases base, structs y enums), pero todo parámetro que pueda necesitar ajuste tiene que quedar modificable desde el editor sin recompilar: exponelo con `UPROPERTY(EditAnywhere, BlueprintReadWrite)` o leélo de `DA_GameConfig` o de una Data Table. Las funciones que use la UI o los Blueprints van con `UFUNCTION(BlueprintCallable)`. No uses plugins de terceros salvo que el equipo lo pida (única excepción ya aprobada: FMOD).
4. **Respetá los nombres** de clases, structs, enums y carpetas de la sección 2. No renombres ni reorganices assets existentes, salvo los cambios que pide la migración de la sección 12.1.
5. **Un sistema por vez.** Terminá cada paso de la sección 12, dejalo compilando y testeable en PIE, y recién ahí pasá al siguiente.
6. **Texto del juego en inglés** (UI, diálogos, nombres de VHS). Comentarios y nombres de variables en inglés.
7. **Arte y audio no son tu tarea.** Usá placeholders (cubos, colores por género, textos) y dejá las referencias de mesh y sonido como variables asignables en las Data Tables.
8. El **objetivo de skills de cada pedido es invisible para el jugador**. Solo existe en datos y lógica. El jugador lo deduce por el diálogo. Las skills de cada VHS, en cambio, sí se muestran.

## 1. Visión y loop principal

El jugador es un cineasta frustrado que atiende un videoclub. Los clientes piden películas que no existen y él las arma mezclando VHS en el depósito (3 slots al empezar, hasta 5): su propio *director's cut*. Si el cliente queda conforme, paga y deja propina. Si no, llega una queja con multa. Es un juego **incremental**: se pierde si no se puede pagar la renta del local y se gana al juntar el presupuesto para filmar una película propia.

**Pilares (no negociables):**

- Hook en 15 a 20 segundos: el primer cliente aparece apenas empieza el Día 1 con un pedido de una sola skill.
- Deducción, no adivinanza: el diálogo siempre da pistas suficientes sobre las skills ocultas del pedido.
- Presión de tiempo: cada cliente se va a los 90 segundos y se puede formar fila.
- Estética low poly, texturas hand-painted de 1K, VHS con un color por género.

**Loop de un cliente:**

1. **Llegada:** el cliente entra, recorre un estante (cosmético) y se suma a la fila del mostrador.
2. **Pedido:** al llegar al frente, dice su pedido con frases que dejan entrever las skills que busca.
3. **Elección:** el jugador va al depósito y llena sus slots con cintas del inventario (puede dejar slots vacíos o repetir una cinta).
4. **Creación:** el jugador le pone título y aprieta *"Combine"*: se gastan las cintas usadas.
5. **Entrega:** vuelve al mostrador, entrega la película y cobra el precio base del alquiler.
6. **Crítica online:** unos segundos después llega la reseña: propina si le encantó o le gustó, nada si fue neutral, multa si la odió; si no fue perfecto, la reseña dice en qué falló.
7. **Reinversión:** con la plata se compran cintas en el proveedor y se desbloquean géneros, slots y estrellas en la computadora.

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
| `AC_ClientDirector` | Spawneo de clientes según el día, fila, paciencia, generación del pedido. |
| `AC_Inventory` | Cantidad de copias de cada cinta, compra de cintas. |
| `AC_Unlocks` | Géneros, slots y nivel de estrellas desbloqueados, compra de desbloqueos. |
| `AC_FilmCrafting` | Slots de combinación (3 a 5), título, perfil de skills, consumo de cintas, película en mano. |
| `BPFL_Scoring` | Funciones puras de puntuación (sección 6). Sin estado. |
| `BP_DC_PlayerController` | Input (WASD, mouse), interacción, abrir menús. |
| `BP_CounterCamera` | Character en primera persona: movimiento WASD y cámara con el mouse. |
| `BP_Client` | NPC: movimiento, estado, paciencia visible (opcional), diálogo. |
| `BP_ClientQueue` | Puntos de fila ordenados frente al mostrador. |
| `BP_BrowsePoint` | Punto de estante donde un cliente se queda mirando antes de hacer fila. |
| `BP_VHSTape` | VHS 3D, toma su color de `DT_Genres`. |
| `BP_Computer` | Computadora del mostrador: abre `WBP_Computer` (desbloqueos). Placeholder hasta que arte entregue el modelo. |
| `BP_HUD` + widgets | Ver sección 10. |

**Event Dispatchers principales:** `OnMoneyChanged(NewMoney, Delta)`, `OnPhaseChanged(Phase, DayIndex)`, `OnPhaseTimeTick(SecondsLeft)`, `OnClientReachedCounter(Client)`, `OnClientLeft(Client, bServed)`, `OnFilmCreated(Film)`, `OnReviewReceived(Review)`, `OnRentCharged(Amount)`, `OnGameOver()`, `OnVictory()`, `OnInventoryChanged()`, `OnUnlockPurchased(UnlockId)`.

## 3. Skills y géneros

Cada VHS aporta puntos en 5 **skills**. El pedido de cada cliente es un objetivo oculto en esas mismas 5 skills y el jugador mezcla cintas para acercar la suma al objetivo. Los géneros no se comparan contra nada: solo definen el color del VHS, la estantería y el sesgo con el que el generador (sección 4.2) reparte las skills de cada cinta.

**Enum `E_Skill`** (5 valores):

| `E_Skill` | Nombre en juego | Nombre de diseño |
| --- | --- | --- |
| `Funny` | Funny | Gracioso |
| `Scary` | Scary | Miedoso |
| `Brave` | Brave | Valiente |
| `Thoughtful` | Thoughtful | Reflexivo |
| `Romantic` | Romantic | Romántico (quinta skill propuesta por Claude, a confirmar) |

**`S_SkillVector`:** un entero por skill (`Funny`, `Scary`, `Brave`, `Thoughtful`, `Romantic`) con helpers `Get(E_Skill)` y `Set(E_Skill, int)`. Se usa para las skills de un VHS, el perfil de una película y el objetivo de un pedido. Rangos: VHS de 0 a `TapeSkillMax` (inicial 3); perfil de película y objetivo de pedido de 0 a `SkillCap` (inicial 10).

**Enum `E_Genre`** (10 valores; agregar uno nuevo solo requiere una fila en `DT_Genres` y generar sus cintas). Sesgo = peso con el que el generador reparte puntos, en el orden Funny / Scary / Brave / Thoughtful / Romantic:

| `E_Genre` | Nombre en juego | Color del VHS | Sesgo (F/S/B/T/R) | Disponibilidad |
| --- | --- | --- | --- | --- |
| `Horror` | Horror | Negro | 0 / 3 / 1 / 1 / 0 | **Inicial** |
| `Comedy` | Comedy | Blanco | 3 / 0 / 1 / 0 / 1 | **Inicial** |
| `Action` | Action | Rojo | 1 / 1 / 3 / 0 / 0 | **Inicial** |
| `Romance` | Romance | Rosa | 1 / 0 / 0 / 1 / 3 | Desbloqueable |
| `Drama` | Drama | Violeta | 0 / 0 / 1 / 3 / 2 | Desbloqueable |
| `SciFi` | Sci-Fi | Azul | 0 / 1 / 2 / 2 / 0 | Desbloqueable |
| `Adventure` | Adventure | Verde | 1 / 0 / 3 / 1 / 1 | Desbloqueable |
| `Thriller` | Thriller | Amarillo (propuesto) | 0 / 2 / 1 / 2 / 0 | Desbloqueable |
| `Experimental` | Experimental Cinema | Cian (propuesto) | 1 / 1 / 1 / 1 / 1 | **Premium** (caro, con requisito, sección 7.1) |
| `BMovie` | B-Movie | Naranja (propuesto) | 2 / 2 / 2 / 0 / 0 | **Premium** |

- Los colores de Romance, Action, Adventure, Sci-Fi, Horror, Comedy y Drama vienen del chat del equipo (Gabriel). Los demás son propuestos y viven en `DT_Genres`, así que arte los cambia sin tocar lógica. **Esto reemplaza los colores anteriores del doc** (Horror rojo oscuro, Comedy amarillo, Action naranja) y **elimina `Paranormal`** (su verde pasa a Adventure).
- Los sesgos son iniciales y viven en `DT_Genres.SkillBias`.
- **Por qué arrancar con Horror, Comedy y Action:** cada uno se corresponde casi 1 a 1 con una skill (Scary, Funny, Brave), tienen colores muy distintos (negro, blanco, rojo) y los primeros pedidos scripteados solo necesitan Horror y Comedy (sección 5.4), así que el jugador arranca con dos géneros efectivos y un tercero de reserva.
- Los nombres de skill (Funny, Scary, etc.) **sí se muestran** en cada VHS (sección 4.1), pero el **objetivo del pedido nunca se muestra**: solo existe en datos y lógica, el jugador lo deduce por el diálogo.
- En builds de desarrollo, una tecla de debug (`F1`) muestra sobre cada cliente su objetivo de skills. Se desactiva en el build de entrega.

## 4. VHS y combinación

### 4.1 El VHS

Una cinta es un **ingrediente consumible**. Cada VHS (`S_VHSData`, fila de `DT_VHS`) tiene:

- `Title` (Text): título ficticio en inglés.
- `PrimaryGenre` (`E_Genre`): define el color del VHS y su estantería.
- `Skills` (`S_SkillVector`): cada skill de 0 a `TapeSkillMax` (3).
- `Rating` (entero de 1 a 5): **las estrellas**. Fijan el presupuesto de puntos de la cinta: la suma de sus skills es `StarPoints[Rating]` (inicial 3 / 4 / 5 / 6 / 7 para 1★ a 5★).
- `MeshOverride`, `Cover`, `Synopsis`: opcionales, como hasta ahora.

**Qué ve el jugador de cada VHS** (en tienda, inventario y panel de armado): título, color, género, estrellas y **sus 5 skills con número** (ícono de skill y valor; las skills en 0 se atenúan). Se elimina `bShowSecondaryGenres`.

**Contenido:** 10 géneros × 50 cintas = 500 filas, `TapesPerStar` (inicial 10) por género y por estrella. Las skills de cada cinta son únicas dentro de su género. No hay `CatalogId`: se compran de a una (sección 7.1).

### 4.2 Generación por script

Las 500 filas **no se cargan a mano**: las genera un script determinista, con una semilla por género.

- Ubicación: `Tools/VHSGenerator/` (Python 3, sin dependencias externas). Entrada: `DT_Genres` exportada o un `genres.json` equivalente con `Genre`, `SkillBias`, `Seed`. Salida: JSON o CSV importable a `DT_VHS`, más un reporte de validación.
- Misma semilla ⇒ mismo resultado, siempre. Cambiar la semilla de un género regenera solo ese género.
- Algoritmo por cinta de estrella `s`: repartir `StarPoints[s]` puntos de uno en uno entre las 5 skills, con probabilidad proporcional a `SkillBias + GeneratorBiasFloor` (inicial 0.25, para que cualquier skill pueda aparecer), sin pasar de `TapeSkillMax` por skill. Si el género tiene una skill dominante única (sesgo máximo único), la cinta debe tener al menos 1 punto en ella. Reintentar si el vector ya existe en el género.
- Las cintas de estrellas distintas nunca chocan entre sí porque su suma de puntos es distinta.
- **Cinta pura:** el generador garantiza que cada género con skill dominante tenga, en 1★, una cinta con todos sus puntos en esa skill (por ejemplo `Scary 3`). Se usan para el inventario inicial y los pedidos scripteados.
- Nombres de fila: `VHS_<Genre>_<Star>_<NN>`, por ejemplo `VHS_Horror_1_01`. **La cinta pura es siempre la `_01` de 1★.**
- Títulos: ficticios, en inglés, armados con listas de palabras por género (`title_words.json` junto al script), sin repetir entre las 500 cintas. Escribir las listas es contenido del equipo; hasta entonces, títulos placeholder `"<Genre> Tape 1★ 01"`.
- Validaciones que el script debe verificar y reportar: 50 cintas por género, 10 por estrella, suma de puntos correcta, ninguna skill por encima de `TapeSkillMax`, vectores únicos por género, títulos únicos, existencia de la cinta pura.

### 4.3 Inventario

`AC_Inventory` guarda **cantidades por cinta** (`TMap<Name, int>`: fila de `DT_VHS` → copias). El inventario inicial sale de `DA_GameConfig.StartingTapes`: 2 copias de la cinta pura de Horror, 2 de la de Comedy y 1 de la de Action (las dos primeras alcanzan para los tres pedidos scripteados).

Las cintas se compran de a una copia en el proveedor y se desbloquean por género y estrellas (sección 7.1). Al armar una película se gastan (sección 4.4).

### 4.4 Armado (`AC_FilmCrafting`)

El jugador tiene `UnlockedSlots` slots de combinación (inicial `StartUnlockedSlots` = 3, máximo `MaxSlots` = 5). Se suben desde la computadora (sección 7.1). Los slots no tienen nombre ni orden narrativo: son "Slot 1", "Slot 2", etc.

**Flujo en el panel de armado (reproductor del depósito):** el jugador elige un slot, elige una cinta del inventario para ese slot, y repite con los demás. Aprieta *"Combine"*. Puede volver a tocar un slot lleno para cambiarlo o vaciarlo antes de combinar.

**Reglas:**

1. Se permite **dejar slots vacíos**. Hace falta al menos 1 cinta para combinar.
2. Se permite **repetir la misma cinta en varios slots** mientras alcance el stock: usar la misma cinta en 2 slots gasta 2 copias.
3. Se permite repetir género libremente.
4. La **suma de skills se actualiza en vivo** mientras se llenan los slots, con el perfil resultante a la vista (no se muestra el objetivo del pedido).
5. Con al menos 1 slot lleno se habilita el campo de título (máximo `MaxFilmTitleLength`, inicial 30). Si queda vacío, se usa `"Untitled Cut #N"`. El título **no afecta la puntuación**.
6. Al combinar se **gastan las copias** (1 por slot usado) y se crea un `S_CreatedFilm` = {`Title`, `SlotRows`, `SkillProfile`} que pasa a ser la **película en mano**. La película resultante **tiene aspecto visual propio**: el VHS toma una mezcla de los colores de género de sus cintas (placeholder: franjas de color por cinta).
7. Solo se puede tener `MaxHeldFilms` películas en mano (inicial 1). Con una en mano, *"Combine"* queda deshabilitado hasta entregarla o descartarla.
8. **Descartar la película en mano devuelve las cintas al inventario** y no tiene costo. No revela nada del pedido, así que no hay forma de abusar.
9. El reproductor de VHS está disponible **desde el segundo 0** del Día 1. Tiene que ser notorio: es el tema de la jam.

**Perfil de la película:**

```latex
Profile[s] = \min\Bigl(SkillCap,\ \sum_{slot} Skills_{slot}[s]\Bigr)
```

Un slot vacío suma 0. Con los valores iniciales, 4 cintas con `Funny 3` dan `Funny 10` (el exceso se corta en `SkillCap`).

## 5. Clientes (NPCs) y pedidos

Cada cliente trae un **pedido**: un objetivo oculto en las 5 skills (`Target`, un `S_SkillVector`). El jugador lo deduce por el diálogo y arma una película cuyo perfil se le acerque. Los clientes se diferencian por **nivel de estrellas** (1★ a 5★): cuanto más exquisito, más grande y más complejo es el pedido y mejor paga. El sistema es el de Schedule I: con cintas de pocas estrellas solo se puede satisfacer a clientes de pocas estrellas, y al desbloquear cintas de más estrellas aparecen clientes más exclusivos.

### 5.1 Niveles de cliente (`DT_ClientTiers`)

| Nivel (`Tier`) | Cintas de la receta | Estrellas de esas cintas | Skills con pista (máx.) | Multiplicador de pago | Peso de aparición |
| --- | --- | --- | --- | --- | --- |
| 1★ (Casual) | 1 a 2 | 1 | 2 | 1.0 | 100 |
| 2★ (Regular) | 2 a 3 | 1 a 2 | 3 | 1.5 | 80 |
| 3★ (Enthusiast) | 3 | 2 a 3 | 3 | 2.0 | 60 |
| 4★ (Cinephile) | 3 a 4 | 3 a 4 | 4 | 3.0 | 40 |
| 5★ (Auteur) | 4 a 5 | 4 a 5 | 5 | 4.0 | 20 |

Todos los valores son iniciales. **Un nivel solo aparece si `Tier ≤ UnlockedStarTier`** (la licencia de estrellas del jugador, sección 7.1). Reemplaza a las categorías Casual / Enthusiast / Cinephile del diseño anterior.

### 5.2 Generación del pedido

Al spawnear, `AC_ClientDirector` arma el cliente así:

1. Elige un arquetipo visual de `DT_ClientArchetypes` según `SpawnWeight` y la fase.
2. **Pedidos scripteados:** los primeros clientes de la partida usan las filas de `DT_ScriptedOrders` en orden (sección 5.4). Si hay una fila para este cliente, se usa su `Target` y su arquetipo y se saltan los pasos siguientes.
3. Elige un nivel según `SpawnWeight` de `DT_ClientTiers`, entre los niveles con `Tier ≤ UnlockedStarTier`.
4. Arma una **receta**: sortea `n` cintas con `n` entre `RecipeTapesMin` y `RecipeTapesMax` del nivel, **sin pasar de `UnlockedSlots`**. Cada cinta sale de las filas de `DT_VHS` con género desbloqueado, estrellas dentro del rango del nivel y `Rating ≤ UnlockedStarTier`. Si el arquetipo tiene `AllowedGenres` no vacío, las cintas salen de esos géneros (si la intersección con los desbloqueados queda vacía, de cualquier género desbloqueado). Se prefieren cintas distintas; se permite repetir si el grupo es chico.
5. `Target` = el perfil de esa receta, con la misma fórmula del perfil de película (suma por skill, tope `SkillCap`).

**Por qué así:** el objetivo siempre se puede alcanzar exactamente con cintas que el proveedor vende, y su magnitud queda ajustada sola al nivel del cliente (mismo mecanismo que escala los pedidos con las estrellas y con los slots). No hay que balancear una escala aparte.

### 5.3 Clientes de noche

Durante la Noche spawnean arquetipos con `Phase` = `Night` o `Any`. Los de noche son los "clientes raros" del board: máscaras, look tipo *Eyes Wide Shut*, con `AllowedGenres` de Horror, Thriller y SciFi. Nada sexual ni explícito, para cumplir las reglas de la jam. Mecánicamente funcionan igual que los de día.

### 5.4 Diálogo de pistas y pedidos scripteados

El pedido se arma concatenando saludo + frases de pista + cierre. Las pistas dicen **cuánto** quiere el cliente de ciertas skills, sin nombrarlas.

**Niveles de pista** (`E_HintLevel`, calculados del valor de la skill en `Target`):

| Nivel | Condición | Ejemplo (Funny) |
| --- | --- | --- |
| `High` | `Target[s] ≥ HintHighMin` (inicial 3) | *"It's been a rough week. I need to laugh."* |
| `Some` | `1 ≤ Target[s] < HintHighMin` | (frase a escribir) |
| `None` | `Target[s] = 0` | *"Nothing goofy. I am not in the mood for jokes."* |

**Qué skills reciben pista:** se ordenan primero las `High` (de mayor a menor), después las `Some` (de mayor a menor) y después las `None` (al azar). Se toman las primeras `MaxHintedSkills` del nivel del cliente (sección 5.1). Las demás no se mencionan: el jugador las infiere con los números visibles de las cintas.

- Cada fila de `DT_HintLines` = {`Skill`, `Level`, `Text`}. Hay al menos 3 frases por combinación skill y nivel (5 × 3 × 3 = 45 frases). Se elige una al azar sin repetir en el mismo día.
- Las frases nunca nombran la skill ni el género.
- **Reutilizar las frases ya escritas.** `DT_HintLines` ya tiene frases por género y polaridad. Se migran así: Horror → `Scary`, Comedy → `Funny`, Romance → `Romantic`, Action → `Brave`, Drama → `Thoughtful`; `Likes` → `High`, `Dislikes` → `None`. Quedan escritas **30 frases** (6 por skill). Las de SciFi y Paranormal no tienen skill: dejarlas en la tabla sin usar, no borrarlas. **Faltan** las 15 frases de nivel `Some` (3 por skill) y completar el mínimo de 3 en las combinaciones que hoy tengan menos: `TODO(DESIGN)`, las escribe el equipo.
- El pedido queda escrito en un panel del HUD mientras el cliente siga en el local, para poder releerlo desde el depósito. El jugador responde con un único botón *"Coming right up"*.

**Pedidos scripteados (`DT_ScriptedOrders`).** Los primeros clientes enseñan la mecánica y solo usan Horror y Comedy (aunque Action también esté desbloqueado). Cada fila fija `Target`, las pistas exactas y el arquetipo:

| # | Target | Pistas | Qué enseña |
| --- | --- | --- | --- |
| 1 | `Funny 3` (el resto en 0) | Funny `High` + Scary `None` ("nada de monstruos") | Una cinta, y que lo que no pide también cuenta |
| 2 | `Funny 3, Scary 3` | Funny `High` + Scary `High` | **Combinar dos cintas** (el Director's Cut), dentro del primer minuto |
| 3 | `Scary 3` (el resto en 0) | Scary `High` + Funny `None` | Repetir la mecánica con el otro género |

El inventario inicial (sección 4.3) alcanza para los tres. Desde el cliente 4 el pedido se genera con la receta de la sección 5.2.

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

La reacción del cliente sale de la **distancia** entre el perfil de la película y su `Target`. Todo se calcula en `BPFL_Scoring` con funciones puras, en el momento de la entrega.

### 6.1 Distancia y precisión

```latex
Miss = \sum_{s \in Skills} \bigl| Profile[s] - Target[s] \bigr|
```

```latex
ErrorRatio = \frac{Miss}{\max(1,\ \sum_{s} Target[s])} \qquad Accuracy = \operatorname{clamp}(1 - ErrorRatio,\ 0,\ 1)
```

- Se normaliza por el total del objetivo para que los umbrales sirvan igual en pedidos chicos (1★) y grandes (5★).
- Pasarse cuenta igual que quedarse corto: `|Profile − Target|`.
- Las estrellas de las cintas ya no entran en la fórmula: están en el presupuesto de puntos de cada cinta (sección 4.1).

### 6.2 Reacción y plata

| Reacción (`E_Reaction`) | Condición (inicial) | Al entregar | Crítica online |
| --- | --- | --- | --- |
| `Loved` | `ErrorRatio ≤ LovedMaxError` (0.10) | Precio = `BaseRentalPrice` × multiplicador de nivel | Propina = `Accuracy` × `TipAtPerfect` × multiplicador de nivel, redondeada. +1 Criterio. |
| `Liked` | `≤ LikedMaxError` (0.35) | Precio | Propina con la misma fórmula. |
| `Neutral` | `≤ NeutralMaxError` (0.65) | Precio | Sin propina. |
| `Hated` | `> NeutralMaxError` | Precio | Multa = `ComplaintFine` × multiplicador de nivel. |

`Loved`, `Liked` y `Hated` son los "perfecto, bien, mal" del diseño; `Neutral` se mantiene como zona intermedia (no pierde ni gana propina). Los valores son iniciales y se balancean con playtest.

- La crítica llega `ReviewDelaySec` después de que el cliente se va (inicial 15 s) como notificación de la página ficticia de reseñas (sección 10), con el título de la película, una frase de `DT_ReviewLines` según la reacción y, si corresponde, la línea de feedback (sección 6.3).
- La plata puede quedar negativa por multas. Solo se pierde en el cobro de la renta (sección 7).

### 6.3 Feedback: en qué falló

La crítica enseña al jugador qué salió mal. Si la reacción **no es `Loved`**, la crítica suma una frase de feedback sobre **la skill con mayor `|Profile − Target|`** (si hay empate, al azar), en la dirección que corresponda:

- `TooLittle` si `Profile[s] < Target[s]`, por ejemplo *"Could have been a lot funnier."*
- `TooMuch` si `Profile[s] > Target[s]`, por ejemplo *"Way too scary for me."*

Las frases salen de `DT_FeedbackLines` = {`Skill`, `Direction`, `Text`}, mínimo 3 por combinación (5 × 2 × 3 = 30 frases): `TODO(DESIGN)`, las escribe el equipo. El feedback aparece **solo en la crítica**: al entregar, el cliente sigue sin mostrar ninguna reacción (intriga).

### 6.4 Criterio

Cada `Loved` suma 1 punto de Criterio. Cada `CriterioPerLevel` puntos (inicial 5) la paciencia de todos los clientes sube `CriterioPatienceBonusSec` (inicial 10 s), sin pasar de `ClientPatienceMaxSec` (90 s). El Criterio no se gasta ni se pierde.

### 6.5 Ejemplos con valores iniciales

Estos casos son los tests de `BPFL_Scoring` (paso 3 de la sección 12.2). Deben dar exactamente estos resultados.

| # | Nivel | Target | Perfil de la película | Miss | ErrorRatio | Reacción | Pago al entregar | Propina / multa |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1★ (×1.0) | Funny 3 | Funny 3 | 0 | 0.00 | `Loved` | 10 | propina 12 |
| 2 | 1★ (×1.0) | Funny 3, Scary 3 | Funny 3, Scary 2 | 1 | 0.167 | `Liked` | 10 | propina 10 (0.833 × 12) |
| 3 | 1★ (×1.0) | Funny 3 | Scary 3 | 6 | 2.00 | `Hated` | 10 | multa 15 |
| 4 | 2★ (×1.5) | Funny 5, Brave 2, Romantic 1 | Funny 5, Brave 1, Romantic 1 | 1 | 0.125 | `Liked` | 15 | propina 16 (0.875 × 12 × 1.5 = 15.75) |
| 5 | 1★ (×1.0) | Funny 3 | Funny 3 y Thoughtful 3 (dos cintas) | 3 | 1.00 | `Hated` | 10 | multa 15 |
| 6 | cualquiera | cualquiera | 4 cintas con Funny 3 (perfil, no pedido) | n/a | n/a | n/a | n/a | `Profile[Funny]` = 10 (tope `SkillCap`) |

En el caso 5, la película tiene `Funny 3` y `Thoughtful 3` contra un `Target` de solo `Funny 3`: sobrarse en una skill que el cliente no pidió también castiga.

## 7. Economía, derrota y victoria

Una sola moneda: dólares (`Money`, entero). Entra por alquileres y propinas. Sale por multas, renta, compra de cintas y desbloqueos.

**Derrota:** al terminar cada Noche, `AC_Economy` cobra la renta del día (`RentAmount` de `DT_DayProgression`). Si `Money < RentAmount` en ese momento: `OnGameOver` y pantalla de derrota (*"Eviction notice"*). Si alcanza, se descuenta y sigue el resumen del día.

**Victoria:** apenas `Money ≥ MovieBudgetGoal` (inicial 400), se dispara `OnVictory`: el juego se pausa y aparece la pantalla final (*"Your Director's Cut gets greenlit"*). Se chequea cada vez que cambia la plata.

**Plata inicial:** `StartMoney` = 50.

### 7.1 Cintas, géneros y desbloqueos

Ya no hay catálogos de paquetes (`DT_Catalogs` queda **deprecada**, sección 12.1): las cintas se compran sueltas y todo lo demás se desbloquea desde la computadora.

**Proveedor (caja registradora, `WBP_Supplier`).** Vende cintas **de a una copia** (botones ×1 y ×5). Una cinta está a la venta si **su género está desbloqueado** y **`Rating ≤ UnlockedStarTier`**. Precio = `TapePriceByStar[Rating]` (inicial 4 / 6 / 9 / 14 / 20). Con 500 cintas la lista necesita pestañas por género (con el color) y filtro por estrellas. Al comprar, el stock sube al instante.

**Computadora (`BP_Computer` + `WBP_Computer`).** Es el único lugar donde se desbloquea contenido. El modelo 3D todavía no está listo: usar un cubo placeholder en el mostrador. Cada desbloqueo se compra una sola vez (`DT_Unlocks`). Los precios y requisitos son iniciales, propuestos por Claude y a balancear:

| `UnlockId` | Efecto | Precio | Requisito |
| --- | --- | --- | --- |
| `Genre_Romance` | Desbloquea Romance | 40 | |
| `Genre_Drama` | Desbloquea Drama | 60 | |
| `Genre_SciFi` | Desbloquea Sci-Fi | 80 | |
| `Genre_Adventure` | Desbloquea Adventure | 100 | |
| `Genre_Thriller` | Desbloquea Thriller | 120 | |
| `Slot_4` | `UnlockedSlots` 3 → 4 | 100 | |
| `Slot_5` | `UnlockedSlots` 4 → 5 | 160 | `Slot_4` |
| `Stars_2` | `UnlockedStarTier` 1 → 2 | 60 | |
| `Stars_3` | `UnlockedStarTier` 2 → 3 | 100 | `Stars_2` |
| `Stars_4` | `UnlockedStarTier` 3 → 4 | 160 | `Stars_3` |
| `Stars_5` | `UnlockedStarTier` 4 → 5 | 240 | `Stars_4` |
| `Genre_Experimental` | Desbloquea Experimental Cinema | 300 | Al menos `PremiumMinGenres` (6) géneros desbloqueados |
| `Genre_BMovie` | Desbloquea B-Movie | 300 | Igual que el anterior |

Los dos géneros premium son caros y exigen tener antes la mayoría de los géneros base ("que no sea tan fácil comprarlos").

**Estado inicial:** géneros Horror, Comedy y Action; `UnlockedSlots` = 3; `UnlockedStarTier` = 1.

> **Riesgo de balance:** la victoria se dispara con `Money ≥ MovieBudgetGoal` y la meta actual (400) fue pensada para ~20 minutos sin gastar en desbloqueos. Con esta economía el dinero se gasta en cintas y en la computadora, así que probablemente haya que subir `MovieBudgetGoal` (el valor original del board era 800). Verificar en playtest.

### 7.2 Progresión por día (`DT_DayProgression`)

| Día | Clientes de día | Clientes de noche | Renta |
| --- | --- | --- | --- |
| 1 | 5 | 3 | 60 |
| 2 | 6 | 4 | 90 |
| 3 | 7 | 4 | 120 |
| 4 | 8 | 5 | 160 |
| 5 | 8 | 6 | 200 |

Se **eliminan** las tres columnas de pesos de categoría: el nivel de los clientes ahora depende de `UnlockedStarTier` y de `DT_ClientTiers`, no del día. Del Día 6 en adelante se repite la fila del Día 5 y la renta sube `RentIncreaseAfterLastDay` (inicial 40) por día. Los clientes de cada fase se distribuyen parejo en el tiempo de la fase: un spawn cada `PhaseDuration / (Clientes + 1)` segundos, ±`SpawnJitterSec` (inicial 5). El primer cliente del Día 1 aparece a los `FirstClientDelaySec` (inicial 3 s) para cumplir con el hook.

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

El jugador es un personaje en primera persona (sin modelo visible, solo cámara) que se mueve libremente por el local y el depósito. No hay tercera persona.

**Personaje (`BP_CounterCamera`):**

- Se mueve con WASD (`WalkSpeed`, inicial 300 cm/s), con colisión contra el mobiliario y las paredes. Sin salto ni correr.
- Mira con el mouse sin límite de yaw y con pitch de ±`LookPitchLimit` (inicial 80°). Sensibilidad (`MouseSensitivity`) en `DA_GameConfig`.
- Interacción por click izquierdo con trazado desde el centro de la pantalla, con alcance `InteractRange` (inicial 450 cm: el cliente del mostrador queda a unos 360 cm del punto de inicio). Los objetos interactuables se resaltan con un outline al apuntarlos.

**Cambio de zona:** no hay botón ni transición. El jugador camina por la puerta entre el mostrador y el depósito. El HUD no tiene botón de zona.

**Mostrador — interactuables:**

| Objeto | Acción |
| --- | --- |
| Cliente en el frente | Abre el diálogo del pedido o, si hay película en mano, la entrega. |
| Caja registradora | Muestra la plata actual (contador 3D) y abre el menú del proveedor (compra de cintas). |
| Computadora (`BP_Computer`) | Abre `WBP_Computer`: desbloqueo de géneros, slots y estrellas. |

**Depósito — interactuables:**

| Objeto | Acción |
| --- | --- |
| Estanterías | Muestran el stock de cintas del jugador, filtrado por el género de esa estantería (solo lectura, con cantidades). |
| Reproductor VHS + TV CRT | Abren el panel de armado (slots, suma de skills en vivo, título, *"Combine"*). Al confirmar se reproduce la animación de meter el VHS en el reproductor y un efecto de estática en la TV. |
| Cajas en el piso | Cosmético: aparecen al comprar cintas. |

Las cintas se eligen directo del inventario dentro del panel de armado; no son objetos 3D que se agarran, como propone el board, para simplificar. La hotbar y la toma de cintas desde estanterías quedan deprecadas (sección 12.1).

**Local — ambiente:** los estantes y exhibidores del salón tienen VHS decorativos. Los clientes caminan hasta un `BP_BrowsePoint` libre frente a esos estantes, miran unos segundos y después hacen fila. No afecta la puntuación ni el inventario. Los clientes se mueven con Nav Mesh y `AI MoveTo` simple, sin Behavior Tree.

## 10. UI y game juice

Toda la UI es UMG, en inglés, con estilo VHS/CRT (scanlines y tipografía tipo display de videocasetera, a definir por arte). Los widgets solo leen estado y llaman funciones de los componentes.

| Widget | Contenido | Cuándo se ve |
| --- | --- | --- |
| `WBP_HUD` | Plata, meta (barra de progreso hacia `MovieBudgetGoal`), día y fase, reloj, renta de esta noche, película en mano. | Siempre en juego |
| `WBP_OrderPanel` | Texto del pedido del cliente del frente y su paciencia como barra. | Mientras haya un cliente en el mostrador, también en el depósito |
| `WBP_Dialogue` | Frase del cliente y botón *"Coming right up"* o *"Here you go"* (entrega, muestra el título). | Al interactuar con el cliente |
| `WBP_Inventory` | Lista de VHS por género (pestañas con el color del género), cada fila con título, estrellas, las 5 skills con número y cantidad de copias. | Al abrir una estantería (solo lectura) o al elegir cinta para un slot del panel de armado |
| `WBP_FilmCrafting` | Slots de combinación (3 a 5) que se llenan desde el inventario (se puede dejar vacíos o repetir), suma de skills en vivo, campo de título, botón *"Combine"*, botón descartar. | En el reproductor del depósito |
| `WBP_Supplier` | Cintas a la venta (género desbloqueado y estrellas permitidas) con pestañas por género, filtro por estrellas, precio y botones ×1 / ×5 (deshabilitados sin plata). | Desde la caja registradora |
| `WBP_Computer` | Lista de desbloqueos (géneros, slots, estrellas) con precio, requisitos y botón de compra (sección 7.1). | Desde la computadora |
| `WBP_ReviewToast` | Notificación de la página ficticia de reseñas (*"ReelTalk.net"*): título de la película, estrellas de 1 a 5 según la reacción, frase, línea de feedback (si no fue perfecto) y +/− plata. | Al llegar cada crítica, 4 s |
| `WBP_DaySummary` | Ver sección 8. | Al cerrar cada día |
| `WBP_GameOver` / `WBP_Victory` | Pantalla final con día alcanzado y botón de reiniciar. | Al perder o ganar |
| `WBP_MainMenu` / `WBP_Pause` | Jugar, salir / continuar, reiniciar, salir. | Inicio / tecla `Esc` |

**Tutorial mínimo (hook de 15 a 20 s):** en la primera partida aparecen tres carteles contextuales, uno por vez: al llegar el primer cliente (*"Listen to what they want"*), al entrar al depósito (*"Pick tapes and combine them"*) y al tener una película en mano (*"Bring it back to the counter"*). Se desactivan con `bShowTutorial`.

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
| `E_Genre` | Horror, Comedy, Action, Romance, Drama, SciFi, Adventure, Thriller, Experimental, BMovie |
| `E_Skill` | Funny, Scary, Brave, Thoughtful, Romantic |
| `E_HintLevel` | None, Some, High |
| `E_FeedbackDirection` | TooLittle, TooMuch |
| `E_UnlockType` | Genre, Slot, StarTier |
| `E_Phase` | Day, Night |
| `E_SpawnPhase` | Day, Night, Any |
| `E_ClientState` | Entering, Browsing, Queued, AtCounter, Served, Leaving |
| `E_Reaction` | Loved, Liked, Neutral, Hated |
| `E_InteractType` | CashRegister, BackRoomDoor, Shelf, VHSPlayer, **Computer** (nuevo) |

**Se eliminan:** `E_Polarity`, `E_ClientCategory` (reemplazada por el entero `Tier` de 1 a 5) y `E_FilmSlot` (los slots son índices).

### 11.2 Structs

| Struct | Campos |
| --- | --- |
| `S_SkillVector` | Funny, Scary, Brave, Thoughtful, Romantic (int, cada uno). Helpers `Get(E_Skill)` y `Set`. |
| `S_HintedSkill` | Skill (`E_Skill`), Level (`E_HintLevel`) |
| `S_CreatedFilm` | Title (Text), SlotRows (Array Name, tamaño = `UnlockedSlots`, `NAME_None` = slot vacío), SkillProfile (`S_SkillVector`) |
| `S_ClientRuntime` | ArchetypeRow (Name), Tier (int), Target (`S_SkillVector`, oculto), HintedSkills (Array `S_HintedSkill`), OrderText (Text), PatienceLeft (float), State (`E_ClientState`) |
| `S_Review` | FilmTitle (Text), Reaction (`E_Reaction`), ErrorRatio (float), Accuracy (float), MoneyDelta (int), Line (Text), FeedbackLine (Text, vacío si `Loved`) |
| `S_DaySummary` | Sin cambios |

**Se eliminan:** `S_GenreValue` y `S_GenrePreference`.

### 11.3 Data Tables

| Data Table | Row struct | Campos |
| --- | --- | --- |
| `DT_Genres` | `S_GenreData` | Genre, DisplayName (Text), Color (Linear Color), ShelfLabel (Text), SkillBias (`S_SkillVector`, 0 a 3), Seed (int), bUnlockedAtStart (bool) |
| `DT_VHS` | `S_VHSData` | Title (Text), PrimaryGenre, Skills (`S_SkillVector`), Rating (int 1 a 5 = estrellas), MeshOverride (opcional), Cover (opcional), Synopsis (opcional). **Sin `CatalogId`.** 500 filas generadas por script (sección 4.2). |
| `DT_ClientTiers` | `S_ClientTierData` | Tier (int 1 a 5), DebugName (Text), RecipeTapesMin / RecipeTapesMax (int), RecipeStarMin / RecipeStarMax (int), MaxHintedSkills (int), PayMultiplier (float), SpawnWeight (float) |
| `DT_ClientArchetypes` | `S_ClientArchetypeData` | DebugName (Text), Mesh (Skeletal Mesh), SpawnPhase (`E_SpawnPhase`), SpawnWeight (float), AllowedGenres (Array `E_Genre`, vacío = cualquiera desbloqueado), bTutorialOnly (bool). **Sin `FixedPreferences`.** |
| `DT_ScriptedOrders` | `S_ScriptedOrderData` | Index (int), Target (`S_SkillVector`), HintedSkills (Array `S_HintedSkill`), ArchetypeRow (Name) |
| `DT_HintLines` | `S_HintLineData` | Skill (`E_Skill`), Level (`E_HintLevel`), Text (Text) |
| `DT_Greetings` | `S_LineData` | Text, SpawnPhase |
| `DT_FarewellLines` | `S_LineData` | Text, SpawnPhase |
| `DT_ReviewLines` | `S_ReviewLineData` | Reaction (`E_Reaction`), Text (Text) |
| `DT_FeedbackLines` | `S_FeedbackLineData` | Skill (`E_Skill`), Direction (`E_FeedbackDirection`), Text (Text) |
| `DT_Unlocks` | `S_UnlockData` | UnlockId (Name), Type (`E_UnlockType`), Genre (`E_Genre`, si es de género), DisplayName (Text), Description (Text), Price (int), RequiresUnlocks (Array Name), MinGenresOwned (int, 0 = sin requisito) |
| `DT_DayProgression` | `S_DayData` | DayIndex (int), DayClients (int), NightClients (int), RentAmount (int). **Sin pesos de categoría.** |

**Se eliminan** (deprecadas, ver 12.1): `DT_Catalogs`, `DT_ClientCategories`.

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
| Skills | SkillCap / TapeSkillMax | 10 / 3 |
| Skills | StarPoints (Array de 5) | 3 / 4 / 5 / 6 / 7 |
| Skills | TapesPerStar | 10 |
| Pistas | HintHighMin | 3 |
| Armado | StartUnlockedSlots / MaxSlots | 3 / 5 |
| Armado | MaxFilmTitleLength / MaxHeldFilms | 30 / 1 |
| Progresión | StartUnlockedStarTier | 1 |
| Progresión | PremiumMinGenres | 6 |
| Inventario | StartingTapes (Map Name → int) | Pura Horror ×2, pura Comedy ×2, pura Action ×1 |
| Inventario | TapePriceByStar (Array de 5) | 4 / 6 / 9 / 14 / 20 |
| Puntaje | LovedMaxError / LikedMaxError / NeutralMaxError | 0.10 / 0.35 / 0.65 |
| Economía | StartMoney / MovieBudgetGoal | 50 / 400 (ver riesgo en 7.1) |
| Economía | BaseRentalPrice / TipAtPerfect / ComplaintFine | 10 / 12 / 15 |
| Economía | RentIncreaseAfterLastDay | 40 |
| Economía | ReviewDelaySec | 15 |
| Jugador | WalkSpeed / InteractRange | 300 / 450 |
| Cámara | LookPitchLimit / MouseSensitivity | 80 / 1.0 |
| Juice | HitStopSec | 0.08 |
| UI | bShowTutorial | true |
| Luz | DayLightColor, DayLightIntensity, NightLightColor, NightLightIntensity | A definir por arte |

**Se eliminan:** `SlotMultipliers`, `bShowSecondaryGenres`, `AvoidBonus`, `RatingStep`, `LovedThreshold` / `LikedThreshold` / `NeutralThreshold` (reemplazados por los `*MaxError`), `MoneyPerPoint` (reemplazado por `TipAtPerfect`). `HotbarSize` queda sin uso (ver 12.1).

## 12. Orden de implementación, alcance y preguntas abiertas

### 12.1 Estado del código y migración

La v1 (sistema de géneros contra géneros) está implementada en C++ en `Source/EpicMegaJam2026/` y en los assets de `Content/DirectorsCut/`. **Nadie auditó ese código al escribir esta sección**: el primer paso es leerlo. Archivos que, por su nombre y contenido, hay que migrar:

| Archivo / asset | Qué cambia |
| --- | --- |
| `Data/DCTypes.h` | `EGenre` (10 valores, sin `Paranormal`), nuevos `ESkill`, `EHintLevel`, `EFeedbackDirection`, `EUnlockType`, `FSkillVector`. Quitar `EPolarity`, `EClientCategory`, `EFilmSlot`, `FGenreValue`, `FGenrePreference`. Reescribir `FCreatedFilm`, `FClientRuntime`, `FReview`, `FVHSData`, `FGenreData`, `FClientArchetypeData`, `FHintLineData`, `FReviewLineData`, `FDayData`; agregar los row structs nuevos. Agregar `Computer` a `EInteractType`. |
| `Data/PDA_GameConfig.h` | Variables de la sección 11.4. |
| `Core/BPFL_Scoring.*` y `Core/ScoringTest.cpp` | Reescribir con la sección 6. Los tests de la sección 6.5 reemplazan a los actuales. |
| `Components/DCComponents.*` | `AC_Inventory` con cantidades, `AC_FilmCrafting` con N slots y consumo, `AC_Economy` con precios por estrella, nuevo `AC_Unlocks`. |
| `Components/DCClientDirector.cpp`, `Core/DCClient.*` | Generación del pedido (5.2), pistas (5.4), niveles, pedidos scripteados, feedback en la crítica. |
| `Core/ClientFlowTest.cpp`, `EconomyTest.cpp`, `DayCycleTest.cpp` | Ajustar a los cambios de arriba. |
| `UI/*` y los `WBP_*` | `WBP_Inventory`, `WBP_FilmCrafting`, `WBP_Supplier`, `WBP_Shelf`, `WBP_ReviewToast` y tutorial; nuevo `WBP_Computer`. |
| `Data/DT_*` y `DA_GameConfig` | **Al cambiar un row struct se pierden las filas existentes.** Antes de tocar un struct, exportar la tabla a CSV/JSON; después reimportar con los campos nuevos. |

**Qué queda deprecado (no borrar hasta confirmar con el equipo):**

- `DT_Catalogs`, `DT_ClientCategories` y el flujo de catálogos de `WBP_Supplier`.
- **La hotbar** (`HotbarSize`, teclas `1`–`9` y `Q`) y la toma de cintas desde estanterías. Con cintas consumibles que se eligen directo del inventario en el panel de armado, la hotbar queda sin función. No usarla en el flujo de armado. Las estanterías del depósito pasan a mostrar el stock (solo lectura, con cantidades). Confirmar con el equipo si se borra (pregunta abierta abajo).

### 12.2 Orden de implementación

Cada paso termina con algo compilando y probado en PIE. No empieces el siguiente sin eso. Los pasos 1 a 9 del diseño anterior (núcleo, economía, día y noche, clientes, UI, pulido) ya existen en el código; esta es la migración.

1. **Auditoría:** leer `Source/EpicMegaJam2026/` y los Blueprints, y reportar qué de la sección 12.1 ya está hecho o difiere. No cambiar nada.
2. **Datos:** enums, structs y row structs nuevos (11.1 a 11.3), `PDA_GameConfig` y `DA_GameConfig` (11.4). Cargar `DT_Genres` (con sesgos y semillas), `DT_ClientTiers`, `DT_Unlocks`, `DT_DayProgression` sin pesos y `DT_ScriptedOrders`. Dejar el proyecto compilando.
3. **Puntuación:** `BPFL_Scoring` completo (perfil, distancia, reacción, propina, multa, línea de feedback). Test automatizado con los 6 casos de la sección 6.5.
4. **Generador de cintas:** `Tools/VHSGenerator/`, reporte de validación (4.2) y reimportar las 500 filas en `DT_VHS`. Ajustar `StartingTapes` con las filas `_01` de 1★.
5. **Inventario y armado:** `AC_Inventory` con cantidades, `AC_FilmCrafting` con N slots (vacíos y repetidos permitidos, consumo, descarte con devolución), `WBP_Inventory` y `WBP_FilmCrafting` con suma en vivo y las skills visibles en cada VHS. Se puede armar una película desde el depósito.
6. **Pedidos y clientes:** nivel, receta, pistas, pedidos scripteados, `DT_HintLines` migrada, crítica con feedback. Los 3 pedidos scripteados se pueden completar con el inventario inicial.
7. **Proveedor y computadora:** `WBP_Supplier` por cintas sueltas, `AC_Unlocks`, `BP_Computer` y `WBP_Computer` con la tabla de 7.1. Los slots 4 y 5 y los niveles de estrellas modifican de verdad las reglas de armado y de generación de clientes.
8. **Pulido y limpieza:** tutorial nuevo, `F1` con el objetivo, `PlayCue`, quitar la hotbar y los catálogos (solo si el equipo lo confirmó).

**Fuera de alcance (no implementar):**

- Cuerpo visible del jugador o cámara en tercera persona. El modelado de un "Personaje Principal" del board queda solo como arte opcional, por ejemplo unas manos.
- IA conversacional (Convai u otras), voces generadas y árboles de diálogo.
- Guardado entre sesiones: cada partida empieza de cero.
- Destrucción de VHS como castigo (las cintas se gastan solo al combinar).
- Que el título de la película afecte la puntuación.
- Un modelo 3D final de la computadora (placeholder hasta que arte lo entregue). La compra de cintas por teléfono: es solo la computadora.
- Multijugador, localización, opciones gráficas avanzadas.

### 12.3 Preguntas abiertas

**Contenido del equipo** (Claude: no lo resuelvas, dejá `TODO(DESIGN):`):

- [ ] 15 frases `Some` de `DT_HintLines` y completar el mínimo de 3 por combinación skill y nivel.
- [ ] 30 frases de `DT_FeedbackLines` (5 skills × 2 direcciones × 3).
- [ ] Frases finales de `DT_Greetings`, `DT_FarewellLines` y `DT_ReviewLines` (mínimo 3 por combinación).
- [ ] Listas de palabras para los títulos de las cintas (`title_words.json`).
- [ ] Nombre del juego para la entrega (en este documento se usa *Director's Cut* como nombre de trabajo).

**Decisiones propuestas por Claude el 13 nov, pendientes de confirmar con el equipo:**

- [ ] Quinta skill = `Romantic`.
- [ ] Octavo género base = `Thriller`; colores propuestos (Thriller amarillo, Experimental cian, B-Movie naranja); se elimina `Paranormal`.
- [ ] Arrancar con Horror, Comedy y Action.
- [ ] Las licencias de estrellas (`Stars_2` a `Stars_5`) y los slots 4 y 5 se compran en la computadora, junto con los géneros.
- [ ] Géneros premium con precio 300 y requisito de 6 géneros desbloqueados.
- [ ] Precios, `StarPoints`, niveles de cliente, `SkillCap` 10 y umbrales de reacción: todo es inicial y se ajusta en playtest.
- [ ] Quitar la hotbar y la toma de cintas desde estanterías (12.1).
- [ ] `MovieBudgetGoal`: subirlo o no, dada la nueva economía (7.1).
- [ ] No hay botón "ir al depósito" al aceptar el pedido (el chat original lo mencionaba): se mantiene el recorrido libre en primera persona por la puerta.
- [ ] La película descartada devuelve las cintas (4.4, regla 8).

**Resueltas:**

- [x] ~~¿Los 90 s son tope o fijos?~~ 60 s base, 90 s es el tope tras mejorar con Criterio.
- [x] ~~Duración de una partida ganadora.~~ Apuntamos a ~20 min (Día 2, máximo Día 3). Ver riesgo de balance en 7.1.
- [x] ~~MetaSounds o FMOD.~~ FMOD.
- [x] ~~Cantidad de skills y escala del pedido.~~ 5 skills; escala 0 a `SkillCap` (10); el pedido se arma sumando una receta real, así que escala sola con las estrellas y los slots.
- [x] ~~Cuántas cintas hay y cómo se hacen.~~ 50 por género, generadas por script con semilla por género.
- [x] ~~Cómo se puntúa.~~ Distancia total entre perfil y pedido, con umbrales (referencia Schedule I).
- [x] ~~Director's Cut desde el minuto 0.~~ Disponible desde el segundo 0, con 3 slots.
