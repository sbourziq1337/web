#!/usr/bin/env python3
# filepath: /home/aamir/Downloads/webserver_last/webserver/cgi-bin/python/decoder.cgi

print("Content-Type: text/html\r\n\r\n")
print("""<!DOCTYPE html>
<html>
<head>
    <title>Base64 Encoder/Decoder</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            max-width: 600px; 
            margin: 20px auto; 
            padding: 20px; 
            background: #f5f5f5;
        }
        .container { 
            background: white; 
            padding: 30px; 
            border-radius: 10px; 
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 { text-align: center; color: #333; margin-bottom: 30px; }
        .section { 
            margin: 25px 0; 
            padding: 20px; 
            border: 2px solid #e9ecef; 
            border-radius: 8px; 
            background: #f8f9fa;
        }
        input { 
            width: 100%; 
            padding: 12px; 
            margin: 10px 0; 
            border: 1px solid #ddd; 
            border-radius: 6px; 
            font-size: 14px;
            box-sizing: border-box;
        }
        button { 
            background: #007bff; 
            color: white; 
            padding: 12px 24px; 
            border: none; 
            border-radius: 6px; 
            cursor: pointer; 
            font-size: 14px;
            width: 100%;
            margin-top: 10px;
        }
        button:hover { background: #0056b3; }
        .output { 
            background: #e9ecef; 
            padding: 15px; 
            border-radius: 6px; 
            margin-top: 15px; 
            min-height: 40px; 
            word-break: break-all;
            font-family: monospace;
        }
        h3 { color: #495057; margin: 0 0 15px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1> Base64 Encoder/Decoder</h1>
        
        <div class="section">
            <h3> Encode Text</h3>
            <input type="text" id="encodeInput" placeholder="Enter text to encode">
            <button onclick="encode()">Encode to Base64</button>
            <div id="encodeOutput" class="output">Encoded text will appear here...</div>
        </div>
        
        <div class="section">
            <h3> Decode Text</h3>
            <input type="text" id="decodeInput" placeholder="Enter base64 text to decode">
            <button onclick="decode()">Decode from Base64</button>
            <div id="decodeOutput" class="output">Decoded text will appear here...</div>
        </div>
    </div>

    <script>
        function encode() {
            const text = document.getElementById('encodeInput').value;
            const output = document.getElementById('encodeOutput');
            
            if (!text.trim()) {
                output.innerHTML = ' Please enter text to encode';
                return;
            }
            
            try {
                const encoded = btoa(text);
                output.innerHTML = `<strong> Encoded:</strong><br>${encoded}`;
            } catch (error) {
                output.innerHTML = ` Error: ${error.message}`;
            }
        }
        
        function decode() {
            const text = document.getElementById('decodeInput').value;
            const output = document.getElementById('decodeOutput');
            
            if (!text.trim()) {
                output.innerHTML = ' Please enter base64 text to decode';
                return;
            }
            
            try {
                const decoded = atob(text);
                output.innerHTML = `<strong> Decoded:</strong><br>${decoded}`;
            } catch (error) {
                output.innerHTML = ' Error: Invalid base64 input';
            }
        }

        // Allow Enter key to trigger encoding/decoding
        document.getElementById('encodeInput').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') encode();
        });
        
        document.getElementById('decodeInput').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') decode();
        });
    </script>
</body>
</html>""")