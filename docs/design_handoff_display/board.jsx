// bustaferl — Display + Sonderscreens
// Invertiertes Bahnsteig-Board (vormals H2) plus alle Zustandsvarianten.
// Strikt 1-bit B/W (kein Greyscale, kein Antialiasing).

// ──────────────────────────────────────────────────────────────────────
// DATA — Standard-Datensatz. Jede Zeit kann live oder plan sein.
// `liveTimes: [true,false]` markiert pro Zeitslot, ob Echtzeit
// vorliegt. Default = live. Plan-Zeiten werden mit einem kleinen
// Hohlquadrat □ nach der Zeit markiert.
// ──────────────────────────────────────────────────────────────────────
const DEFAULT_DATA = {
  tg: [
    { line: '58A', dir: 'Atzgers.', times: ['18:32', '18:48'], liveTimes: [true, false] },
    { line: '58A', dir: 'Hietzing', times: ['18:35', '18:50'], liveTimes: [true, false] },
  ],
  eg: { line: '58B', dir: 'Atzgers.', times: ['18:41', '19:01'], liveTimes: [true, true] },
  sb: [
    { line: 'S2', t: '18:37', live: true },
    { line: 'S3', t: '18:51', live: true },
    { line: 'S4', t: '19:05', live: false },
  ],
};

// Hilfs-Komponente: ein Zeit-Paar mit kontrollierbarem Abstand.
const Times = ({ a, b, gap = 20 }) => (
  <span style={{ display: 'inline-flex', gap, alignItems: 'baseline' }}>
    <span>{a}</span><span>{b}</span>
  </span>
);

// Plan-Indikator: kleines Hohlquadrat nach einer Plan-Zeit.
// Live-Zeiten bekommen keinen Marker (cleaner default).
const PlanMark = () => (
  <span style={{
    display: 'inline-block',
    width: 5, height: 5,
    border: '1px solid currentColor',
    marginLeft: 3, verticalAlign: 'middle',
  }} />
);

// Zeit + optionaler Plan-Indikator.
// Wenn die Zeit selbst "leer" ist (z.B. '--:--'), wird auch kein
// Marker gerendert — es gibt ja keine Zeit zu kennzeichnen.
const T = ({ t, live = true }) => {
  const empty = !t || /^-+:?-*$/.test(t);
  return (
    <span style={{ whiteSpace: 'nowrap' }}>
      {t}{!live && !empty && <PlanMark />}
    </span>
  );
};

