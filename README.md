# Vibe Dama

Motore per la **dama italiana** scritto in **C11** con client grafico nativo **macOS (Cocoa/AppKit)**.

Autori: **Sandro Borioni**, **ChatGPT**, **Claude**
Versione: **1.0** M-bM-^@M-^T Licenza: **MIT**

> *between 2022 and 2026 M-BM-7 with a swift acceleration in March 2026*
> *not a single line of code is crafted by a human*

---

## Descrizione

Il progetto M-CM-( organizzato in tre layer:

- **[engine-core](https://github.com/Haglard/engine-core)** *(submodule in `extern/engine-core`)* M-bM-^@M-^T motore di ricerca negamax generico, transposition table, utility di sistema. Condiviso con chess-engine, forza4 e tris.
- **checkers** M-bM-^@M-^T regole della dama italiana: cattura obbligatoria, massimo numero di catture, dame volanti, promozione.
- **game_dama** M-bM-^@M-^T adapter `GameAPI` che connette le regole al motore di ricerca.

Il motore di ricerca non contiene una sola riga specifica per la dama.

---

## Requisiti

- CMake >= 3.16
- GCC o Clang con C11
- Git (per i submodule)
- macOS 12+ per il client Cocoa (target `dama_client`)

---

## Prima installazione (con submodule)

```bash
git clone https://github.com/Haglard/dama.git
cd dama
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# Target specifico
cmake --build build --target dama_client
cmake --build build --target search_cli

# Pulizia
cmake --build build --target clean
```

---

## Target CMake

### Librerie

| Target | Descrizione |
|---|---|
| `engine_core` | Motore negamax + utility *(da submodule engine-core)* |
| `checkers` | Regole dama: board, movegen (regole italiane), eval, zobrist |
| `game_dama` | Adapter GameAPI per la dama italiana |

### Eseguibili

| Target | Descrizione |
|---|---|
| `search_cli` | Ricerca best-move da riga di comando |
| `selfplay_cli` | Autogioco motore vs motore |
| `dama_client` | Client grafico nativo macOS (Cocoa/AppKit) |

---

## Client macOS

```bash
cmake --build build --target dama_client
./build/dama_client
```

FunzionalitM-CM- :
- Splash screen con scacchiera decorativa
- Dialogo configurazione: modalitM-CM-  HvC / CvC, colore, difficoltM-CM- 
- Handicap computer (rimuove N pezzi casuali, re sempre preservato)
- Posizione personalizzata (formato `W:Wa1,c1,...:Bb6,d6,...`)
- Pannello laterale: turno, barra di valutazione, profonditM-CM- /nps/TT, ultime mosse
- Tasti: `M-bM-^LM-^XN` nuova partita, `M-bM-^LM-^XZ` annulla mossa, `Esc` deseleziona

---

## Regole dama italiana implementate

| Regola | Note |
|---|---|
| Cattura obbligatoria | Se esiste almeno una cattura, deve essere effettuata |
| Massimo numero di catture | Tra piM-CM-9 sequenze, obbligatoria quella che cattura piM-CM-9 pezzi |
| Preferenza dama su pedina | A paritM-CM-  di numero, si preferisce catturare una dama |
| Dama volante | Si muove e cattura diagonalmente su qualsiasi distanza |
| Promozione | La pedina raggiunge l'ultima traversa e diventa dama |
| Patta per 40 mosse | 40 mosse senza catture M-bM-^FM-^R patta |
| Tripla ripetizione | Stessa posizione 3 volte M-bM-^FM-^R patta |

---

## Struttura directory

```
extern/
  engine-core/        submodule: motore e utility condivise
include/
  checkers/           board, movegen, eval, zobrist
  game/               api.h, checkers_adapter.h
src/
  checkers/           implementazione regole dama
  game/               checkers_adapter.c
tools/
  dama_client_cocoa.m client macOS
  search_cli.c
  selfplay_cli.c
build/                out-of-source (non tracciato)
```

---

## Architettura

```
M-bM-^TM-^LM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^P
M-bM-^TM-^B    dama_client      M-bM-^TM-^B  client macOS
M-bM-^TM-^B    search_cli       M-bM-^TM-^B  strumenti CLI
M-bM-^TM-^TM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-,M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^X
         M-bM-^TM-^B
M-bM-^TM-^LM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^VM-<M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^P
M-bM-^TM-^B     game_dama       M-bM-^TM-^B  GameAPI adapter
M-bM-^TM-^TM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-,M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^X
         M-bM-^TM-^B
M-bM-^TM-^LM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^VM-<M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^P  M-bM-^TM-^LM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^P
M-bM-^TM-^B    checkers     M-bM-^TM-^B  M-bM-^TM-^B  engine_core  M-bM-^TM-^B  submodule
M-bM-^TM-^B (regole dama)   M-bM-^TM-^B  M-bM-^TM-^B  (negamax +   M-bM-^TM-^B
M-bM-^TM-^B                 M-bM-^TM-^B  M-bM-^TM-^B   utility)    M-bM-^TM-^B
M-bM-^TM-^TM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^X  M-bM-^TM-^TM-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^@M-bM-^TM-^X
```
