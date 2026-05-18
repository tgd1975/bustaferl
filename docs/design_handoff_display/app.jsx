// bustaferl — Display + Sonderscreens
// Canvas mit Hauptlayout + allen Sonderzuständen.

const SCALE = 1.6;
const ARTBOARD_W = EPD_W * SCALE + 28;
const ARTBOARD_H = EPD_H * SCALE + 28 + 24;

const Stage = ({ label, sublabel, children }) => (
  <div style={{
    width: '100%', height: '100%',
    display: 'flex', alignItems: 'center', justifyContent: 'center',
    background: '#f0eee9',
  }}>
    <Display scale={SCALE} invert={true} label={label} sublabel={sublabel}>
      {children}
    </Display>
  </div>
);

const App = () => (
  <DesignCanvas>
    <DCSection
      id="main"
      title="Hauptanzeige"
      subtitle="Regulärer Betrieb · TG · EG · Atzgersdorf S-Bahn">
      <DCArtboard id="normal" label="Normal" width={ARTBOARD_W} height={ARTBOARD_H}>
        <Stage label="Normal" sublabel="alle Streams live"><BoardNormal /></Stage>
      </DCArtboard>
    </DCSection>

    <DCSection
      id="states"
      title="Datenzustände"
      subtitle="Live vs. Plan: □ markiert Fahrplan-Zeiten. Live = ohne Marker.">
      <DCArtboard id="stale" label="Veraltet" width={ARTBOARD_W} height={ARTBOARD_H}>
        <Stage label="Veraltet" sublabel="§4 — API > 3 min stumm"><BoardStale /></Stage>
      </DCArtboard>
      <DCArtboard id="quiet" label="Keine Abfahrten" width={ARTBOARD_W} height={ARTBOARD_H}>
        <Stage label="Keine Abfahrten" sublabel="Lücke > 20 min"><BoardQuiet /></Stage>
      </DCArtboard>
    </DCSection>

    <DCSection
      id="modes"
      title="Betriebsmodi"
      subtitle="Nachtbetrieb · Initialisierung">
      <DCArtboard id="night" label="Nachtbetrieb" width={ARTBOARD_W} height={ARTBOARD_H}>
        <Stage label="Nachtbetrieb" sublabel="erste Abfahrten morgens, alles Plan"><BoardNight /></Stage>
      </DCArtboard>
      <DCArtboard id="boot" label="Boot" width={ARTBOARD_W} height={ARTBOARD_H}>
        <Stage label="Boot" sublabel="erster Render nach Reset"><BoardBoot /></Stage>
      </DCArtboard>
    </DCSection>

    <DCSection
      id="errors"
      title="Fehlerbilder"
      subtitle="Vollbild — Symbol + kurze Erklärung + Diagnose-Foot">
      <DCArtboard id="offline" label="Kein Empfang" width={ARTBOARD_W} height={ARTBOARD_H}>
        <Stage label="Kein Empfang" sublabel="WLAN/MQTT weg, letzte Update-Zeit"><BoardOffline /></Stage>
      </DCArtboard>
      <DCArtboard id="auth" label="Auth-Fehler" width={ARTBOARD_W} height={ARTBOARD_H}>
        <Stage label="Auth-Fehler" sublabel="§9 — AID/Client veraltet"><BoardAuth /></Stage>
      </DCArtboard>
    </DCSection>

    <DCPostIt id="design-note" x={40} y={40} w={300}>
      <strong>bustaferl · Display + Sonderscreens</strong><br/>
      Echtgrößen-Mockup (400 × 300 px), 1.6× gerendert.<br/>
      Strikt 1-bit B/W — keine Opazität, keine Graustufen.<br/>
      Banner oben (invertiert) bei Datenstatus, Vollbild-Glyph
      bei Fehlern, Plan-Zeiten bei Nachtbetrieb.
    </DCPostIt>
  </DesignCanvas>
);

ReactDOM.createRoot(document.getElementById('root')).render(<App />);
