// =============================================
// File: tools/dama_client_cocoa.m
// Project: dama (Checkers/Draughts engine)
// Purpose: Native macOS GUI client (Cocoa/AppKit)
// License: MIT (c) 2025
//
// NOTE: Board rendering and move logic are STUBS —
// they will be implemented once the checkers module
// is complete. The full application framework
// (splash, setup dialog, engine threading, panel,
// undo, handicap) is already wired up.
// =============================================
#import <Cocoa/Cocoa.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <pthread.h>

#include "game/api.h"
#include "engine/search.h"
#include "game/checkers_adapter.h"
#include "checkers/board.h"
#include "checkers/movegen.h"
#include "core/bitops.h"

// ---- Engine thread (identical pattern to chess client) ----
// dama_board_t has no giant stack-allocated arrays so GCD would work,
// but we keep pthread for consistency and forward-compatibility.
typedef struct {
    const GameAPI   *api;
    game_state_t    *st;
    search_params_t  sp;
    search_result_t  out;
    void (^completion)(search_result_t *out);
} engine_job_t;

static void *engine_thread_func(void *arg) {
    engine_job_t *job = (engine_job_t *)arg;
    search_root(job->api, job->st, &job->sp, &job->out);
    void (^cb)(search_result_t *) = job->completion;
    search_result_t *outp = malloc(sizeof(search_result_t));
    *outp = job->out;
    free(job);
    dispatch_async(dispatch_get_main_queue(), ^{
        cb(outp);
        free(outp);
    });
    return NULL;
}

// ---- Layout constants ----
#define SQ_SIZE   62.0
#define BOARD_X   40.0
#define BOARD_Y   46.0
#define PANEL_X  540.0
#define WIN_W    800.0
#define WIN_H    580.0
#define UNDO_MAX 256
#define UNDO_BUF_SZ  64

typedef enum { MODE_HVC = 1, MODE_CVC = 2 } play_mode_t;

typedef struct {
    game_move_t mv;
    char  buf[UNDO_BUF_SZ];
    char  caps_w[32]; int n_w;
    char  caps_b[32]; int n_b;
    int   hist_n;
} undo_entry_t;

// ================================================================
// DamaView — board rendering + input + game logic
// ================================================================
@interface DamaView : NSView {
    const GameAPI   *_api;
    game_state_t    *_st;
    game_state_t    *_eng_st;
    play_mode_t      _mode;
    int              _humanColor;

    undo_entry_t     _undo[UNDO_MAX];
    int              _undoTop;

    char   _hist[20][12];
    int    _histN;

    char   _capsW[32]; int _nW;
    char   _capsB[32]; int _nB;

    int      _selSq;
    uint64_t _legalTargets;
    int      _lastFrom, _lastTo;

    NSString *_message;

    volatile int _stopEngine;
    int          _generation;
    BOOL         _engineBusy;
    int32_t      _scoreWhite;

    BOOL     _sinfoValid;
    int      _sinfoSide;
    int32_t  _sinfoScore;
    int      _sinfoDepth;
    uint64_t _sinfoNodes;
    double   _sinfoNPS;
    double   _sinfoTimeSec;
    double   _sinfoTTRate;
    char     _sinfoPV[80];

    search_params_t _sp;
}
- (void)showSetupDialog:(BOOL)startup;
- (void)promptNewGame;
- (void)undoLastMove;
@end

@implementation DamaView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    _api    = dama_api();
    _st     = calloc(1, _api->state_size);
    _eng_st = calloc(1, _api->state_size);
    dama_init_state_str(_st, "startpos");
    _selSq    = -1;
    _lastFrom = -1;
    _lastTo   = -1;
    _message  = @"Configura la partita dal menu Partita → Nuova Partita";
    _stopEngine = 1;
    return self;
}

- (void)dealloc {
    free(_st);
    free(_eng_st);
}

// ----------------------------------------------------------------
// Game setup
// ----------------------------------------------------------------
- (void)resetGameState:(play_mode_t)mode pos:(const char*)pos
           humanColor:(int)hc timeMs:(int)tms handicap:(int)h {
    _stopEngine = 1;
    _generation++;
    _mode        = mode;
    _humanColor  = hc;
    _selSq       = -1;
    _legalTargets = 0;
    _lastFrom    = -1;
    _lastTo      = -1;
    _undoTop     = 0;
    _histN       = 0;
    _nW = _nB    = 0;
    memset(_hist,  0, sizeof(_hist));
    memset(_capsW, 0, sizeof(_capsW));
    memset(_capsB, 0, sizeof(_capsB));
    _message     = @"Partita iniziata";
    _sinfoValid  = NO;
    _scoreWhite  = 0;
    _engineBusy  = NO;

    if (dama_init_state_str(_st, pos) != 0)
        dama_init_state_str(_st, "startpos");

    // Apply handicap (remove h random pieces from computer side)
    if (mode == MODE_HVC && h > 0)
        [self applyHandicap:h computerColor:(hc ^ 1)];

    memset(&_sp, 0, sizeof(_sp));
    _sp.use_time    = 1;
    _sp.time_ms     = tms;
    _sp.max_depth   = 99;
    _sp.use_qsearch = 1;
    _sp.tt_size_mb  = 32;
    _sp.stop        = &_stopEngine;

    [self setNeedsDisplay:YES];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 80*NSEC_PER_MSEC),
                   dispatch_get_main_queue(), ^{ [self maybeRunEngine]; });
}

