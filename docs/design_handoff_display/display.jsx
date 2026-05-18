// Shared e-paper display primitives.
// Real pixel dimensions: 400 × 300 (Waveshare 4.2" UC8176, B/W).
// Rendered at 2× via CSS scale for legibility in the canvas.

const EPD_W = 400;
const EPD_H = 300;

const ink = '#0d0d0d';      // "schwarz"
const paper = '#f4f1e8';    // leicht warmes "Papier-Weiß" — wirkt echter als reines #fff
const paperShadow = 'rgba(0,0,0,0.08)';

// Faux-bitmap rendering: turn off antialiasing where the browser will let us.
const pixelText = {
  WebkitFontSmoothing: 'none',
  MozOsxFontSmoothing: 'unset',
  textRendering: 'geometricPrecision',
  fontFeatureSettings: '"liga" 0, "calt" 0',
};

// The display chrome — black bezel, subtle drop shadow, scale wrapper.
const Display = ({ children, scale = 2, label, sublabel, invert = false }) => {
  const bg = invert ? ink : paper;
  const fg = invert ? paper : ink;
  return (
    <div style={{
      display: 'inline-flex',
      flexDirection: 'column',
      gap: 10,
      alignItems: 'flex-start',
    }}>
      {/* Bezel */}
      <div style={{
        padding: 14 * (scale / 2),
        background: '#1a1a1a',
        borderRadius: 6,
        boxShadow: `0 ${6*scale}px ${20*scale}px rgba(0,0,0,0.18), inset 0 0 0 1px #2a2a2a`,
      }}>
        <div style={{
          width: EPD_W * scale,
          height: EPD_H * scale,
          overflow: 'hidden',
          background: bg,
          boxShadow: `inset 0 0 0 1px rgba(0,0,0,0.25)`,
          imageRendering: 'pixelated',
        }}>
          <div style={{
            transform: `scale(${scale})`,
            transformOrigin: '0 0',
            width: EPD_W,
            height: EPD_H,
            color: fg,
            background: bg,
            fontFamily: '"VT323", ui-monospace, monospace',
            ...pixelText,
          }}>
            {children}
          </div>
        </div>
      </div>
      {/* Footer caption */}
      {label && (
        <div style={{
          fontFamily: '"Silkscreen", ui-monospace, monospace',
          fontSize: 10,
          letterSpacing: 1,
          color: '#3a3530',
          textTransform: 'uppercase',
        }}>
          <span style={{ color: '#0d0d0d', fontWeight: 700 }}>{label}</span>
          {sublabel && <span style={{ color: '#7a6f60', marginLeft: 8 }}>· {sublabel}</span>}
          <span style={{ color: '#a59c8e', marginLeft: 8 }}>· 400 × 300 · b/w</span>
        </div>
      )}
    </div>
  );
};

// Layout primitives — all measured in real e-paper pixels.

const Block = ({ children, style }) => (
  <div style={{ padding: '0 18px', ...style }}>{children}</div>
);

// Section header — bold all-caps with a 1px underline rule.
const Hdr = ({ children, rule = true, tight = false, style }) => (
  <div style={{
    fontFamily: '"Silkscreen", ui-monospace, monospace',
    fontWeight: 700,
    fontSize: 11,
    letterSpacing: 1,
    textTransform: 'uppercase',
    paddingBottom: 2,
    marginBottom: tight ? 1 : 3,
    borderBottom: rule ? `1px solid currentColor` : 'none',
    lineHeight: 1,
    ...style,
  }}>{children}</div>
);

// One data line: line code + direction (left) and time slots (right).
// `right` may be a Fragment of multiple <Time> elements; we splay them into
// fixed grid columns so the gap between times is deterministic and the left
// block keeps its natural width (no flex-shrink clipping).
// Flatten <>…</> wrappers — React.Children.toArray treats a Fragment as
// a single opaque child, which collapses our per-slot grid columns.
const unwrapFragment = (node) => {
  if (React.isValidElement(node) && node.type === React.Fragment) {
    return React.Children.toArray(node.props.children);
  }
  return React.Children.toArray(node);
};

const Row = ({ left, right, sub, style }) => {
  const rightArr = unwrapFragment(right);
  return (
    <div style={{
      display: 'grid',
      gridTemplateColumns: `1fr repeat(${Math.max(1, rightArr.length)}, auto)`,
      columnGap: 22,
      alignItems: 'baseline',
      fontSize: 22,
      lineHeight: '22px',
      fontFamily: '"VT323", ui-monospace, monospace',
      marginTop: 4,
      whiteSpace: 'nowrap',
      ...style,
    }}>
      <div style={{ minWidth: 0 }}>{left}</div>
      {rightArr.map((node, i) => <div key={i}>{node}</div>)}
    </div>
  );
};

const Sub = ({ children, style }) => (
  <div style={{
    fontFamily: '"VT323", ui-monospace, monospace',
    fontSize: 14,
    lineHeight: '14px',
    marginTop: 1,
    marginLeft: 30,
    opacity: 0.7,
    ...style,
  }}>{children}</div>
);

const Time = ({ children, dim = false, style }) => (
  <span style={{
    fontVariantNumeric: 'tabular-nums',
    opacity: dim ? 0.35 : 1,
    ...style,
  }}>{children}</span>
);

// Inverted line badge — black rectangle, white code inside.
// Looks like a bus/train roll sign at 1× pixel-grid.
const Badge = ({ children, invertHost = false, size = 'md', style }) => {
  const sizes = {
    sm: { fontSize: 9,  padding: '0px 3px', minWidth: 18, lineHeight: '12px' },
    md: { fontSize: 11, padding: '1px 4px', minWidth: 22, lineHeight: '14px' },
    lg: { fontSize: 14, padding: '2px 5px', minWidth: 28, lineHeight: '17px' },
  };
  return (
    <span style={{
      display: 'inline-block',
      background: invertHost ? paper : ink,
      color: invertHost ? ink : paper,
      fontFamily: '"Silkscreen", ui-monospace, monospace',
      fontWeight: 700,
      letterSpacing: 0.5,
      verticalAlign: 'baseline',
      textAlign: 'center',
      ...sizes[size],
      ...style,
    }}>{children}</span>
  );
};

// Arrow glyph — using a heavy unicode right-arrow rendered crisp.
const Arrow = () => <span style={{ margin: '0 4px' }}>→</span>;

// Status banner at bottom of display.
const Banner = ({ children, style }) => (
  <div style={{
    position: 'absolute',
    left: 0,
    right: 0,
    bottom: 0,
    padding: '4px 16px 6px',
    background: 'currentColor',
    fontFamily: '"Silkscreen", ui-monospace, monospace',
    fontWeight: 700,
    fontSize: 14,
    letterSpacing: 2,
    textAlign: 'center',
    textTransform: 'uppercase',
    ...style,
  }}>
    <span style={{
      // invert: text reads as paper/bg over the ink banner
      color: paper,
      mixBlendMode: 'normal',
    }}>{children}</span>
  </div>
);

Object.assign(window, {
  EPD_W, EPD_H, ink, paper,
  Display, Block, Hdr, Row, Sub, Time, Badge, Arrow, Banner,
});