// ──────────────────────────────────────────────────────────────────────
// NetworkPlan — zweizeiliger Netzplan mit Atzg als gemeinsamem Knoten
// Hbf ── Atzg
//          │
//       (▼ über Tull)
//        Atzg ── Ende ── Tull ── Hietz
// ──────────────────────────────────────────────────────────────────────
const NetworkPlan = () => {
  const cols = 5;
  const labels = ['Hbf', 'Atzg', 'Ende', 'Tull', 'Hietz'];
  const here = 3;
  const transferCol = 1;
  const grid = { display: 'grid', gridTemplateColumns: `repeat(${cols}, 1fr)`, alignItems: 'center' };
  const Dot = () => <div style={{ width: 4, height: 4, background: 'currentColor' }} />;
  const Big = () => <div style={{ width: 8, height: 8, background: 'currentColor' }} />;
  const Diamond = () => <div style={{ width: 7, height: 7, background: 'currentColor', transform: 'rotate(45deg)' }} />;
  const Cell = ({ children }) => (
    <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: 10 }}>{children}</div>
  );
  const MarkerRow = ({ from, to, markers }) => {
    const leftPct = (from + 0.5) * (100 / cols);
    const widthPct = (to - from) * (100 / cols);
    return (
      <div style={{ position: 'relative', height: 10 }}>
        <div style={{
          position: 'absolute', left: `${leftPct}%`, width: `${widthPct}%`,
          top: '50%', height: 1, background: 'currentColor',
          transform: 'translateY(-0.5px)',
        }} />
        <div style={{ ...grid, position: 'relative', height: '100%' }}>
          {Array.from({ length: cols }).map((_, i) => (
            <Cell key={i}>{markers[i] || null}</Cell>
          ))}
        </div>
      </div>
    );
  };
  const transferLeftPct = (transferCol + 0.5) * (100 / cols);
  return (
    <div style={{ marginTop: 'auto', padding: '4px 6px 0', position: 'relative' }}>
      <MarkerRow from={0} to={transferCol} markers={[<Dot/>, <Diamond/>, null, null, null]} />
      <div style={{ position: 'relative' }}>
        <div style={{
          position: 'absolute', left: `${transferLeftPct}%`,
          top: 0, bottom: 0, width: 1, background: 'currentColor',
          transform: 'translateX(-0.5px)',
        }} />
        <div style={{ ...grid, marginTop: 2, position: 'relative' }}>
          <Cell/><Cell/><Cell/>
          <Cell>
            <span style={{
              fontFamily: '"VT323", ui-monospace, monospace',
              fontSize: 10, lineHeight: '10px',
            }}>▼</span>
          </Cell>
          <Cell/>
        </div>
      </div>
      <MarkerRow from={transferCol} to={cols - 1} markers={[null, <Diamond/>, <Dot/>, <Big/>, <Dot/>]} />
      <div style={{
        ...grid,
        fontFamily: '"Silkscreen", ui-monospace, monospace',
        fontSize: 7, letterSpacing: 0.5,
        textTransform: 'uppercase', textAlign: 'center',
        marginTop: 4,
      }}>
        {labels.map((s, i) => (
          <div key={i} style={{ fontWeight: i === here || i === transferCol ? 700 : 400 }}>{s}</div>
        ))}
      </div>
    </div>
  );
};

// ──────────────────────────────────────────────────────────────────────
// Banner — invertierter Status-Streifen, paper auf ink (oder umgekehrt
// im invertierten Display: ink-auf-paper). Wird über die ganze Breite
// gerendert und sticht aus dem normalen Layout heraus.
// ──────────────────────────────────────────────────────────────────────
const StatusBanner = ({ children }) => (
  <div style={{
    background: '#f4f1e8',   // paper
    color: '#0d0d0d',         // ink
    margin: '0 -18px',
    padding: '2px 18px',
    fontFamily: '"Silkscreen", ui-monospace, monospace',
    fontSize: 10, letterSpacing: 1,
    textTransform: 'uppercase',
    textAlign: 'center',
  }}>
    {children}
  </div>
);

// ──────────────────────────────────────────────────────────────────────
// Header-Helper
// ──────────────────────────────────────────────────────────────────────
const SectionHead = ({ children, size = 12 }) => (
  <Hdr rule={false} style={{ fontSize: size, marginBottom: 6 }}>{children}</Hdr>
);

// Zeit-Render — entweder live (clean) oder plan (mit □).
const RenderTime = ({ t, live = true }) => <T t={t} live={live} />;