- (void)applyHandicap:(int)n computerColor:(int)cc {
    if (n <= 0) return;
    dama_board_t *b = (dama_board_t *)_st;  // board is first member of dama_state_t

    typedef struct { int sq; int pc; } PieceInfo;
    PieceInfo pool[32];
    int total = 0;
    for (int pi = DAMA_MAN; pi < DAMA_PIECE_N && total < 32; pi++) {
        uint64_t bb = b->bb[cc][pi];
        while (bb && total < 32) {
            pool[total].sq = bo_lsb_index(bb);
            pool[total].pc = pi;
            total++;
            bb &= bb - 1;
        }
    }
    if (n > total) n = total;
    for (int i = total - 1; i > 0; i--) {
        int j = (int)arc4random_uniform((uint32_t)(i + 1));
        PieceInfo tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    for (int i = 0; i < n; i++)
        dama_board_remove(b, cc, pool[i].pc, pool[i].sq);
}

- (void)showSetupDialog:(BOOL)startup {
    _stopEngine = 1;
    _generation++;

    NSView *box = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 360, 310)];
    float y = 285;

    NSTextField *(^lbl)(NSString*, float) = ^NSTextField*(NSString *t, float yy) {
        NSTextField *f = [NSTextField labelWithString:t];
        f.frame = NSMakeRect(0, yy, 360, 17);
        f.font  = [NSFont boldSystemFontOfSize:12];
        return f;
    };

    // — Modalità —
    [box addSubview:lbl(@"Modalità", y)]; y -= 26;
    NSSegmentedControl *modeSC = [NSSegmentedControl
        segmentedControlWithLabels:@[@"  Uomo vs Computer  ", @"  Computer vs Computer  "]
        trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:nil];
    modeSC.frame = NSMakeRect(0, y, 360, 26);
    [modeSC setSelectedSegment:0];
    [box addSubview:modeSC];
    y -= 36;

    // — Colore —
    [box addSubview:lbl(@"Il tuo colore", y)]; y -= 26;
    NSSegmentedControl *colorSC = [NSSegmentedControl
        segmentedControlWithLabels:@[@"  ⬜  Bianco  ", @"  ⬛  Nero  "]
        trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:nil];
    colorSC.frame = NSMakeRect(0, y, 240, 26);
    [colorSC setSelectedSegment:0];
    [box addSubview:colorSC];
    y -= 36;

    // — Difficoltà —
    [box addSubview:lbl(@"Difficoltà motore", y)]; y -= 26;
    NSSegmentedControl *diffSC = [NSSegmentedControl
        segmentedControlWithLabels:@[@"  Lampo 0.5s  ", @"  Normale 2s  ", @"  Profondo 6s  "]
        trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:nil];
    diffSC.frame = NSMakeRect(0, y, 360, 26);
    [diffSC setSelectedSegment:1];
    [box addSubview:diffSC];
    y -= 36;

    // — Handicap —
    [box addSubview:lbl(@"Handicap computer (solo HvC) — pezzi rimossi al computer", y)]; y -= 26;
    NSPopUpButton *hcPop = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(0, y, 340, 26) pullsDown:NO];
    [hcPop addItemWithTitle:@"0  —  nessun handicap"];
    for (int i = 1; i <= 12; i++)
        [hcPop addItemWithTitle:[NSString stringWithFormat:
            @"%d  —  %d pezzo/i rimossi al computer", i, i]];
    [hcPop selectItemAtIndex:0];
    [box addSubview:hcPop];
    y -= 34;

    // — Posizione (opzionale) —
    [box addSubview:lbl(@"Posizione (opzionale, formato W:W...:B...)", y)]; y -= 24;
    NSTextField *posField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, y, 360, 22)];
    posField.placeholderString = @"startpos";
    posField.font = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
    [box addSubview:posField];

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText     = startup ? @"Vibe Dama" : @"Nuova Partita";
    alert.informativeText = startup
        ? @"Benvenuto! Configura la partita e premi Inizia."
        : @"Configura la nuova partita:";
    alert.accessoryView = box;
    [alert addButtonWithTitle:@"Inizia"];
    if (!startup) [alert addButtonWithTitle:@"Annulla"];

    NSModalResponse resp = [alert runModal];
    if (!startup && resp == NSAlertSecondButtonReturn) return;

    play_mode_t mode = ([modeSC selectedSegment] == 0) ? MODE_HVC : MODE_CVC;
    int hc  = ([colorSC selectedSegment] == 0) ? DAMA_WHITE : DAMA_BLACK;
    int tms = ([diffSC  selectedSegment] == 0) ? 500 :
              ([diffSC  selectedSegment] == 1) ? 2000 : 6000;
    int hcp = (mode == MODE_HVC) ? (int)[hcPop indexOfSelectedItem] : 0;

    NSString *posStr = [posField.stringValue
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    const char *pos = (posStr.length > 0) ? posStr.UTF8String : "startpos";

    [self resetGameState:mode pos:pos humanColor:hc timeMs:tms handicap:hcp];
}

