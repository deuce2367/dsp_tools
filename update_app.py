import re

with open("web/frontend/src/App.jsx", "r") as f:
    content = f.read()

# 1. Add panels and layoutMode state
content = re.sub(
    r"const \[sigplotType, setSigplotType\] = useState\('1D'\);",
    r"const [sigplotType, setSigplotType] = useState('1D');\n  const [panels, setPanels] = useState([\n    { id: 'interactive-fft', type: 'interactive', subType: '1D', title: 'Interactive Spectrum (FFT)', url: '' },\n    { id: 'static-waterfall', type: 'static', subType: 'waterfall', title: 'Static Waterfall (PSD)', url: '' }\n  ]);\n  const [layoutMode, setLayoutMode] = useState('auto');\n",
    content
)

# 2. Update layout toggle in header
content = re.sub(
    r"<div style=\{\{display: 'flex', gap: '15px', alignItems: 'center'\}\}>",
    r"<div style={{display: 'flex', gap: '15px', alignItems: 'center'}}>\n          <select value={layoutMode} onChange={e => setLayoutMode(e.target.value)} style={{width: 'auto', margin: 0}}>\n            <option value=\"auto\">Layout: Auto</option>\n            <option value=\"horizontal\">Layout: Horizontal</option>\n            <option value=\"vertical\">Layout: Vertical</option>\n          </select>",
    content
)

# 3. Update handleInteractivePSD
new_psd = """const handleInteractivePSD = async (overrideFreq = null, targetFile = null) => {
    setLoading(true);
    try {
      const activeFile = targetFile || file;
      const reqWindowSize = getComputedFftSize(windowSize, {current: {clientWidth: 800}}, true);
      const res = await fetch('/api/run/psd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: activeFile,
          center_freq: (overrideFreq !== null && typeof overrideFreq !== 'object') ? Number(overrideFreq) : Number(centerFreq),
          zoom_bw: 0,
          window_size: reqWindowSize,
          smoothing: smoothing
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setSigplotType('2D');
        const newUrl = `/api/data/${data.output_file}?t=${Date.now()}`;
        setPanels(prev => {
            const existingIdx = prev.findIndex(p => p.id === 'interactive-psd');
            if (existingIdx >= 0) {
                const next = [...prev];
                next[existingIdx] = { ...next[existingIdx], url: newUrl };
                return next;
            } else {
                return [...prev, { id: 'interactive-psd', type: 'interactive', subType: '2D', title: 'Interactive Waterfall (PSD)', url: newUrl }];
            }
        });
      }
    } catch (e) {
      alert("Error generating PSD: " + e);
    }
    setLoading(false);
  };"""

content = re.sub(
    r"const handleInteractivePSD = async[\s\S]*?setLoading\(false\);\n  };",
    new_psd,
    content
)

# 4. Update handleInteractiveFFT
new_fft = """const handleInteractiveFFT = async (overrideFreq = null, targetFile = null) => {
    setLoading(true);
    try {
      const activeFile = targetFile || file;
      const reqWindowSize = getComputedFftSize(windowSize, {current: {clientWidth: 800}}, true);
      const res = await fetch('/api/run/fft', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: activeFile,
          center_freq: (overrideFreq !== null && typeof overrideFreq !== 'object') ? Number(overrideFreq) : Number(centerFreq),
          zoom_bw: 0,
          window_size: reqWindowSize,
          smoothing: smoothing
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setSigplotType('1D');
        const newUrl = `/api/data/${data.output_file}?t=${Date.now()}`;
        setPanels(prev => {
            const existingIdx = prev.findIndex(p => p.id === 'interactive-fft');
            if (existingIdx >= 0) {
                const next = [...prev];
                next[existingIdx] = { ...next[existingIdx], url: newUrl };
                return next;
            } else {
                return [...prev, { id: 'interactive-fft', type: 'interactive', subType: '1D', title: 'Interactive Spectrum (FFT)', url: newUrl }];
            }
        });
      }
    } catch (e) {
      alert("Error generating FFT: " + e);
    }
    setLoading(false);
  };"""

