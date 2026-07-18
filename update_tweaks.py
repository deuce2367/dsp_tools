import re

with open("web/frontend/src/App.jsx", "r") as f:
    content = f.read()

# 1. Initial panels state
content = re.sub(
    r"const \[panels, setPanels\] = useState\(\[\s*\{ id: 'interactive-fft', type: 'interactive', subType: '1D', title: 'Interactive Spectrum \(FFT\)', url: '' \},\s*\{ id: 'static-waterfall', type: 'static', subType: 'waterfall', title: 'Static Waterfall \(PSD\)', url: '' \}\s*\]\);",
    r"const [panels, setPanels] = useState([\n    { id: 'static-waterfall', type: 'static', subType: 'waterfall', title: 'Static Waterfall (PSD)', url: '' }\n  ]);",
    content
)

# 2. Add targetFile to handleStaticPlot
content = re.sub(
    r"const handleStaticPlot = async \(isFft, overrideFreq = null\) => \{\n    setLoading\(true\);\n    try \{\n      const reqWindowSize = getComputedFftSize\(windowSize, \{current: \{clientWidth: 800\}\}, false\);\n      const res = await fetch\('/api/run/plot', \{\n        method: 'POST',\n        headers: \{ 'Content-Type': 'application/json' \},\n        body: JSON\.stringify\(\{\n          input_file: file,",
    r"const handleStaticPlot = async (isFft, overrideFreq = null, targetFile = null) => {\n    setLoading(true);\n    try {\n      const activeFile = targetFile || file;\n      const reqWindowSize = getComputedFftSize(windowSize, {current: {clientWidth: 800}}, false);\n      const res = await fetch('/api/run/plot', {\n        method: 'POST',\n        headers: { 'Content-Type': 'application/json' },\n        body: JSON.stringify({\n          input_file: activeFile,",
    content
)

# 3. Update startTuner setTimeout
new_tuner_timeout = """setTimeout(() => {
          setPanels(prev => {
              prev.forEach(p => {
                  if (p.id === 'interactive-fft') handleInteractiveFFT(tunerParams.center, tunerOutName);
                  if (p.id === 'interactive-psd') handleInteractivePSD(tunerParams.center, tunerOutName);
                  if (p.id === 'static-fft') handleStaticPlot(true, tunerParams.center, tunerOutName);
                  if (p.id === 'static-waterfall') handleStaticPlot(false, tunerParams.center, tunerOutName);
              });
              return prev;
          });
        }, 300);"""

content = re.sub(
    r"setTimeout\(\(\) => \{\s*setPanels\(prev => \{\s*prev\.forEach\(p => \{\s*if \(p\.id === 'interactive-fft'\) handleInteractiveFFT\(tunerParams\.center, tunerOutName\);\s*if \(p\.id === 'interactive-psd'\) handleInteractivePSD\(tunerParams\.center, tunerOutName\);\s*\}\);\s*return prev;\s*\}\);\s*\}, 300\);",
    new_tuner_timeout,
    content
)

# 4. Plot titles font size and close button style
content = re.sub(
    r"<h2 style=\{\{marginTop: 0, marginBottom: 0, fontSize: '1.1rem'\}\}>\{panel.title\}<\/h2>\s*<button onClick=\{\(\) => setPanels\(prev => prev.filter\(p => p.id !== panel.id\)\)\} style=\{\{width: '24px', height: '24px', padding: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', borderRadius: '50%', background: '#ff4444', margin: 0, flexShrink: 0\}\} title=\"Close Panel\">\s*<svg width=\"14\" height=\"14\"",
    r"""<h2 style={{marginTop: 0, marginBottom: 0, fontSize: '0.9rem'}}>{panel.title}</h2>
                  <button onClick={() => setPanels(prev => prev.filter(p => p.id !== panel.id))} style={{width: '20px', height: '20px', padding: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', borderRadius: '4px', background: 'var(--accent-color)', color: '#000', margin: 0, flexShrink: 0}} title="Close Panel">
                      <svg width="12" height="12" """,
    content
)

with open("web/frontend/src/App.jsx", "w") as f:
    f.write(content)