- (void)promptNewGame { [self showSetupDialog:NO]; }

// ----------------------------------------------------------------
// Drawing  (STUB — will be replaced with actual checkers rendering)
// ----------------------------------------------------------------
- (void)drawRect:(NSRect)__unused dirty {
    // ---- Board squares ----
    NSColor *lightSq = [NSColor colorWithRed:0.937 green:0.851 blue:0.710 alpha:1];
    NSColor *darkSq  = [NSColor colorWithRed:0.250 green:0.130 blue:0.050 alpha:1];

    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            NSRect sqr = NSMakeRect(BOARD_X + f*SQ_SIZE, BOARD_Y + r*SQ_SIZE, SQ_SIZE, SQ_SIZE);
            BOOL isDark = ((r + f) & 1) != 0;
            [(isDark ? darkSq : lightSq) setFill];
            NSRectFill(sqr);

            // Highlight last move
            int sq = r * 8 + f;
            if ((sq == _lastFrom || sq == _lastTo) && _lastFrom >= 0) {
                [[NSColor colorWithRed:0.45 green:0.78 blue:0.95 alpha:0.45] setFill];
                NSRectFillUsingOperation(sqr, NSCompositingOperationSourceOver);
            }
            if (sq == _selSq) {
                [[NSColor colorWithRed:0.98 green:0.90 blue:0.20 alpha:0.55] setFill];
                NSRectFillUsingOperation(sqr, NSCompositingOperationSourceOver);
            }

            // TODO: render actual dama pieces from _st board
            // For now draw placeholder circles on dark squares
            if (isDark) {
                const dama_board_t *b = dama_state_as_board(_st);
                uint64_t bit = 1ULL << sq;
                NSColor *pieceCol = nil;
                BOOL isKing = NO;
                if (b->bb[DAMA_WHITE][DAMA_MAN] & bit)        { pieceCol = [NSColor whiteColor]; }
                else if (b->bb[DAMA_WHITE][DAMA_KING] & bit)  { pieceCol = [NSColor whiteColor]; isKing = YES; }
                else if (b->bb[DAMA_BLACK][DAMA_MAN] & bit)   { pieceCol = [NSColor colorWithRed:0.12 green:0.12 blue:0.12 alpha:1]; }
                else if (b->bb[DAMA_BLACK][DAMA_KING] & bit)  { pieceCol = [NSColor colorWithRed:0.12 green:0.12 blue:0.12 alpha:1]; isKing = YES; }

                if (pieceCol) {
                    float margin = 6.0;
                    NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:
                        NSMakeRect(sqr.origin.x + margin, sqr.origin.y + margin,
                                   SQ_SIZE - 2*margin, SQ_SIZE - 2*margin)];
                    [pieceCol setFill];
                    [circle fill];
                    [[NSColor colorWithRed:0.5 green:0.35 blue:0.2 alpha:0.8] setStroke];
                    circle.lineWidth = 2.0;
                    [circle stroke];
                    if (isKing) {
                        // crown marker
                        NSDictionary *ka = @{
                            NSFontAttributeName: [NSFont boldSystemFontOfSize:18],
                            NSForegroundColorAttributeName: [NSColor colorWithRed:0.85 green:0.67 blue:0.20 alpha:1]
                        };
                        NSString *crown = @"♛";
                        NSSize cs = [crown sizeWithAttributes:ka];
                        [crown drawAtPoint:NSMakePoint(
                            sqr.origin.x + (SQ_SIZE - cs.width)  / 2.0,
                            sqr.origin.y + (SQ_SIZE - cs.height) / 2.0)
                            withAttributes:ka];
                    }
                }
            }

            // Legal target dots
            if (_legalTargets & (1ULL << sq)) {
                NSPoint ctr = NSMakePoint(BOARD_X + f*SQ_SIZE + SQ_SIZE/2,
                                          BOARD_Y + r*SQ_SIZE + SQ_SIZE/2);
                float dr = SQ_SIZE * 0.14;
                NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:
                    NSMakeRect(ctr.x - dr, ctr.y - dr, dr*2, dr*2)];
                [[NSColor colorWithRed:0.98 green:0.90 blue:0.20 alpha:0.70] setFill];
                [dot fill];
            }
        }
    }

    // ---- Coordinate labels ----
    NSDictionary *lblA = @{
        NSFontAttributeName: [NSFont systemFontOfSize:11],
        NSForegroundColorAttributeName: [NSColor secondaryLabelColor]
    };
    for (int f = 0; f < 8; f++) {
        [[NSString stringWithFormat:@"%c", 'a'+f]
         drawAtPoint:NSMakePoint(BOARD_X + f*SQ_SIZE + SQ_SIZE/2 - 4, BOARD_Y - 20)
         withAttributes:lblA];
    }
    for (int r = 0; r < 8; r++) {
        [[NSString stringWithFormat:@"%d", r+1]
         drawAtPoint:NSMakePoint(BOARD_X - 18, BOARD_Y + r*SQ_SIZE + SQ_SIZE/2 - 6)
         withAttributes:lblA];
    }

    // ---- Message bar ----
    NSDictionary *msgA = @{
        NSFontAttributeName: [NSFont systemFontOfSize:12],
        NSForegroundColorAttributeName: [NSColor systemBlueColor]
    };
    [_message drawAtPoint:NSMakePoint(BOARD_X, 14) withAttributes:msgA];

    // ---- Side panel ----
    [self drawPanel];
}