// ──────────────────────────────────────────────────────────────────────
// Board — Hauptlayout, parametrisierbar via `data` und `notice`
// notice = { kind: 'stale' | 'delay' | 'cancel' | …, text: '…' }
// Wird als Banner oben oder als Ersatz-Inhalt gerendert.
// ──────────────────────────────────────────────────────────────────────
const Board = ({ data = DEFAULT_DATA, notice = null }) => {
  const ih = true;

  // TG-Reihe rendert ein Array von Einträgen
  const renderTG = (entries) => (
    <div style={{
      fontFamily: '"VT323", ui-monospace, monospace',
      fontSize: 28, lineHeight: '32px',
      display: 'grid', gridTemplateColumns: 'auto 1fr auto',
      columnGap: 12, alignItems: 'center',
    }}>
      {entries.flatMap((e, i) => [
        <Badge key={`b${i}`} invertHost={ih} size="lg">{e.line}</Badge>,
        <span key={`d${i}`}>{e.dir}</span>,
        <span key={`t${i}`} style={{ display: 'inline-flex', gap: 20, alignItems: 'baseline' }}>
          <T t={e.times[0]} live={e.liveTimes?.[0] !== false} />
          <T t={e.times[1]} live={e.liveTimes?.[1] !== false} />
        </span>,
      ])}
    </div>
  );

  return (
    <div style={{ position: 'relative', height: 300, display: 'flex', flexDirection: 'column' }}>
      {notice && (
        <div style={{
          background: '#f4f1e8',   // paper
          color: '#0d0d0d',         // ink
          padding: '3px 18px',
          fontFamily: '"Silkscreen", ui-monospace, monospace',
          fontSize: 10, letterSpacing: 1,
          textTransform: 'uppercase',
          textAlign: 'center',
        }}>
          {notice.text}
        </div>
      )}
      <div style={{ padding: '8px 18px 8px', borderBottom: '2px solid currentColor' }}>
        <SectionHead>Tullnertalgasse</SectionHead>
        {renderTG(data.tg)}
      </div>
      <div style={{ padding: '8px 18px 8px', borderBottom: '1px solid currentColor' }}>
        <SectionHead size={10}>Endemanngasse · nach Schleife</SectionHead>
        <div style={{
          fontFamily: '"VT323", ui-monospace, monospace',
          fontSize: 22, lineHeight: '24px',
          display: 'grid', gridTemplateColumns: 'auto 1fr auto',
          columnGap: 12, alignItems: 'center',
        }}>
          <Badge invertHost={ih} size="md">{data.eg.line}</Badge>
          <span>{data.eg.dir}</span>
          <span style={{ display: 'inline-flex', gap: 14, alignItems: 'baseline' }}>
            <T t={data.eg.times[0]} live={data.eg.liveTimes?.[0] !== false} />
            <T t={data.eg.times[1]} live={data.eg.liveTimes?.[1] !== false} />
          </span>
        </div>
      </div>
      <div style={{ padding: '8px 18px 12px', flex: 1, display: 'flex', flexDirection: 'column' }}>
        <SectionHead size={10}>Atzgersdorf <Arrow/> Wien Hbf</SectionHead>
        {data.sbNotice ? (
          <div style={{
            fontFamily: '"VT323", ui-monospace, monospace',
            fontSize: 18, lineHeight: '20px',
          }}>{data.sbNotice}</div>
        ) : (
          <div style={{
            fontFamily: '"VT323", ui-monospace, monospace',
            fontSize: 20, lineHeight: '24px',
            display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)',
            alignItems: 'baseline', columnGap: 6,
          }}>
            {data.sb.map((d, i) => (
              <div key={i} style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <Badge invertHost={ih} size="sm">{d.line}</Badge>
                <T t={d.t} live={d.live !== false} />
              </div>
            ))}
          </div>
        )}
        <NetworkPlan />
      </div>
    </div>
  );
};

// ──────────────────────────────────────────────────────────────────────
// FullscreenError — ersetzt die gesamte Anzeige durch ein zentriertes
// Glyph + Text. Für Offline, Auth-Fehler, Boot.
// ──────────────────────────────────────────────────────────────────────
const FullscreenError = ({ glyph, title, sub, foot }) => (
  <div style={{
    position: 'relative',
    height: 300, display: 'flex', flexDirection: 'column',
    alignItems: 'center', justifyContent: 'center',
    fontFamily: '"Silkscreen", ui-monospace, monospace',
    textAlign: 'center', padding: '0 24px',
  }}>
    <div style={{
      fontFamily: '"VT323", ui-monospace, monospace',
      fontSize: 90, lineHeight: '90px', marginBottom: 8,
    }}>{glyph}</div>
    <div style={{ fontSize: 18, letterSpacing: 2, fontWeight: 700, marginBottom: 6 }}>{title}</div>
    {sub && <div style={{
      fontFamily: '"VT323", ui-monospace, monospace',
      fontSize: 16, lineHeight: '18px', marginTop: 4, maxWidth: 320,
    }}>{sub}</div>}
    {foot && <div style={{
      position: 'absolute', bottom: 8, left: 0, right: 0,
      fontSize: 8, letterSpacing: 1, opacity: 1,
    }}>{foot}</div>}
  </div>
);

