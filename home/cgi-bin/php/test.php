<?php
#!/usr/bin/php
// filepath: /home/aamir/Downloads/webserver_last/webserver/cgi-bin/hello.php
header("Content-Type: text/html");

echo "<html>\n";
echo "<head><title>PHP CGI Test</title></head>\n";
echo "<body>\n";
echo "<h1>Hello from PHP CGI!</h1>\n";
echo "<p>Current time: " . date('Y-m-d H:i:s') . "</p>\n";
// echo "<p>Server: " . $_SERVER['SERVER_NAME'] . "</p>\n";
// echo "<p>Request Method: " . $_SERVER['REQUEST_METHOD'] . "</p>\n";      
echo "</body>\n";
echo "</html>\n";
?>