- (void)drawPanel {
    [[NSColor windowBackgroundColor] setFill];
    NSRectFill(NSMakeRect(PANEL_X - 1, 0, WIN_W - PANEL_X + 1, WIN_H));
    [[NSColor separatorColor] set];
    NSBezierPath *sep = [NSBezierPath bezierPath];
    [sep moveToPoint:NSMakePoint(PANEL_X, 0)];
    [sep lineToPoint:NSMakePoint(PANEL_X, WIN_H)];
    sep.lineWidth = 1;
    [sep stroke];

    float x = PANEL_X + 14;
    float y = WIN_H - 28;

    NSDictionary *titleA = @{
        NSFontAttributeName: [NSFont boldSystemFontOfSize:15],
        NSForegroundColorAttributeName: [NSColor labelColor]
    };
    NSDictionary *hdrA = @{
        NSFontAttributeName: [NSFont boldSystemFontOfSize:11],
        NSForegroundColorAttributeName: [NSColor labelColor]
    };
    NSDictionary *bodyA = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular],
        NSForegroundColorAttributeName: [NSColor secondaryLabelColor]
    };
    NSDictionary *hiA = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName: [NSColor labelColor]
    };

    [@"Vibe Dama" drawAtPoint:NSMakePoint(x, y) withAttributes:titleA];
    y -= 22;

    int stm = _api->side_to_move(_st);
    NSString *modeS = (_mode == MODE_HVC) ? @"HvC" : @"CvC";
    NSString *turnS = (stm == DAMA_WHITE) ? @"Bianco" : @"Nero";
    NSString *busyS = _engineBusy ? @" ⏳" : @"";
    [[NSString stringWithFormat:@"%@ — Turno: %@%@", modeS, turnS, busyS]
     drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA];
    y -= 22;

    // Eval bar
    [self drawEvalBar:x y:y width:WIN_W - PANEL_X - 28]; y -= 26;

    // Search info
    if (_sinfoValid) {
        int sc = (_sinfoSide == DAMA_WHITE) ? _sinfoScore : -_sinfoScore;
        NSString *scS = (abs(sc) >= 29000)
            ? [NSString stringWithFormat:@"%sMatto", sc > 0 ? "+" : "-"]
            : [NSString stringWithFormat:@"%+.2f", sc / 100.0];
        [[NSString stringWithFormat:@"Score: %@  depth: %d", scS, _sinfoDepth]
         drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 16;
        NSString *npsS = (_sinfoNPS >= 1e6)
            ? [NSString stringWithFormat:@"%.1fM nps", _sinfoNPS/1e6]
            : [NSString stringWithFormat:@"%.0fK nps", _sinfoNPS/1e3];
        [[NSString stringWithFormat:@"%@  t:%.1fs  TT:%.0f%%",
          npsS, _sinfoTimeSec, _sinfoTTRate]
         drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 16;
        if (_sinfoPV[0]) {
            [[NSString stringWithFormat:@"PV: %@",
              [NSString stringWithUTF8String:_sinfoPV]]
             drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 16;
        }
        y -= 4;
    }

    // Move history
    [[NSColor separatorColor] setFill];
    NSRectFill(NSMakeRect(x, y, WIN_W - PANEL_X - 28, 1)); y -= 18;
    [@"Ultime mosse" drawAtPoint:NSMakePoint(x, y) withAttributes:hdrA]; y -= 16;
    int startH = (_histN > 12) ? _histN - 12 : 0;
    for (int i = startH; i < _histN; i++) {
        NSString *ms = [NSString stringWithUTF8String:_hist[i]];
        [[NSString stringWithFormat:@"%2d. %@", i+1, ms]
         drawAtPoint:NSMakePoint(x, y)
         withAttributes:(i == _histN-1) ? hiA : bodyA];
        y -= 15;
    }
    if (_histN == 0) {
        [@"(nessuna mossa)" drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 15;
    }

    // Captured pieces
    y -= 4;
    [[NSColor separatorColor] setFill];
    NSRectFill(NSMakeRect(x, y, WIN_W - PANEL_X - 28, 1)); y -= 18;
    [@"Pezzi catturati" drawAtPoint:NSMakePoint(x, y) withAttributes:hdrA]; y -= 16;
    [[NSString stringWithFormat:@"⬛ %s", _nW > 0 ? _capsW : "—"]
     drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 15;
    [[NSString stringWithFormat:@"⬜ %s", _nB > 0 ? _capsB : "—"]
     drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA];

    // Keyboard hint
    NSDictionary *hintA = @{
        NSFontAttributeName: [NSFont systemFontOfSize:10],
        NSForegroundColorAttributeName: [NSColor tertiaryLabelColor]
    };
    [@"⌘N Nuova Partita  ·  ⌘Z Annulla mossa"
     drawAtPoint:NSMakePoint(x, 10) withAttributes:hintA];
}

- (void)drawEvalBar:(float)x y:(float)y width:(float)w {
    float h = 20.0, sc = _scoreWhite / 100.0f;
    if (sc > 4.0f) sc = 4.0f; if (sc < -4.0f) sc = -4.0f;
    float wFrac = (sc + 4.0f) / 8.0f;
    [[NSColor colorWithRed:0.15 green:0.15 blue:0.15 alpha:1] setFill]; NSRectFill(NSMakeRect(x, y, w, h));
    [[NSColor colorWithRed:0.93 green:0.93 blue:0.93 alpha:1] setFill]; NSRectFill(NSMakeRect(x + w - wFrac*w, y, wFrac*w, h));
    NSString *lbl = (_scoreWhite >= 29000) ? @"+M" : (_scoreWhite <= -29000) ? @"−M"
        : [NSString stringWithFormat:@"%+.1f", _scoreWhite / 100.0];
    NSDictionary *la = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:10],
                          NSForegroundColorAttributeName: [NSColor systemGreenColor] };
    NSSize ls = [lbl sizeWithAttributes:la];
    [lbl drawAtPoint:NSMakePoint(x + w/2 - ls.width/2, y + (h - ls.height)/2 + 1) withAttributes:la];
}

// ----------------------------------------------------------------
// Input
// ----------------------------------------------------------------
- (BOOL)acceptsFirstResponder { return YES; }

- (void)mouseDown:(NSEvent *)ev {
    if (_engineBusy) return;
    game_result_t gr = GAME_RESULT_NONE;
    if (_api->is_terminal(_st, &gr)) return;
    NSPoint pt = [self convertPoint:ev.locationInWindow fromView:nil];
    int sq = [self squareAt:pt];
    if (sq < 0) return;

    int stm = _api->side_to_move(_st);
    if (_mode == MODE_HVC && stm != _humanColor) return;

    const dama_board_t *b = dama_state_as_board(_st);

    if (_selSq < 0) {
        if (b->occ[stm] & (1ULL << sq)) {
            _selSq = sq;
            _legalTargets = [self legalTargetsFrom:sq];
            [self setNeedsDisplay:YES];
        }
    } else {
        if (sq == _selSq) {
            _selSq = -1; _legalTargets = 0;
            [self setNeedsDisplay:YES];
        } else if (_legalTargets & (1ULL << sq)) {
            game_move_t mv = [self findMove:_selSq to:sq];
            _selSq = -1; _legalTargets = 0;
            if (mv) [self applyMove:mv];
        } else if (b->occ[stm] & (1ULL << sq)) {
            _selSq = sq;
            _legalTargets = [self legalTargetsFrom:sq];
            [self setNeedsDisplay:YES];
        } else {
            _selSq = -1; _legalTargets = 0;
            [self setNeedsDisplay:YES];
        }
    }
}

- (void)keyDown:(NSEvent *)ev {
    if (ev.keyCode == 53) { _selSq = -1; _legalTargets = 0; [self setNeedsDisplay:YES]; }
}

- (int)squareAt:(NSPoint)pt {
    float rx = pt.x - BOARD_X, ry = pt.y - BOARD_Y;
    if (rx < 0 || rx >= 8*SQ_SIZE || ry < 0 || ry >= 8*SQ_SIZE) return -1;
    return (int)(ry / SQ_SIZE) * 8 + (int)(rx / SQ_SIZE);
}

- (uint64_t)legalTargetsFrom:(int)from {
    game_move_t buf[DAMA_MAX_MOVES];
    int n = _api->generate_legal(_st, buf, DAMA_MAX_MOVES);
    uint64_t mask = 0;
    for (int i = 0; i < n; i++) {
        if (dama_move_from(buf[i]) == from)
            mask |= 1ULL << dama_move_to(buf[i]);
    }
    return mask;
}

- (game_move_t)findMove:(int)from to:(int)to {
    game_move_t buf[DAMA_MAX_MOVES];
    int n = _api->generate_legal(_st, buf, DAMA_MAX_MOVES);
    for (int i = 0; i < n; i++) {
        if (dama_move_from(buf[i]) == from && dama_move_to(buf[i]) == to)
            return buf[i];
    }
    return 0;
}

- (void)applyMove:(game_move_t)mv {
    int stm = _api->side_to_move(_st);

    if (_undoTop < UNDO_MAX) {
        undo_entry_t *e = &_undo[_undoTop];
        e->mv = mv;
        memcpy(e->caps_w, _capsW, sizeof(_capsW)); e->n_w = _nW;
        memcpy(e->caps_b, _capsB, sizeof(_capsB)); e->n_b = _nB;
        e->hist_n = _histN;
        [self recordCapture:mv moverSide:stm];
        _api->make_move(_st, mv, e->buf);
        _undoTop++;
    } else {
        void *tmp = malloc(_api->undo_size);
        if (tmp) { [self recordCapture:mv moverSide:stm]; _api->make_move(_st, mv, tmp); free(tmp); }
    }

    _lastFrom = dama_move_from(mv);
    _lastTo   = dama_move_to(mv);

    char mstr[12] = {0};
    dama_move_to_str(mv, mstr, sizeof(mstr));
    if (_histN < 20) strncpy(_hist[_histN++], mstr, 11);
    else {
        memmove(_hist[0], _hist[1], 19 * sizeof(_hist[0]));
        strncpy(_hist[19], mstr, 11); _hist[19][11] = '\0';
    }

    game_result_t post = GAME_RESULT_NONE;
    int terminal = _api->is_terminal(_st, &post);
    if (terminal && post == GAME_RESULT_LOSS)
        _message = [NSString stringWithFormat:@"Vince %@!", (stm == DAMA_WHITE) ? @"Bianco" : @"Nero"];
    else if (terminal && post == GAME_RESULT_DRAW)
        _message = @"Patta!";
    else
        _message = [NSString stringWithFormat:@"Ultima mossa: %s", mstr];

    [self setNeedsDisplay:YES];
    if (!terminal) [self maybeRunEngine];
}

- (void)recordCapture:(game_move_t)mv moverSide:(int)side {
    if (!dama_move_is_cap(mv)) return;
    int victim = side ^ 1;
    int capsq   = dama_move_cap_sq(mv);
    const dama_board_t *b = dama_state_as_board(_st);
    char g = (b->bb[victim][DAMA_KING] & (1ULL << capsq)) ? 'D' : 'P';
    if (victim == DAMA_BLACK) g = (char)tolower((unsigned char)g);
    if (victim == DAMA_WHITE && _nW < 31) { _capsW[_nW++] = g; _capsW[_nW] = '\0'; }
    else if (victim == DAMA_BLACK && _nB < 31) { _capsB[_nB++] = g; _capsB[_nB] = '\0'; }
}

- (void)undoLastMove {
    if (_engineBusy) { _stopEngine = 1; _generation++; _engineBusy = NO; }
    if (_undoTop == 0) { _message = @"Nessuna mossa da annullare"; [self setNeedsDisplay:YES]; return; }
    int steps = (_mode == MODE_HVC && _undoTop >= 2) ? 2 : 1;
    for (int i = 0; i < steps && _undoTop > 0; i++) {
        undo_entry_t *e = &_undo[--_undoTop];
        _api->unmake_move(_st, e->mv, e->buf);
        memcpy(_capsW, e->caps_w, sizeof(_capsW)); _nW = e->n_w;
        memcpy(_capsB, e->caps_b, sizeof(_capsB)); _nB = e->n_b;
        _histN = e->hist_n;
    }
    _selSq = -1; _legalTargets = 0; _lastFrom = -1; _lastTo = -1;
    _message = @"Mossa annullata";
    [self setNeedsDisplay:YES];
}

// ----------------------------------------------------------------
// Engine
// ----------------------------------------------------------------
- (void)maybeRunEngine {
    int stm = _api->side_to_move(_st);
    game_result_t gr = GAME_RESULT_NONE;
    if (_api->is_terminal(_st, &gr)) return;
    if (_mode == MODE_HVC && stm == _humanColor) return;
    if (_engineBusy) return;

    _engineBusy = YES; _stopEngine = 0;
    int gen = _generation;
    [self setNeedsDisplay:YES];
    memcpy(_eng_st, _st, _api->state_size);

    engine_job_t *job = calloc(1, sizeof(engine_job_t));
    job->api = _api; job->st = _eng_st; job->sp = _sp;
    job->completion = ^(search_result_t *out) {
        self->_engineBusy = NO;
        if (self->_generation != gen) { [self setNeedsDisplay:YES]; return; }
        if (out->best_move == 0ULL) { self->_message = @"Motore: nessuna mossa"; [self setNeedsDisplay:YES]; return; }
        self->_scoreWhite   = (stm == DAMA_WHITE) ? out->score : -out->score;
        self->_sinfoValid   = YES;
        self->_sinfoSide    = stm;
        self->_sinfoScore   = out->score;
        self->_sinfoDepth   = out->depth_searched;
        self->_sinfoNodes   = out->nodes;
        self->_sinfoNPS     = out->nps;
        self->_sinfoTimeSec = out->time_ns / 1e9;
        self->_sinfoTTRate  = out->tt_probes > 0 ? (100.0 * out->tt_hits / out->tt_probes) : 0.0;
        self->_sinfoPV[0] = '\0';
        int pvN = out->pv_len < 5 ? out->pv_len : 5;
        for (int i = 0; i < pvN; i++) {
            char ms[8]; dama_move_to_str(out->pv[i], ms, sizeof(ms));
            if (i) strncat(self->_sinfoPV, " ", sizeof(self->_sinfoPV) - strlen(self->_sinfoPV) - 1);
            strncat(self->_sinfoPV, ms, sizeof(self->_sinfoPV) - strlen(self->_sinfoPV) - 1);
        }
        [self applyMove:out->best_move];
    };

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u * 1024u * 1024u);
    pthread_t tid;
    pthread_create(&tid, &attr, engine_thread_func, job);
    pthread_attr_destroy(&attr);
    pthread_detach(tid);
}

@end // DamaView

// ================================================================
// SplashView
// ================================================================
@interface SplashView : NSView
@property (copy) void (^onTap)(void);
@end

@implementation SplashView

- (void)drawRect:(NSRect)__unused dirty {
    NSRect bounds = self.bounds;
    const float W = bounds.size.width, H = bounds.size.height;

    NSGradient *bg = [[NSGradient alloc] initWithColors:@[
        [NSColor colorWithRed:0.07 green:0.09 blue:0.18 alpha:1],
        [NSColor colorWithRed:0.02 green:0.02 blue:0.07 alpha:1]
    ]];
    [bg drawInRect:bounds angle:130];

    // ---- Mini checkers board ----
    const float SQ = 28.0, BW = SQ * 8;
    const float bx = (W - BW) / 2.0;
    const float by = H - 28.0 - BW;

    NSColor *gold = [NSColor colorWithRed:0.82 green:0.67 blue:0.28 alpha:1];
    [gold set]; NSFrameRect(NSMakeRect(bx-2, by-2, BW+4, BW+4));

    NSColor *sqLight = [NSColor colorWithRed:0.94 green:0.85 blue:0.71 alpha:1];
    NSColor *sqDark  = [NSColor colorWithRed:0.25 green:0.13 blue:0.05 alpha:1];

    // Standard Italian checkers starting position
    // White: a1,c1,e1,g1, b2,d2,f2,h2, a3,c3,e3,g3   (ranks 1-3)
    // Black: b6,d6,f6,h6, a7,c7,e7,g7, b8,d8,f8,h8   (ranks 6-8)
    uint64_t wMen = 0, bMen = 0;
    // rank 1 (r=0): a1=0,c1=2,e1=4,g1=6
    // rank 2 (r=1): b2=9,d2=11,f2=13,h2=15
    // rank 3 (r=2): a3=16,c3=18,e3=20,g3=22
    int wSqs[] = {0,2,4,6, 9,11,13,15, 16,18,20,22};
    // rank 6 (r=5): b6=41,d6=43,f6=45,h6=47
    // rank 7 (r=6): a7=48,c7=50,e7=52,g7=54
    // rank 8 (r=7): b8=57,d8=59,f8=61,h8=63
    int bSqs[] = {41,43,45,47, 48,50,52,54, 57,59,61,63};
    for (int i = 0; i < 12; i++) { wMen |= 1ULL << wSqs[i]; bMen |= 1ULL << bSqs[i]; }

    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            int drawRank = r; // rank 0 at bottom
            NSRect sqr = NSMakeRect(bx + f*SQ, by + drawRank*SQ, SQ, SQ);
            BOOL isDark = ((r + f) & 1) != 0;
            [(isDark ? sqDark : sqLight) setFill]; NSRectFill(sqr);

            if (!isDark) continue;
            int sq = r * 8 + f;
            uint64_t bit = 1ULL << sq;
            NSColor *pc = nil;
            if (wMen & bit)      pc = [NSColor colorWithRed:0.95 green:0.92 blue:0.85 alpha:1];
            else if (bMen & bit) pc = [NSColor colorWithRed:0.18 green:0.10 blue:0.06 alpha:1];
            if (pc) {
                float m = 5.0;
                NSBezierPath *circ = [NSBezierPath bezierPathWithOvalInRect:
                    NSMakeRect(sqr.origin.x+m, sqr.origin.y+m, SQ-2*m, SQ-2*m)];
                [pc setFill]; [circ fill];
                [gold setStroke]; circ.lineWidth = 1.5; [circ stroke];
            }
        }
    }

    // ---- Title ----
    float titleY = by - 54;
    NSDictionary *ta = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:40],
                          NSForegroundColorAttributeName: gold };
    NSString *title = @"Vibe Dama";
    NSSize ts = [title sizeWithAttributes:ta];
    [title drawAtPoint:NSMakePoint((W-ts.width)/2.0, titleY) withAttributes:ta];

    // ---- Credits ----
    NSColor *bright = [NSColor colorWithRed:0.80 green:0.80 blue:0.86 alpha:1];
    NSColor *dim    = [NSColor colorWithRed:0.50 green:0.50 blue:0.56 alpha:1];
    struct { NSString *text; float size; NSColor *color; } lines[] = {
        { @"realizzato da sb – Haglard",                           13, bright },
        { @"between 2022 and 2026 · using ChatGPT and Claude",     11, bright },
        { @"with a swift acceleration in March 2026",              11, dim    },
        { @"not a single line of code is crafted by a human",      11, dim    },
    };
    float cy = titleY - 30;
    for (int i = 0; i < 4; i++) {
        NSDictionary *ca = @{ NSFontAttributeName: [NSFont systemFontOfSize:lines[i].size],
                              NSForegroundColorAttributeName: lines[i].color };
        NSSize ls = [lines[i].text sizeWithAttributes:ca];
        [lines[i].text drawAtPoint:NSMakePoint((W-ls.width)/2.0, cy) withAttributes:ca];
        cy -= (i == 0) ? 21 : 18;
    }

    NSDictionary *ha = @{ NSFontAttributeName: [NSFont systemFontOfSize:10],
                          NSForegroundColorAttributeName: [NSColor colorWithRed:0.35 green:0.35 blue:0.40 alpha:1] };
    NSString *hint = @"click to start";
    NSSize hs = [hint sizeWithAttributes:ha];
    [hint drawAtPoint:NSMakePoint((W-hs.width)/2.0, 12) withAttributes:ha];
}

