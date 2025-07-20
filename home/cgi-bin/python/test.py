#!/usr/bin/env python3
import cgi
import os

# Folder where uploaded files will be stored
UPLOAD_DIR = "/tmp/uploads"

# Create directory if not exists
os.makedirs(UPLOAD_DIR, exist_ok=True)

print("Content-Type: text/html")
print("")  # Empty line to separate headers from body

form = cgi.FieldStorage()

if "file" not in form:
    print("<h1>Error: No file part</h1>")
else:
    fileitem = form["file"]

    if fileitem.filename:
        filename = os.path.basename(fileitem.filename)
        filepath = os.path.join(UPLOAD_DIR, filename)

        with open(filepath, 'wb') as f:
            f.write(fileitem.file.read())

        print(f"<h1>Upload successful</h1><p>File saved as: {filepath}</p>")
    else:
        print("<h1>Error: Empty filename</h1>")