// ──────────────────────────────────────────────────────────────────────
// Konkrete Varianten
// ──────────────────────────────────────────────────────────────────────
const BoardNormal = () => <Board />;

// 1. Stale — keine Echtzeit, alle Zeiten als '--:--'.
const BoardStale = () => {
  const dashed = ['--:--', '--:--'];
  const data = {
    tg: DEFAULT_DATA.tg.map(e => ({ ...e, times: dashed, liveTimes: [false, false] })),
    eg: { ...DEFAULT_DATA.eg, times: dashed, liveTimes: [false, false] },
    sb: DEFAULT_DATA.sb.map(d => ({ ...d, t: '--:--', live: false })),
  };
  return <Board data={data} />;
};

// 2. Nachtbetrieb — Nachts (keine S-Bahn, kein Bus mehr aktiv),
// zeigt die ersten Abfahrten des Tages. Alle Zeiten = Plan.
const BoardNight = () => {
  const data = {
    tg: [
      { line: '58A', dir: 'Atzgers.', times: ['04:23', '04:38'], liveTimes: [false, false] },
      { line: '58A', dir: 'Hietzing', times: ['04:31', '04:46'], liveTimes: [false, false] },
    ],
    eg: { line: '58B', dir: 'Atzgers.', times: ['04:38', '04:53'], liveTimes: [false, false] },
    sb: [
      { line: 'S2', t: '04:43', live: false },
      { line: 'S3', t: '04:58', live: false },
      { line: 'S4', t: '05:13', live: false },
    ],
  };
  return <Board data={data} />;
};

// 3. Funk weg / Offline — kein Netz seit X
const BoardOffline = () => (
  <FullscreenError
    glyph="!"
    title="Kein Empfang"
    sub="Letzte Aktualisierung 17:48"
    foot="WLAN · Retry in 30s"
  />
);

// 4. Auth-Fehler — § 9 AID/Client veraltet
const BoardAuth = () => (
  <FullscreenError
    glyph="§9"
    title="Auth-Fehler"
    sub="Client-ID veraltet · bitte neu registrieren"
    foot="AID 0x8F · ERR 401"
  />
);

// 5. Boot — initialisiert
const BoardBoot = () => (
  <FullscreenError
    glyph="◌"
    title="bustaferl"
    sub="lädt Fahrplan…"
    foot="v2.0 · UC8176 · 400×300"
  />
);

// 6. Alles ruhig — keine Abfahrten in den nächsten 20 min
const BoardQuiet = () => {
  const data = {
    tg: [],
    eg: null,
    sb: [],
  };
  // ein vereinfachtes Layout direkt rendern (Board braucht tg/eg)
  return (
    <div style={{
      height: 300, display: 'flex', flexDirection: 'column',
      alignItems: 'center', justifyContent: 'center', padding: '0 24px',
      fontFamily: '"Silkscreen", ui-monospace, monospace',
      textAlign: 'center',
    }}>
      <div style={{
        fontFamily: '"VT323", ui-monospace, monospace',
        fontSize: 72, lineHeight: '72px', marginBottom: 8,
      }}>—</div>
      <div style={{ fontSize: 14, letterSpacing: 2, fontWeight: 700 }}>Keine Abfahrten</div>
      <div style={{
        fontFamily: '"VT323", ui-monospace, monospace',
        fontSize: 18, marginTop: 6,
      }}>in den nächsten 20 min</div>
    </div>
  );
};

Object.assign(window, {
  Board, BoardNormal, BoardStale,
  BoardNight, BoardOffline, BoardAuth, BoardBoot, BoardQuiet,
});
