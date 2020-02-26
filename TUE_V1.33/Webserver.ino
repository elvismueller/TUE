//Webserver
ESP8266WebServer server(80);

void setupWebServer()
{
  // start web server
  server.on("/", handleRoot);
  server.on("/inline", []() { server.send(200, "text/plain", "this works as well"); } );
  server.on("/time", HTTP_POST, handleTime);
  server.on("/stop", HTTP_POST, handleStop);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loopWebServer()
{
  // called in main loop to handle web server
  server.handleClient();  
}

void handleRoot()
{
  String page;
  page = "<p>TUE Version "; 
  page += VERSION;
  page += "<br>hostname = ";
  page += "TUE";
  page += esp_chipid;
  page += "<br>last received time = ";
  page += c_clock_hour;
  page += " : ";
  page += c_clock_minute;
  page += "<br>tochteruhr time = ";
  page += tochter_h;
  page += " : ";
  page += tochter_m;
  page += "</p>"; 
  page += "<p>Setze die angezeigte Uhrzeit im Format hh : mm:</p>";
  page += "<form action=\"/time\" method=\"POST\"><input type=\"text\" name=\"hour\" placeholder=\"00\"> : <input type=\"text\" name=\"minute\" placeholder=\"00\"></br><input type=\"submit\" value=\"Send\"></form>";
  page += "<p>vorlaufen der Uhr stoppen:</p>";
  page += "<form action=\"/stop\" method=\"POST\"><input type=\"submit\" value=\"Stop\"></form>";
  server.send(200, "text/html", page);
}

void handleTime()
{ // If a POST request is made to URI /login
  if(!server.hasArg("hour") || !server.hasArg("minute") || server.arg("hour") == NULL || server.arg("minute") == NULL)
  { // If the POST request doesn't have username and password data
    // The request is invalid, so send HTTP status 400
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  tochter_h = atoi(server.arg("hour").c_str());
  tochter_m = atoi(server.arg("minute").c_str());
  Serial.print("Got new time via website: ");
  Serial.print(tochter_h);
  Serial.print(":");
  Serial.print(tochter_m);
  Serial.println("!");
  // Add a header to respond with a new location for the browser to go to the home page again
  server.sendHeader("Location","/");
  // Send it back to the browser
  server.send(303);
}

void handleStop()
{ // sync times
  syncInternalTimeWithTochteruhr();
  // Add a header to respond with a new location for the browser to go to the home page again
  server.sendHeader("Location","/");
  // Send it back to the browser
  server.send(303);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
