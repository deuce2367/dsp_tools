import re

with open("web/frontend/src/App.jsx", "r") as f:
    content = f.read()

content = content.replace('\\"', '"')

with open("web/frontend/src/App.jsx", "w") as f:
    f.write(content)
