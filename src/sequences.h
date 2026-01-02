#ifndef SEQUENCES_H
#define SEQUENCES_H

// LED Sequence Configuration
// Edit this JSON to change your LED patterns
// 
// Format:
// - Each object in the array is a sequence (button press)
// - Keys are grid positions: A-G (columns) and 1-3 (rows)
// - Values are hex color codes: "#RRGGBB" format
//
// Grid Layout (7x3):
//   A1  B1  C1  D1  E1  F1  G1
//   A2  B2  C2  D2  E2  F2  G2
//   A3  B3  C3  D3  E3  F3  G3
//
// Example hex colors:
//   "#FFFFFF" = White
//   "#FF0000" = Red
//   "#00FF00" = Green
//   "#0000FF" = Blue
//   "#FFFF00" = Yellow
//   "#FF00FF" = Magenta
//   "#00FFFF" = Cyan

const char* SEQUENCES_JSON = R"(
[
  {
    "A1": "#FFFFFF",
    "A2": "#FF0000"
  },
  {
    "B1": "#0000FF"
  },
  {
    "C1": "#00FF00",
    "D2": "#FFFF00",
    "G3": "#FF00FF"
  }
]
)";

#endif // SEQUENCES_H

