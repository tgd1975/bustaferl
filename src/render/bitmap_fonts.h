#ifndef BUSTAFERL_RENDER_BITMAP_FONTS_H
#define BUSTAFERL_RENDER_BITMAP_FONTS_H

#include <cstdint>

namespace bustaferl {

// Logical font roles used across the v2 display. Sizes follow the design
// handoff (docs/design_handoff_display/README.md "Typography"). Comments
// capture the design-spec size; the concrete font on the ESP32 (U8g2) may
// be 1–2 px off due to library coverage (acknowledged drift per Risiko V8).
// The host renderer uses a builtin 5×7 font scaled per role (close enough
// per the apprentice contract).
enum class FontRole : std::uint8_t {
  TG_Row,   // 28 px — TULLNERTALGASSE row times + line label
  EG_Row,   // 22 px — ENDEMANNGASSE row
  Atzg_Row, // 20 px — ATZGERSDORF S-Bahn row

  Section_Header_TG,      // 12 px
  Section_Header_EG_Atzg, // 10 px

  Network_Label, //  7 px
  Network_Arrow, // 10 px

  Badge_sm, // 14 px
  Badge_md, // 18 px
  Badge_lg, // 22 px

  Fullscreen_Glyph_90, // 90 px sentinel (use drawCustomGlyph, not text)
  Fullscreen_Title,    // 18 px
  Fullscreen_Sub,      // 16 px
  Fullscreen_Foot,     //  8 px
};

} // namespace bustaferl

#endif