- (BOOL)acceptsFirstResponder { return YES; }
- (void)mouseDown:(NSEvent *)__unused ev { if (_onTap) _onTap(); }
@end

// ================================================================
// SplashController
// ================================================================
@interface SplashController : NSObject <NSWindowDelegate>
@property (strong) NSPanel *panel;
@property (copy)   void (^completion)(void);
@end

@implementation SplashController

- (void)showRelativeTo:(NSWindow *)parent completion:(void(^)(void))done {
    _completion = done;
    const CGFloat W = 480, H = 460;
    NSRect pf = parent.frame;
    NSRect fr = NSMakeRect(pf.origin.x + (pf.size.width-W)/2.0,
                           pf.origin.y + (pf.size.height-H)/2.0, W, H);
    _panel = [[NSPanel alloc] initWithContentRect:fr
        styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    _panel.opaque = NO; _panel.backgroundColor = [NSColor clearColor];
    _panel.hasShadow = YES; _panel.delegate = self; _panel.level = NSFloatingWindowLevel;
    SplashView *sv = [[SplashView alloc] initWithFrame:NSMakeRect(0,0,W,H)];
    __weak SplashController *ws = self;
    sv.onTap = ^{ [ws dismiss]; };
    _panel.contentView = sv;
    [_panel makeKeyAndOrderFront:nil];
    [_panel makeFirstResponder:sv];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(60.0 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{ [ws dismiss]; });
}

- (void)dismiss {
    if (!_panel) return;
    NSPanel *p = _panel; _panel = nil;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *ctx) {
        ctx.duration = 0.4; p.animator.alphaValue = 0.0;
    } completionHandler:^{ [p close]; }];
}

