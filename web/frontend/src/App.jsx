import React, { useState, useEffect, useRef } from 'react';
import SigPlot from './components/SigPlot.jsx';
import Slider from 'rc-slider';
import 'rc-slider/assets/index.css';

function App() {
  const [file, setFile] = useState('timecode.prm');
  const [availableFiles, setAvailableFiles] = useState([]);
  const [centerFreq, setCenterFreq] = useState(0);
  const [windowSize, setWindowSize] = useState(4096);
  const [smoothing, setSmoothing] = useState(8);
  const [colormap, setColormap] = useState('jet');
  const [sigplotColormap, setSigplotColormap] = useState(1);
  const [fftColor, setFftColor] = useState('#00ff00');
  
  const [zmin, setZmin] = useState(-80);
  const [zmax, setZmax] = useState(-20);
  const [gainBounds, setGainBounds] = useState([-150, 0]);
  const [gainHover, setGainHover] = useState(false);

  const [imagePlot, setImagePlot] = useState('');
  const [sigplotUrl, setSigplotUrl] = useState('');
  const [sigplotType, setSigplotType] = useState('1D');
  const [loading, setLoading] = useState(false);
  const [theme, setTheme] = useState('dark');
  const [fillMode, setFillMode] = useState('gradient');
  const [fillColor, setFillColor] = useState('#00ff00');

  const interactivePanelRef = useRef(null);
  const staticPanelRef = useRef(null);

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme(theme === 'dark' ? 'light' : 'dark');
  };

  const fetchFiles = async () => {
    try {
      const res = await fetch('/api/files');
      const data = await res.json();
      if (data.files) {
        setAvailableFiles(data.files);
        if (!data.files.includes(file) && data.files.length > 0) {
          setFile(data.files[0]);
        }
      }
    } catch (e) {
      console.error("Error fetching files", e);
    }
  };

  useEffect(() => {
    fetchFiles();
  }, []);

  const handleUpload = async (e) => {
    if (!e.target.files || e.target.files.length === 0) return;
    const uploadData = new FormData();
    uploadData.append('file', e.target.files[0]);
    
    setLoading(true);
    try {
      const res = await fetch('/api/upload', {
        method: 'POST',
        body: uploadData
      });
      if (res.ok) {
        await fetchFiles();
        setFile(e.target.files[0].name);
      } else {
        alert("Upload failed.");
      }
    } catch (err) {
      alert("Error uploading: " + err);
    }
    setLoading(false);
  };

  const getComputedFftSize = (val, ref, isInteractive = false) => {
    if (val !== 'auto' && val !== '') return Number(val);
    if (!ref.current) return isInteractive ? 4096 : 1024;
    // Nearest power of 2 for FFT size based on panel width
    let base = Math.pow(2, Math.round(Math.log2(ref.current.clientWidth)));
    return isInteractive ? base * 4 : base;
  };

  const handleStaticPlot = async (isFft) => {
    setLoading(true);
    try {
      const reqWindowSize = getComputedFftSize(windowSize, staticPanelRef, false);
      const res = await fetch('/api/run/plot', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          center_freq: Number(centerFreq),
          zoom_bw: 0,
          width: staticPanelRef.current ? staticPanelRef.current.clientWidth : 800,
          height: staticPanelRef.current ? staticPanelRef.current.clientHeight : 600,
          window_size: reqWindowSize,
          smoothing: smoothing,
          colormap: colormap,
          plot_fft: isFft,
          plot_waterfall: !isFft,
          theme: theme,
          fill_mode: fillMode,
          fill_color: fillColor
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setImagePlot(`/api/data/${data.output_file}`);
      }
    } catch (e) {
      alert("Error generating plot: " + e);
    }
    setLoading(false);
  };

  const handleInteractivePSD = async () => {
    setLoading(true);
    try {
      const reqWindowSize = getComputedFftSize(windowSize, interactivePanelRef, true);
      const res = await fetch('/api/run/psd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          center_freq: Number(centerFreq),
          zoom_bw: 0,
          window_size: reqWindowSize,
          smoothing: smoothing
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setSigplotType('2D');
        setSigplotUrl(`/api/data/${data.output_file}`);
      }
    } catch (e) {
      alert("Error generating PSD: " + e);
    }
    setLoading(false);
  };

  const handleInteractiveFFT = async () => {
    setLoading(true);
    try {
      const reqWindowSize = getComputedFftSize(windowSize, interactivePanelRef, true);
      const res = await fetch('/api/run/fft', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          center_freq: Number(centerFreq),
          zoom_bw: 0,
          window_size: reqWindowSize,
          smoothing: smoothing
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setSigplotType('1D');
        setSigplotUrl(`/api/data/${data.output_file}`);
      }
    } catch (e) {
      alert("Error generating FFT: " + e);
    }
    setLoading(false);
  };

  const handleDataLoaded = (min_val, max_val) => {
    // Round and add 20dB padding for slider bounds
    const boundsMin = Math.floor(min_val - 20);
    const boundsMax = Math.ceil(max_val + 20);
    setGainBounds([boundsMin, boundsMax]);
    // Don't auto-set zmin/zmax here, leave them empty to allow auto-scaling initially
    // but the slider will display the bounds.
  };

  return (
    <div className="container">
      <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center'}}>
        <h1>DSP Tools Interface</h1>
        <button onClick={toggleTheme} style={{width: 'auto', margin: 0}}>{theme === 'dark' ? '☀️ Light' : '🌙 Dark'}</button>
      </div>
      <div className="content">
        
        {/* Controls Panel */}
        <div className="panel" style={{flex: 1, overflowY: 'auto'}}>
          <h2 style={{marginTop: 0, fontSize: '1.2rem'}}>Controls</h2>
          <div className="form-group">
            <label>Input File</label>
            <div style={{ display: 'flex', gap: '5px' }}>
              <select value={file} onChange={(e) => setFile(e.target.value)} style={{ flex: 1 }}>
                {availableFiles.map(f => <option key={f} value={f}>{f}</option>)}
                {availableFiles.length === 0 && <option value={file}>{file}</option>}
              </select>
              <input type="file" accept=".prm,.wav,.bin" onChange={handleUpload} style={{ display: 'none' }} id="file-upload" />
              <label htmlFor="file-upload" style={{
                background: 'var(--border-color)', padding: '6px 10px', borderRadius: '4px', cursor: 'pointer', margin: 0, fontWeight: 'normal', whiteSpace: 'nowrap'
              }}>Upload</label>
            </div>
          </div>
          <div className="form-group">
            <label>Center Frequency (MHz)</label>
            <input type="text" value={centerFreq} onChange={(e) => setCenterFreq(e.target.value)} />
          </div>
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>FFT Size</label>
              <select value={windowSize} onChange={(e) => setWindowSize(e.target.value)}>
                {[5,6,7,8,9,10,11,12,13,14,15,16,17].map(i => {
                  const val = Math.pow(2, i);
                  return <option key={val} value={val}>{val}</option>;
                })}
              </select>
            </div>
            <div style={{flex: 1}}>
              <label>Smoothing {smoothing > 0 ? `(${smoothing})` : '(Off)'}</label>
              <input type="range" min="0" max="64" value={smoothing} onChange={(e) => setSmoothing(Number(e.target.value))} style={{width: '100%', accentColor: 'var(--accent-color)'}} />
            </div>
          </div>
          
          <h3 style={{marginTop: '15px', borderBottom: '1px solid var(--border-color)', paddingBottom: '3px', fontSize: '1.1rem'}}>Interactive Plot Options</h3>
          
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>Spectrum Color (1D)</label>
              <select value={fftColor} onChange={(e) => setFftColor(e.target.value)}>
                <option value="#00ff00">Green</option>
                <option value="#00ffff">Cyan</option>
                <option value="#ffff00">Yellow</option>
                <option value="#ff00ff">Magenta</option>
                <option value="#ffffff">White</option>
                <option value="#ff0000">Red</option>
              </select>
            </div>
            <div style={{flex: 1}}>
              <label>Interactive Colormap (2D)</label>
              <select value={sigplotColormap} onChange={(e) => setSigplotColormap(Number(e.target.value))}>
                <option value={10}>BuGn</option>
                <option value={4}>calewhite</option>
                <option value={8}>Cold</option>
                <option value={2}>Color Wheel</option>
                <option value={0}>Greyscale</option>
                <option value={14}>GreyNRed</option>
                <option value={7}>Hot</option>
                <option value={5}>HotDesat</option>
                <option value={1}>Ramp Colormap</option>
                <option value={3}>Spectrum</option>
                <option value={6}>Sunset</option>
                <option value={12}>YlGnBu</option>
                <option value={11}>YlOrBr</option>
                <option value={13}>YlOrRd</option>
              </select>
              <div style={{marginTop: '5px', height: '25px', borderRadius: '3px', background: 
                sigplotColormap === 0 ? 'linear-gradient(to right, #000000, #ffffff)' :
                (sigplotColormap === 1 || sigplotColormap === 3) ? 'linear-gradient(to right, #000080, #0000ff, #00ffff, #ffff00, #ff0000, #800000)' :
                sigplotColormap === 4 ? 'linear-gradient(to right, #ffffff, #000000)' :
                sigplotColormap === 5 ? 'linear-gradient(to right, #000000, #880000, #ff8800, #ffffdd, #ffffff)' :
                sigplotColormap === 6 ? 'linear-gradient(to right, #000000, #4400aa, #dd3344, #ffaa00, #ffffff)' :
                sigplotColormap === 7 ? 'linear-gradient(to right, #000000, #ff0000, #ffff00, #ffffff)' :
                sigplotColormap === 8 ? 'linear-gradient(to right, #000000, #0000ff, #00ffff, #ffffff)' :
                sigplotColormap === 10 ? 'linear-gradient(to right, #e5f5f9, #99d8c9, #2ca25f)' :
                sigplotColormap === 11 ? 'linear-gradient(to right, #fff7bc, #fec44f, #d95f0e)' :
                sigplotColormap === 12 ? 'linear-gradient(to right, #edf8b1, #7fcdbb, #2c7fb8)' :
                sigplotColormap === 13 ? 'linear-gradient(to right, #ffeda0, #feb24c, #f03b20)' :
                sigplotColormap === 14 ? 'linear-gradient(to right, #000000, #888888, #ff0000)' :
                'linear-gradient(to right, #000, #fff)'
              }}></div>
            </div>
          </div>
          
          <div style={{display: 'flex', gap: '10px'}}>
            <button onClick={handleInteractiveFFT} disabled={loading}>Spectrum (FFT)</button>
            <button onClick={handleInteractivePSD} disabled={loading}>Waterfall (PSD)</button>
          </div>
          
          {loading && <p style={{color: 'var(--accent-color)', fontWeight: 'bold', margin: '5px 0'}}>Processing...</p>}

          <h3 style={{marginTop: '15px', borderBottom: '1px solid var(--border-color)', paddingBottom: '3px', fontSize: '1.1rem'}}>Static Plot Options</h3>
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>Fill Mode</label>
              <select value={fillMode} onChange={(e) => setFillMode(e.target.value)}>
                <option value="gradient">Gradient</option>
                <option value="solid">Solid</option>
                <option value="none">None</option>
              </select>
              {fillMode !== 'gradient' ? (
                <div style={{marginTop: '5px'}}>
                  <input type="color" value={fillColor} onChange={(e) => setFillColor(e.target.value)} style={{width: '100%', height: '25px', border: 'none', padding: '0', background: 'transparent', cursor: 'pointer'}} />
                </div>
              ) : (
                <div style={{marginTop: '5px', height: '25px', borderRadius: '3px', background: 
                  colormap === 'jet' ? 'linear-gradient(to right, #00007f, #0000ff, #00ffff, #ffff00, #ff0000, #7f0000)' :
                  colormap === 'electric' ? 'linear-gradient(to right, #000000, #000064, #0000ff, #00ffff, #ffff00, #ffffff)' :
                  colormap === 'turbo' ? 'linear-gradient(to right, #30123b, #4288eb, #28fb95, #a4fc3c, #f39913, #d12907, #7a0403)' :
                  colormap === 'pablo' ? 'linear-gradient(to right, #000000, #0000ff, #00ff00, #ffff00, #ff0000, #ffffff)' :
                  colormap === 'frog' ? 'linear-gradient(to right, #000000, #005000, #00b400, #00ff00, #ffff00, #ff0000, #ffffff)' :
                  colormap === 'grape' ? 'linear-gradient(to right, #000000, #3c0078, #9600c8, #dc00ff, #ff64ff, #b4ff00, #ffffff)' :
                  colormap === 'gqrx' ? 'linear-gradient(to right, #000000, #000096, #0096ff, #ffff00, #ff0000, #ffffff)' :
                  colormap === 'websdr' ? 'linear-gradient(to right, #000000, #500096, #ff0000, #ffff00, #ffffff)' : '#888'
                }}></div>
              )}
            </div>
            <div style={{flex: 1}}>
              <label>Color Map</label>
              <select value={colormap} onChange={(e) => setColormap(e.target.value)}>
                <option value="electric">Electric</option>
                <option value="frog">Frog</option>
                <option value="gqrx">GQRX</option>
                <option value="grape">Grape</option>
                <option value="jet">Jet</option>
                <option value="pablo">Pablo</option>
                <option value="turbo">Turbo</option>
                <option value="websdr">WebSDR</option>
              </select>
              <div style={{marginTop: '5px', height: '25px', borderRadius: '3px', background: 
                colormap === 'jet' ? 'linear-gradient(to right, #00007f, #0000ff, #00ffff, #ffff00, #ff0000, #7f0000)' :
                colormap === 'electric' ? 'linear-gradient(to right, #000000, #000064, #0000ff, #00ffff, #ffff00, #ffffff)' :
                colormap === 'turbo' ? 'linear-gradient(to right, #30123b, #4288eb, #28fb95, #a4fc3c, #f39913, #d12907, #7a0403)' :
                colormap === 'pablo' ? 'linear-gradient(to right, #000000, #0000ff, #00ff00, #ffff00, #ff0000, #ffffff)' :
                colormap === 'frog' ? 'linear-gradient(to right, #000000, #005000, #00b400, #00ff00, #ffff00, #ff0000, #ffffff)' :
                colormap === 'grape' ? 'linear-gradient(to right, #000000, #3c0078, #9600c8, #dc00ff, #ff64ff, #b4ff00, #ffffff)' : 
                colormap === 'gqrx' ? 'linear-gradient(to right, #000000, #000096, #0096ff, #ffff00, #ff0000, #ffffff)' :
                colormap === 'websdr' ? 'linear-gradient(to right, #000000, #500096, #ff0000, #ffff00, #ffffff)' : '#888'
              }}></div>
            </div>
          </div>

          <div style={{display: 'flex', gap: '10px', marginTop: '5px'}}>
            <button onClick={() => handleStaticPlot(true)} disabled={loading}>Spectrum (FFT)</button>
            <button onClick={() => handleStaticPlot(false)} disabled={loading}>Waterfall (PSD)</button>
          </div>
          
        </div>

        {/* Visualization Panel */}
        <div className="plot-container" style={{flex: 2, display: 'flex', flexDirection: 'column'}}>
          
          <div className="panel" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <h2 style={{marginTop: 0, fontSize: '1.2rem'}}>Interactive SigPlot View</h2>
            <div style={{flex: 1, display: 'flex', gap: '2px'}}>
              <div ref={interactivePanelRef} style={{flex: 1, position: 'relative', minHeight: '200px'}}>
                {sigplotUrl ? (
                  <SigPlot dataUrl={sigplotUrl} type={sigplotType} zmin={zmin} zmax={zmax} theme={theme} fftColor={fftColor} sigplotColormap={sigplotColormap} onDataLoaded={handleDataLoaded} />
                ) : (
                  <p style={{margin: 0}}>Select an interactive mode to load sigplot.</p>
                )}
              </div>
              <div style={{display: 'flex', alignItems: 'center', justifyContent: 'center', width: '12px'}}>
                <div style={{fontSize: '0.8rem', writingMode: 'vertical-rl', transform: 'rotate(180deg)', color: '#ffffff', fontWeight: 'bold'}}>Gain (dB)</div>
              </div>
              <div style={{width: '25px', display: 'flex', flexDirection: 'column', alignItems: 'center', position: 'relative', margin: '30px 0'}} 
                   onMouseEnter={() => setGainHover(true)} 
                   onMouseLeave={() => setGainHover(false)}>
                {gainHover && (
                  <div style={{position: 'absolute', top: '-25px', left: '50%', transform: 'translateX(-50%)', background: 'var(--border-color)', padding: '2px 4px', borderRadius: '4px', fontSize: '0.7rem', whiteSpace: 'nowrap', zIndex: 50}}>
                    {zmax === '' ? 'Auto' : zmax}
                  </div>
                )}
                <div style={{flex: 1, margin: '0', width: '100%', display: 'flex', justifyContent: 'center'}}>
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
                      { width: 16, height: 16, borderRadius: 4, marginLeft: -4, backgroundColor: '#ffd700', borderColor: '#ffd700', zIndex: 100, cursor: 'ns-resize', opacity: 1, boxShadow: 'none' },
                      { width: 16, height: 16, borderRadius: 4, marginLeft: -4, backgroundColor: '#ffd700', borderColor: '#ffd700', zIndex: 100, cursor: 'ns-resize', opacity: 1, boxShadow: 'none' }
                    ]}
                    activeHandleStyle={[
                      { backgroundColor: '#ffd700', borderColor: '#ffd700', boxShadow: 'none', width: 16, height: 16, borderRadius: 4, marginLeft: -4, zIndex: 100 },
                      { backgroundColor: '#ffd700', borderColor: '#ffd700', boxShadow: 'none', width: 16, height: 16, borderRadius: 4, marginLeft: -4, zIndex: 100 }
                    ]}
                    trackStyle={[{ backgroundColor: 'var(--accent-color)', width: 8, zIndex: 1 }]}
                    railStyle={{ backgroundColor: 'var(--border-color)', width: 8, zIndex: 1 }}
                  />
                </div>
                {gainHover && (
                  <div style={{position: 'absolute', bottom: '-25px', left: '50%', transform: 'translateX(-50%)', background: 'var(--border-color)', padding: '2px 4px', borderRadius: '4px', fontSize: '0.7rem', whiteSpace: 'nowrap', zIndex: 50}}>
                    {zmin === '' ? 'Auto' : zmin}
                  </div>
                )}
              </div>
            </div>
          </div>

          <div className="panel" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <h2 style={{marginTop: 0, fontSize: '1.2rem'}}>Static Plot Output</h2>
            <div ref={staticPanelRef} style={{flex: 1, position: 'relative', minHeight: '200px', overflow: 'hidden'}}>
              {imagePlot ? (
                <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0}}>
                  <img src={imagePlot} alt="DSP Plotter Output" style={{width: '100%', height: '100%', objectFit: 'fill', border: '1px solid var(--border-color)', borderRadius: '4px', boxSizing: 'border-box'}} />
                </div>
              ) : (
                <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                  <p style={{margin: 0}}>Select static plot to view image.</p>
                </div>
              )}
            </div>
          </div>

        </div>

      </div>
    </div>
  );
}

export default App;
