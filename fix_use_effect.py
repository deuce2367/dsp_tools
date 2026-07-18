import re

with open("web/frontend/src/App.jsx", "r") as f:
    content = f.read()

# Fix the theme useEffect to use the panels state
new_use_effect = """useEffect(() => {
    panels.forEach(p => {
        if (p.type === 'static') {
            handleStaticPlot(p.subType === 'fft');
        }
    });
  }, [theme]);"""

content = re.sub(
    r"useEffect\(\(\) => \{\s*if \(imagePlot && staticPlotType\) \{\s*handleStaticPlot\(staticPlotType === 'fft'\);\s*\}\s*\}, \[theme\]\);",
    new_use_effect,
    content
)

with open("web/frontend/src/App.jsx", "w") as f:
    f.write(content)
