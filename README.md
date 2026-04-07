# Vibe Dama (C11) — README

Questo README e' la home della documentazione per **build**, **target CMake**, **test** e **docs**.

Autori: **Sandro Borioni** (sb – Haglard), **ChatGPT**, **Claude**
Versione progetto: **1.0** — Licenza: **MIT**

> *between 2022 and 2026 · with a swift acceleration in March 2026*
> *not a single line of code is crafted by a human*

---

## Descrizione

**Vibe Dama** è un motore per il gioco della **dama italiana** scritto in C11,
basato sulla stessa architettura del progetto [chess-engine](https://github.com/Haglard/chess-engine).

Il design è completamente modulare:

- Il **motore di ricerca** (`engine/`) è generico: negamax con alpha-beta,
  iterative deepening, transposition table, aspiration windows, LMR e quiescence search.
  Non contiene una singola riga specifica per la dama.
- Le **regole di gioco** (`checkers/`) implementano la dama italiana:
  cattura obbligatoria, regola del massimo numero di catture, dame volanti,
  promozione a dama.
- L'**adapter** (`game/checkers_adapter.c`) è la colla che espone le regole
  al motore tramite l'interfaccia `GameAPI`.
- Il **client macOS** (`tools/dama_client_cocoa.m`) è un'applicazione
  Cocoa/AppKit nativa con splash screen, dialogo di configurazione,
  pannello laterale con info motore, undo e sistema di handicap.

---

## Requisiti

- CMake >= 3.16
- GCC o Clang con C11
- macOS 12+ per il client Cocoa (target `dama_client`)
- (Opzionale) Doxygen + Graphviz per la documentazione

---

## Configurazione e build

### Configura (out-of-source)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

Opzioni comuni:
- `-DCMAKE_BUILD_TYPE=RelWithDebInfo | Debug | Release | MinSizeRel`
- `-DLOG_COMPILED_LEVEL=LOG_TRACE|LOG_DEBUG|LOG_INFO|LOG_WARN|LOG_ERROR`
- `-G Ninja` per usare Ninja (più veloce di make)

### Compila tutto
```bash
cmake --build build -j
```

### Compila solo un target
```bash
cmake --build build --target dama_client
cmake --build build --target search_cli
```

### Pulizia
```bash
cmake --build build --target clean
# oppure
rm -rf build
```

---

## Target CMake principali

### Librerie
| Target        | Tipo | Descrizione                                                                 |
|---------------|------|-----------------------------------------------------------------------------|
| `core`        | lib  | Utility generiche: log, vec, hashmap, rng, bitops, ringbuf, pool, time + GameAPI base |
| `engine`      | lib  | Motore di ricerca generico: negamax/alpha-beta, TT, IDA*, LMR, quiescence  |
| `checkers`    | lib  | Modulo dama: board, movegen (regole italiane), eval, zobrist                |
| `game_dama`   | lib  | Adapter GameAPI per la dama italiana                                        |

### Eseguibili
| Target          | Descrizione                                               |
|-----------------|-----------------------------------------------------------|
| `search_cli`    | Ricerca su una singola posizione da riga di comando       |
| `selfplay_cli`  | Self-play motore vs motore                                |
| `dama_client`   | Client grafico nativo macOS (Cocoa/AppKit) — solo Apple   |

---

## Client macOS — Vibe Dama

Il client grafico si trova in `tools/dama_client_cocoa.m`.

### Build e avvio
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target dama_client
./build/dama_client
```

### Funzionalità
- **Splash screen** "Vibe Dama" con scacchiera decorativa (60 s o click per chiudere)
- **Dialogo di configurazione**: modalità HvC / CvC, colore, difficoltà (0.5 s / 2 s / 6 s)
- **Handicap computer**: rimuove N pezzi casuali al computer (0–12), re sempre preservato
- **Posizione personalizzata** (formato `W:Wa1,c1,...:Bb6,d6,...`)
- **Pannello laterale**: turno, barra di valutazione, profondità/nps/TT, ultime mosse, pezzi catturati
- **Tasti**: `⌘N` nuova partita, `⌘Z` annulla mossa, `Esc` deseleziona

---

## Regole dama italiana (implementate)

| Regola                     | Note                                                              |
|----------------------------|-------------------------------------------------------------------|
| Cattura obbligatoria       | Se esiste almeno una cattura, deve essere effettuata             |
| Massimo numero di catture  | Tra più sequenze, obbligatoria quella che cattura più pezzi      |
| Preferenza dama su pedina  | A parità di numero, si preferisce catturare una dama             |
| Dama volante               | La dama si muove e cattura diagonalmente su qualsiasi distanza   |
| Promozione                 | La pedina raggiunge l'ultima traversa e diventa dama; si ferma   |
| Patta per 40 mosse         | 40 mosse senza catture → patta                                    |
| Tripla ripetizione         | Stessa posizione 3 volte → patta                                  |

---

## Struttura directory

```
include/
  core/             header utility generiche
  engine/           interfaccia motore di ricerca
  game/             GameAPI + checkers_adapter.h
  checkers/         board, movegen, eval, zobrist
src/
  core/             implementazione utility
  engine/           search.c, tt.c
  game/             api.c, nim.c (riferimento), checkers_adapter.c
  checkers/         board.c, movegen.c, eval.c, zobrist.c
tools/
  dama_client_cocoa.m   client macOS
  search_cli.c          ricerca CLI
  selfplay_cli.c        self-play CLI
tests/
  unit/             unit test (CTest)
build/              out-of-source (non tracciato)
```

---

## Architettura — GameAPI

Il motore di ricerca comunica con le regole di gioco esclusivamente tramite
la struttura `GameAPI` definita in `include/game/api.h`:

```c
typedef struct GameAPI_s {
    size_t state_size;
    size_t undo_size;
    int      (*side_to_move)     (const game_state_t *st);
    int      (*generate_legal)   (const game_state_t *st, game_move_t *out, int cap);
    uint64_t (*make_move)        (game_state_t *st, game_move_t m, void *undo);
    uint64_t (*unmake_move)      (game_state_t *st, game_move_t m, const void *undo);
    uint64_t (*hash)             (const game_state_t *st);
    int      (*is_terminal)      (const game_state_t *st, game_result_t *out);
    game_score_t (*evaluate)     (const game_state_t *st);
    // ...
} GameAPI;
```

`game_move_t` è un `uint64_t` opaco. Per la dama l'encoding è:

```
bit  0.. 5   casa di partenza  (0..63)
bit  6..11   casa di arrivo    (0..63)
bit    12    flag cattura
bit    13    flag promozione a dama
bit 14..19   casa del pezzo catturato (valido se bit 12 = 1)
```

---

## Test con CTest

```bash
# Esegui tutta la suite
ctest --test-dir build --output-on-failure

# Solo alcuni test
ctest --test-dir build -R board --output-on-failure

# Elenco test disponibili
ctest --test-dir build -N
```

---

## Consigli utili

- **Sanitizer** (debug):
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
  ```

- **Log verboso** a compile-time:
  ```bash
  cmake -S . -B build -DLOG_COMPILED_LEVEL=LOG_DEBUG
  ```

- **Solo librerie** (senza client macOS, per build cross-platform):
  ```bash
  cmake --build build --target core --target checkers --target game_dama --target engine
  ```
