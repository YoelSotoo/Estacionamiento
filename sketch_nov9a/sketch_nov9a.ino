/*
 ESP32: servidor estático + WebSocket relay + control 2 servos
 - Sirve archivos desde SPIFFS (Estacionamiento.html, camera_scan.html, styles.css...)
 - HTTP endpoints:
     GET /entrar  -> abre (y luego cierra) servo ENTRADA
     GET /salir   -> abre (y luego cierra) servo SALIDA
 - WebSocket relay en el puerto 81: cualquier cliente puede enviar JSON y el ESP32 lo retransmite a todos
 - Nota: cámaras y getUserMedia pueden requerir HTTPS en algunos navegadores móviles; probar Firefox/Android o alternativas si falla.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <ESPmDNS.h>

///////// CONFIGURA AQUI /////////
const char* WIFI_SSID = "Familia Soto Obeso";
const char* WIFI_PASS = "11sherlyn02";

const int ENTRADA_SERVO_PIN = 13; // ajustar
const int SALIDA_SERVO_PIN  = 12; // ajustar

const int CLOSED_ANGLE = 0;
const int OPEN_ANGLE   = 80;
const unsigned long OPEN_MS = 1600;

const String SECRET_TOKEN = ""; // si pones token, usa ?token=...
//////////////////////////////////

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

Servo entradaServo;
Servo salidaServo;

volatile bool entradaMoving = false;
volatile bool salidaMoving = false;

String contentType(const String &path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".jpg")) return "image/jpeg";
  if (path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".json")) return "application/json";
  return "text/plain";
}

void setCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

bool checkAuth() {
  if (SECRET_TOKEN.length() == 0) return true;
  if (server.hasArg("token")) return server.arg("token") == SECRET_TOKEN;
  return false;
}

bool handleFileRead(String path) {
  if (path == "/") path = "/Estacionamiento.html";
  String filePath = path;
  if (SPIFFS.exists(filePath)) {
    File f = SPIFFS.open(filePath, "r");
    server.streamFile(f, contentType(filePath));
    f.close();
    return true;
  }
  // try without leading slash
  if (filePath.startsWith("/")) {
    String alt = filePath.substring(1);
    if (SPIFFS.exists(alt)) {
      File f = SPIFFS.open(alt, "r");
      server.streamFile(f, contentType(alt));
      f.close();
      return true;
    }
  }
  return false;
}

// HTTP handlers
void handleRoot() {
  setCors();
  if (!handleFileRead("/")) {
    server.send(404, "text/plain", "Not found");
  }
}

void handleNotFound() {
  setCors();
  if (!handleFileRead(server.uri())) {
    server.send(404, "text/plain", "Not found: " + server.uri());
  }
}

void moveThenCloseTask(void *pvParameters) {
  int which = (int)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(OPEN_MS));
  if (which == 0) {
    entradaServo.write(CLOSED_ANGLE);
    entradaMoving = false;
    Serial.println("Entrada: cerrado");
  } else {
    salidaServo.write(CLOSED_ANGLE);
    salidaMoving = false;
    Serial.println("Salida: cerrado");
  }
  vTaskDelete(NULL);
}

void handleEntrar() {
  Serial.println("GET /entrar");
  setCors();
  if (!checkAuth()) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"invalid_token\"}");
    return;
  }
  if (entradaMoving) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
    return;
  }
  entradaMoving = true;
  entradaServo.write(OPEN_ANGLE);
  xTaskCreate(moveThenCloseTask, "closeEntrada", 2048, (void*)0, 1, NULL);
  server.send(200, "application/json", "{\"ok\":true,\"action\":\"entrar\"}");
}

void handleSalir() {
  Serial.println("GET /salir");
  setCors();
  if (!checkAuth()) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"invalid_token\"}");
    return;
  }
  if (salidaMoving) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
    return;
  }
  salidaMoving = true;
  salidaServo.write(OPEN_ANGLE);
  xTaskCreate(moveThenCloseTask, "closeSalida", 2048, (void*)1, 1, NULL);
  server.send(200, "application/json", "{\"ok\":true,\"action\":\"salir\"}");
}

// WebSocket event: cuando llega un mensaje desde un cliente, lo rebroadcast a todos
void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("WebSocket client %u conectado\n", num);
  } else if (type == WStype_DISCONNECTED) {
    Serial.printf("WebSocket client %u desconectado\n", num);
  } else if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    Serial.printf("WS mensaje de %u: %s\n", num, msg.c_str());
    // reenviar a todos
    webSocket.broadcastTXT(msg);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    while (1) delay(1000);
  }
  Serial.println("SPIFFS montado.");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 30000) break;
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("estacionamiento")) {
    Serial.println("mDNS: estacionamiento.local");
  }

  // init servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  entradaServo.attach(ENTRADA_SERVO_PIN);
  salidaServo.attach(SALIDA_SERVO_PIN);
  entradaServo.write(CLOSED_ANGLE);
  salidaServo.write(CLOSED_ANGLE);

  // HTTP routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/entrar", HTTP_GET, handleEntrar);
  server.on("/salir", HTTP_GET, handleSalir);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  // WebSocket server
  webSocket.begin();
  webSocket.onEvent(onWsEvent);
  Serial.println("WebSocket relay running on port 81");
}

void loop() {
  server.handleClient();
  webSocket.loop();
}