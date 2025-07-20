#!/usr/bin/python3

import sys
import os

print("Content-Type: text/html")
print("")

print("<html><body>")
print("<h1>POST Test</h1>")

# Print environment variables
print("<h2>Environment Variables:</h2>")
print(f"REQUEST_METHOD: {os.environ.get('REQUEST_METHOD', 'Not set')}<br>")
print(f"CONTENT_LENGTH: {os.environ.get('CONTENT_LENGTH', 'Not set')}<br>")
print(f"CONTENT_TYPE: {os.environ.get('CONTENT_TYPE', 'Not set')}<br>")

# Read POST data from stdin
post_data = ""
if os.environ.get('REQUEST_METHOD') == 'POST':
    content_length = os.environ.get('CONTENT_LENGTH')
    if content_length and content_length.isdigit():
        post_data = sys.stdin.read(int(content_length))

print("<h2>POST Data:</h2>")
print(f"<pre>{post_data}</pre>")
print("</body></html>")
