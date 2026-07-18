import re

with open("web/frontend/src/App.jsx", "r") as f:
    content = f.read()

# Fix the tuner reload to use the panels state
new_reload = """setTimeout(() => {
          setPanels(prev => {
              prev.forEach(p => {
                  if (p.id === 'interactive-fft') handleInteractiveFFT(tunerParams.center, tunerOutName);
                  if (p.id === 'interactive-psd') handleInteractivePSD(tunerParams.center, tunerOutName);
              });
              return prev;
          });
        }, 300);"""

content = re.sub(
    r"setTimeout\(\(\) => \{\s*if \(sigplotType === '1D'\) handleInteractiveFFT\(tunerParams\.center, tunerOutName\);\s*else if \(sigplotType === '2D'\) handleInteractivePSD\(tunerParams\.center, tunerOutName\);\s*\}, 300\);",
    new_reload,
    content
)

with open("web/frontend/src/App.jsx", "w") as f:
    f.write(content)
