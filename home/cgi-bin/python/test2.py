#!/usr/bin/env python3

import cgi
import cgitb
cgitb.enable()  # Enable traceback for debugging

print("Content-Type: text/plain\r\n")

form = cgi.FieldStorage()

print("form = ",form)
for key in form.keys():
    print(f"{key} -> {form.getvalue(key)}")