content = re.sub(
    r"const handleInteractiveFFT = async[\s\S]*?setLoading\(false\);\n  };",
    new_fft,
    content
)

# 5. Update handleStaticPlot
new_static = """const handleStaticPlot = async (isFft, overrideFreq = null) => {
    setLoading(true);
    try {
      const reqWindowSize = getComputedFftSize(windowSize, {current: {clientWidth: 800}}, false);
      const res = await fetch('/api/run/plot', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          center_freq: overrideFreq !== null ? Number(overrideFreq) : Number(centerFreq),
          zoom_bw: 0,
          width: 800,
          height: 600,
          window_size: reqWindowSize,
          smoothing: smoothing,
          colormap: colormap,
          plot_fft: isFft,
          plot_waterfall: !isFft,
          theme: theme,
          fill_mode: fillMode,
          fill_color: fillColor,
          zmin: zmin !== '' ? Number(zmin) : null,
          zmax: zmax !== '' ? Number(zmax) : null
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        const newUrl = `/api/data/${data.output_file}?t=${Date.now()}`;
        const pId = isFft ? 'static-fft' : 'static-waterfall';
        const pTitle = isFft ? 'Static Spectrum (FFT)' : 'Static Waterfall (PSD)';
        setPanels(prev => {
            const existingIdx = prev.findIndex(p => p.id === pId);
            if (existingIdx >= 0) {
                const next = [...prev];
                next[existingIdx] = { ...next[existingIdx], url: newUrl };
                return next;
            } else {
                return [...prev, { id: pId, type: 'static', subType: isFft ? 'fft' : 'waterfall', title: pTitle, url: newUrl }];
            }
        });
      }
    } catch (e) {
      alert("Error generating plot: " + e);
    }
    setLoading(false);
  };"""

content = re.sub(
    r"const handleStaticPlot = async[\s\S]*?setLoading\(false\);\n  };",
    new_static,
    content
)