- (void)windowWillClose:(NSNotification *)__unused n {
    if (_completion) { void (^cb)(void) = _completion; _completion = nil; cb(); }
}
@end

// ================================================================
// AppDelegate
// ================================================================
@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSWindow         *window;
@property (strong) DamaView         *board;
@property (strong) SplashController *splash;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)__unused n {
    NSRect fr = NSMakeRect(120, 120, WIN_W, WIN_H);
    self.window = [[NSWindow alloc]
        initWithContentRect:fr
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title    = @"Vibe Dama";
    self.window.delegate = self;
    self.window.releasedWhenClosed = NO;

    self.board = [[DamaView alloc] initWithFrame:NSMakeRect(0, 0, WIN_W, WIN_H)];
    self.window.contentView = self.board;

    [self buildMenuBar];
    [self.window makeFirstResponder:self.board];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    self.splash = [[SplashController alloc] init];
    [self.splash showRelativeTo:self.window completion:^{
        self.splash = nil;
        [self.board showSetupDialog:YES];
    }];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)__unused a { return YES; }

- (void)buildMenuBar {
    NSMenu *bar = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"Vibe Dama"];
    [appMenu addItemWithTitle:@"Esci" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;
    [bar addItem:appItem];

    NSMenuItem *gameItem = [[NSMenuItem alloc] initWithTitle:@"Partita" action:nil keyEquivalent:@""];
    NSMenu *gameMenu = [[NSMenu alloc] initWithTitle:@"Partita"];
    [gameMenu addItemWithTitle:@"Nuova Partita…" action:@selector(newGame:) keyEquivalent:@"n"];
    [gameMenu addItem:[NSMenuItem separatorItem]];
    [gameMenu addItemWithTitle:@"Annulla Mossa"  action:@selector(undoMove:) keyEquivalent:@"z"];
    gameItem.submenu = gameMenu;
    [bar addItem:gameItem];
    [NSApp setMainMenu:bar];
}

- (void)newGame:(id)__unused s  { [self.board promptNewGame]; }
- (void)undoMove:(id)__unused s { [self.board undoLastMove]; }
@end

// ================================================================
// main
// ================================================================
int main(int __unused argc, const char *__unused argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppDelegate *del = [[AppDelegate alloc] init];
        app.delegate = del;
        [app run];
    }
    return 0;
}
