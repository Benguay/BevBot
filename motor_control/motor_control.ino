#include <WiFi.h>
#include <Stepper.h>

// ------------------------- WIFI CONFIG -------------------------
const char* ssid = "FBI Surveillance Van";
const char* password = "Idonotknow!!!";

WiFiServer server(80);
String header;

// ------------------------- STEPPER CONFIG -------------------------
#define FRONT_RIGHT_1  34
#define FRONT_RIGHT_2  35
#define FRONT_RIGHT_3  36
#define FRONT_RIGHT_4  37

#define FRONT_LEFT_1   10
#define FRONT_LEFT_2   9
#define FRONT_LEFT_3   8
#define FRONT_LEFT_4   7

#define BACK_RIGHT_1   20
#define BACK_RIGHT_2   21
#define BACK_RIGHT_3   22
#define BACK_RIGHT_4   23

#define BACK_LEFT_1    18
#define BACK_LEFT_2    17
#define BACK_LEFT_3    16
#define BACK_LEFT_4    15

const int stepsPerRevolution = 2048;

Stepper motorFrontRight(stepsPerRevolution, FRONT_RIGHT_1, FRONT_RIGHT_3, FRONT_RIGHT_2, FRONT_RIGHT_4);
Stepper motorFrontLeft (stepsPerRevolution, FRONT_LEFT_1, FRONT_LEFT_3, FRONT_LEFT_2, FRONT_LEFT_4);
Stepper motorBackRight (stepsPerRevolution, BACK_RIGHT_1, BACK_RIGHT_3, BACK_RIGHT_2, BACK_RIGHT_4);
Stepper motorBackLeft  (stepsPerRevolution, BACK_LEFT_1, BACK_LEFT_3, BACK_LEFT_2, BACK_LEFT_4);

// calibration
float stepsPerCm = 81.48733;
float stepsPerDegree = 17.77778;

// ------------------------- ADC CUTOFF -------------------------
const int adcPin = 34;
const int voltageThreshold = 100;
bool motorRunning = true;

// ------------------------- STORED VARIABLES -------------------------
String linearDir = "";
int linearDist = 0;

String strafeDir = "";
int strafeDist = 0;

String turnDir = "";
int turnDeg = 0;

// ------------------------- MOVEMENT FUNCTIONS -------------------------
void moveLinear(float distanceCm) {
  if (!motorRunning) return;
  int steps = distanceCm * stepsPerCm;

  motorFrontRight.step(steps);
  motorFrontLeft.step(steps);
  motorBackRight.step(steps);
  motorBackLeft.step(steps);
}

void strafe(float distanceCm) {
  if (!motorRunning) return;
  int steps = distanceCm * stepsPerCm;

  motorFrontRight.step(steps);
  motorFrontLeft.step(-steps);
  motorBackRight.step(-steps);
  motorBackLeft.step(steps);
}

void rotate(float theta) {
  if (!motorRunning) return;
  int steps = theta * stepsPerDegree;

  motorFrontRight.step(steps);
  motorFrontLeft.step(steps);
  motorBackRight.step(-steps);
  motorBackLeft.step(-steps);
}

bool checkADC() {
  int adcValue = analogRead(adcPin);
  Serial.println(adcValue);

  if (adcValue >= voltageThreshold) {
    motorRunning = false;
    Serial.println("MOTORS DISABLED (ADC TRIP)");
    return false;
  }

  motorRunning = true;
  return true;
}

// ------------------------- WEB PARAM PARSER -------------------------
String getValue(String data, String key) {
  int start = data.indexOf(key);
  if (start == -1) return "";
  start += key.length();
  int end = data.indexOf("&", start);
  if (end == -1) end = data.indexOf(" ", start);
  return data.substring(start, end);
}

// ------------------------- SETUP -------------------------
void setup() {
  Serial.begin(115200);
  pinMode(adcPin, INPUT);

  motorFrontRight.setSpeed(8);
  motorFrontLeft.setSpeed(8);
  motorBackRight.setSpeed(8);
  motorBackLeft.setSpeed(8);

  Serial.println("Connecting to WiFi…");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }

  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());
  server.begin();
}

// ------------------------- LOOP -------------------------
void loop() {
  checkADC();

  WiFiClient client = server.available();
  if (!client) return;

  String currentLine = "";
  header = "";

  while (client.connected()) {
    if (!client.available()) continue;

    char c = client.read();
    header += c;
    if (c != '\n') continue;

    if (currentLine.length() == 0) {

      // ================= LINEAR COMMAND =================
      if (header.indexOf("GET /linear") >= 0) {
        linearDir = getValue(header, "dir=");
        linearDist = getValue(header, "dist=").toInt();

        float cm = (linearDir == "forward") ? linearDist : -linearDist;
        Serial.println("=== LINEAR MOVE ===");
        Serial.println(linearDir + " " + String(linearDist) + "cm");

        moveLinear(cm);
      }

      // ================= STRAFE COMMAND =================
      if (header.indexOf("GET /strafe") >= 0) {
        strafeDir = getValue(header, "dir=");
        strafeDist = getValue(header, "dist=").toInt();

        float cm = (strafeDir == "right") ? strafeDist : -strafeDist;
        Serial.println("=== STRAFE MOVE ===");
        Serial.println(strafeDir + " " + String(strafeDist) + "cm");

        strafe(cm);
      }

      // ================= ROTATION COMMAND ===============
      if (header.indexOf("GET /turn") >= 0) {
        turnDir = getValue(header, "dir=");
        turnDeg = getValue(header, "deg=").toInt();

        float deg = (turnDir == "right") ? turnDeg : -turnDeg;
        Serial.println("=== ROTATION ===");
        Serial.println(turnDir + " " + String(turnDeg) + " degrees");

        rotate(deg);
      }

      // ================= SEND WEBPAGE ==================
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/html\n");
      client.println("<html><body style='font-family:Arial;text-align:center;'>");

      client.println("<h1>Mecanum Robot Control</h1><hr>");

      // LINEAR FORM
      client.println("<h2>Linear Move</h2>");
      client.println("<form action='/linear'>");
      client.println("<select name='dir'>"
                     "<option value='forward'>Forward</option>"
                     "<option value='backward'>Backward</option>"
                     "</select><br><br>");
      client.println("Distance (cm): <input name='dist' type='number'><br><br>");
      client.println("<button type='submit'>MOVE</button></form><hr>");

      // STRAFE FORM
      client.println("<h2>Strafe Move</h2>");
      client.println("<form action='/strafe'>");
      client.println("<select name='dir'>"
                     "<option value='right'>Right</option>"
                     "<option value='left'>Left</option>"
                     "</select><br><br>");
      client.println("Distance (cm): <input name='dist' type='number'><br><br>");
      client.println("<button type='submit'>STRAFE</button></form><hr>");

      // TURN FORM
      client.println("<h2>Rotate</h2>");
      client.println("<form action='/turn'>");
      client.println("<select name='dir'>"
                     "<option value='right'>Right</option>"
                     "<option value='left'>Left</option>"
                     "</select><br><br>");
      client.println("Degrees: <input name='deg' type='number'><br><br>");
      client.println("<button type='submit'>ROTATE</button></form>");

      client.println("</body></html>");
      break;
    }

    currentLine = "";
  }

  client.stop();
  Serial.println("Client disconnected.\n");
}
