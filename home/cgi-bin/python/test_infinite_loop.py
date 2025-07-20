#!/usr/bin/env python3
"""
Infinite loop test script for CGI error handling
This script intentionally creates an infinite loop to test server resilience
"""
import os
import time

# Print CGI headers first
print("Content-Type: text/html")
print()  # Empty line required

print("<html><head><title>Infinite Loop Test</title></head><body>")
print("<h1>Starting Infinite Loop Test...</h1>")
print("<p>This script will run indefinitely to test server timeout handling.</p>")

# Flush output to ensure headers are sent
import sys
sys.stdout.flush()

# Create an infinite loop
counter = 0
while True:
    counter += 1
    time.sleep(1)  # Sleep to prevent excessive CPU usage
    if counter % 10 == 0:
        print(f"<p>Loop iteration: {counter}</p>")
        sys.stdout.flush()