# 6. Update rendering loop
rendering_code = """
        {/* Visualization Panel */}
        <div className="plot-container" style={{
            flex: 3, 
            display: 'grid', 
            gap: '10px',
            gridTemplateColumns: panels.length === 1 ? '1fr' : 
                                 panels.length === 2 ? (layoutMode === 'horizontal' || layoutMode === 'auto' ? '1fr 1fr' : '1fr') :
                                 panels.length === 3 ? (layoutMode === 'vertical' ? '1fr' : '1fr 1fr 1fr') :
                                 panels.length === 4 ? '1fr 1fr' :
                                 panels.length <= 6 ? '1fr 1fr 1fr' :
                                 '1fr 1fr 1fr 1fr',
            gridTemplateRows: panels.length === 1 ? '1fr' :
                              panels.length === 2 ? (layoutMode === 'horizontal' || layoutMode === 'auto' ? '1fr' : '1fr 1fr') :
                              panels.length === 3 ? (layoutMode === 'vertical' ? '1fr 1fr 1fr' : '1fr') :
                              panels.length <= 6 ? '1fr 1fr' :
                              '1fr 1fr 1fr',
        }}>
          {panels.map(panel => (
            <div key={panel.id} className="panel" style={{ flex: 1, display: 'flex', flexDirection: 'column', position: 'relative' }}>
              <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '5px'}}>
                  <h2 style={{marginTop: 0, marginBottom: 0, fontSize: '1.1rem'}}>{panel.title}</h2>
                  <button onClick={() => setPanels(prev => prev.filter(p => p.id !== panel.id))} style={{width: '24px', height: '24px', padding: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', borderRadius: '50%', background: '#ff4444', margin: 0, flexShrink: 0}} title="Close Panel">
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>
                  </button>
              </div>

              {panel.type === 'interactive' ? (
                  <div style={{flex: 1, display: 'flex', gap: '2px'}}>
                    <div style={{flex: 1, position: 'relative', minHeight: '200px'}}>
                      {loading && panel.url === '' && <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.5)', zIndex: 10, display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#fff', fontSize: '1.2rem'}}>Processing...</div>}
                      {panel.url ? (
                        <SigPlot dataUrl={panel.url} type={panel.subType} zmin={zmin} zmax={zmax} theme={theme} fftColor={fftColor} sigplotColormap={sigplotColormap} onDataLoaded={handleDataLoaded} onZoom={setZoomBounds} />
                      ) : (
                        <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                            <p style={{margin: 0}}>Select an interactive mode to load sigplot.</p>
                        </div>
                      )}
                    </div>
                    <div style={{display: 'flex', alignItems: 'center', justifyContent: 'center', width: '15px', marginLeft: '5px'}}>
                      <div style={{fontSize: '0.8rem', writingMode: 'vertical-rl', transform: 'rotate(180deg)', color: 'var(--text-color)', fontWeight: 'bold'}}>Gain (dB)</div>
                    </div>

                    <div style={{width: '35px', display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '5px 0'}}>
                      <div style={{fontSize: '0.7rem', fontWeight: 'bold', marginBottom: '8px'}}>{zmax === '' ? 'Auto' : zmax}</div>
                      <div style={{flex: 1, width: '100%', display: 'flex', justifyContent: 'center'}}>
                        <Slider 
                          vertical 
                          range 
                          min={gainBounds[0]} 
                          max={gainBounds[1]} 
                          value={[
                            zmin === '' ? gainBounds[0] + 10 : Number(zmin), 
                            zmax === '' ? gainBounds[1] - 10 : Number(zmax)
                          ]} 
                          onChange={(val) => { setZmin(val[0]); setZmax(val[1]); }}
                          handleStyle={[
                            { width: 16, height: 16, borderRadius: 2, left: 5, backgroundColor: '#ffd700', borderColor: '#ffd700', zIndex: 100, cursor: 'ns-resize', opacity: 1, boxShadow: 'none' },
                            { width: 16, height: 16, borderRadius: 2, left: 5, backgroundColor: '#ffd700', borderColor: '#ffd700', zIndex: 100, cursor: 'ns-resize', opacity: 1, boxShadow: 'none' }
                          ]}
                          activeHandleStyle={[
                            { backgroundColor: '#ffd700', borderColor: '#ffd700', boxShadow: 'none', width: 16, height: 16, borderRadius: 2, left: 5, zIndex: 100 },
                            { backgroundColor: '#ffd700', borderColor: '#ffd700', boxShadow: 'none', width: 16, height: 16, borderRadius: 2, left: 5, zIndex: 100 }
                          ]}
                          trackStyle={[{ backgroundColor: 'var(--accent-color)' }]}
                          railStyle={{ backgroundColor: 'var(--border-color)' }}
                        />
                      </div>
                      <div style={{fontSize: '0.7rem', fontWeight: 'bold', marginTop: '8px'}}>{zmin === '' ? 'Auto' : zmin}</div>
                    </div>
                  </div>
              ) : (
                  <div style={{flex: 1, position: 'relative', minHeight: '200px', overflow: 'hidden'}}>
                    {panel.url ? (
                      <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0}}>
                        <img src={panel.url} alt="DSP Plotter Output" style={{width: '100%', height: '100%', objectFit: 'fill', border: '1px solid var(--border-color)', borderRadius: '4px', boxSizing: 'border-box'}} />
                      </div>
                    ) : (
                      <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                        <p style={{margin: 0}}>Select static plot to view image.</p>
                      </div>
                    )}
                  </div>
              )}
            </div>
          ))}
          {panels.length === 0 && (
             <div style={{display: 'flex', alignItems: 'center', justifyContent: 'center', flex: 1, background: 'var(--panel-bg)', borderRadius: '8px', border: '1px dashed var(--border-color)'}}>
                 <h2 style={{opacity: 0.5}}>No panels open. Select an option from Controls.</h2>
             </div>
          )}
        </div>
"""

content = re.sub(
    r"\{\/\* Visualization Panel \*\/\}[\s\S]*?<\/div>\s*<\/div>\s*\{\/\* Raw File Modal \*\/\}",
    rendering_code + "\n      </div>\n      \n      {/* Raw File Modal */}",
    content
)


with open("web/frontend/src/App.jsx", "w") as f:
    f.write(content)
