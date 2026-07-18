import re

with open("web/frontend/src/App.jsx", "r") as f:
    content = f.read()

# Fix literal newlines inside alert strings
content = content.replace('alert("Error from server:\n" +', 'alert("Error from server:\\n" +')

with open("web/frontend/src/App.jsx", "w") as f:
    f.write(content)
