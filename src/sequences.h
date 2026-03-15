#ifndef SEQUENCES_H
#define SEQUENCES_H

// LED Sequence Configuration
//
// Grid Layout (7x3) — up to 21 games:
//   A1  B1  C1  D1  E1  F1  G1
//   A2  B2  C2  D2  E2  F2  G2
//   A3  B3  C3  D3  E3  F3  G3
//
// Each game has a set of selectors (e.g. players).
// Each selector has:
//   - indicator: JSON object of LEDs to light during player selection
//   - sequence:  JSON array of frames to play once the player is chosen
//
// After the sequence finishes, player selection restarts.

// ---------------------------------------------------------------------------
// Selector: indicator LEDs + sequence frames
// ---------------------------------------------------------------------------

struct SelectorDef {
    const char* indicator;   // JSON object — LEDs shown while this selector is highlighted
    const char* sequence;    // JSON array  — frames played after this selector is confirmed
};

// ---------------------------------------------------------------------------
// Game A1 — Agricola
// ---------------------------------------------------------------------------

const char* AGRICOLA_RED_INDICATOR = R"({"A1": "#FF0000"})";
const char* AGRICOLA_RED_SEQUENCE = R"([
  {"B1": "#FF0000"},
  {"B2": "#0000FF"},
  {"C1": "#FF0000"},
  {"C2": "#0000FF"},
  {"D1": "#FF0000"},
  {"D2": "#0000FF"},
  {"E1": "#FF0000", "E2": "#0000FF"},
  {"F1": "#FF0000", "F2": "#0000FF"},
  {"A3": "#808080"},
  {"B3": "#8B4513"},
  {"C3": "#FFFF00"},
  {"D3": "#FFF8E7"},
  {"E3": "#808080"},
  {"F3": "#8B4513"},
  {"G3": "#D2B48C"}
])";

const char* AGRICOLA_BLUE_INDICATOR = R"({"A2": "#0000FF"})";
const char* AGRICOLA_BLUE_SEQUENCE = R"([
  {"B2": "#0000FF"},
  {"B1": "#FF0000"},
  {"C2": "#0000FF"},
  {"C1": "#FF0000"},
  {"D2": "#0000FF"},
  {"D1": "#FF0000"},
  {"E1": "#FF0000", "E2": "#0000FF"},
  {"F1": "#FF0000", "F2": "#0000FF"},
  {"A3": "#808080"},
  {"B3": "#8B4513"},
  {"C3": "#FFFF00"},
  {"D3": "#FFF8E7"},
  {"E3": "#808080"},
  {"F3": "#8B4513"},
  {"G3": "#D2B48C"}
])";

const SelectorDef AGRICOLA_SELECTORS[] = {
    {AGRICOLA_RED_INDICATOR, AGRICOLA_RED_SEQUENCE},
    {AGRICOLA_BLUE_INDICATOR, AGRICOLA_BLUE_SEQUENCE},
};

// ---------------------------------------------------------------------------
// Game A2 — Placeholder (Green vs Yellow)
// ---------------------------------------------------------------------------

const char* A2_GREEN_INDICATOR = R"({"A1": "#00FF00"})";
const char* A2_GREEN_SEQUENCE = R"([
  {"E1": "#00FF00"},
  {"F1": "#00FF00"},
  {"G1": "#00FF00"}
])";

const char* A2_YELLOW_INDICATOR = R"({"A2": "#FFFF00"})";
const char* A2_YELLOW_SEQUENCE = R"([
  {"E1": "#FFFF00"},
  {"F1": "#FFFF00"},
  {"G1": "#FFFF00"}
])";

const SelectorDef A2_SELECTORS[] = {
    {A2_GREEN_INDICATOR, A2_GREEN_SEQUENCE},
    {A2_YELLOW_INDICATOR, A2_YELLOW_SEQUENCE},
};

// ---------------------------------------------------------------------------
// Game registry — add new games here
// ---------------------------------------------------------------------------

struct GameDef {
    const char* gridPos;
    const SelectorDef* selectors;
    int numSelectors;
};

const GameDef GAMES[] = {
    {"A1", AGRICOLA_SELECTORS, sizeof(AGRICOLA_SELECTORS) / sizeof(AGRICOLA_SELECTORS[0])},
    {"A2", A2_SELECTORS, sizeof(A2_SELECTORS) / sizeof(A2_SELECTORS[0])},
};

const int NUM_GAMES = sizeof(GAMES) / sizeof(GAMES[0]);

#endif // SEQUENCES_H
