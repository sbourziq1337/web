#!/usr/bin/env python3
import cgi
import sys

print("Content-Type: text/plain\n")  # ❗️VERY important newline

form = cgi.FieldStorage()

# For debugging
print("DEBUG: keys ->", form.keys())

data = form.getvalue("data")
if data:
    print("Data received:", data)
else:
    print("No data provided.")
