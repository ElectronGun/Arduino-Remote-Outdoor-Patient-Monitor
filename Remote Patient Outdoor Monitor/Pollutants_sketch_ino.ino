#include <Arduino.h>
#include "WiFiS3.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

const char *ssid = "BELL254";
const char *password = "E39425DCDA12";

const int MQ135_PIN = A0;        // Air Quality Sensor
const int UV_SENSOR_PIN = A1;    // UV Sensor
const int BODY_TEMP_PIN = A2;    // Body Temperature Sensor
const int BUZZER_PIN = 9;        // Buzzer Pin
const float BASELINE_VALUE = 62.0;

WiFiServer server(80);

ArduinoLEDMatrix matrix;

float nh3_total = 0, nox_total = 0, co_total = 0, smoke_total = 0, co2_total = 0;
int nh3_count = 0, nox_count = 0, co_count = 0, smoke_count = 0, co2_count = 0;
unsigned long previousMillis = 0;
const int DISPLAY_INTERVAL = 3;

float uvIndex = 0;
float bodyTemp = 0;

// TLV-TWA exposure limits for a 8hr work day
float nh3_threshold = 25;
float nox_threshold = 5;
float co_threshold = 50;
float smoke_threshold = 100;
float co2_threshold = 5000;
float uv_threshold = 7;
float temp_threshold = 38.0;

String alertMessage = "<span style='color: green;'>Normal Atmosphere</span>";

// Function to get status color
String getStatusColor(float value, float threshold) {
  return value <= threshold ? "green" : "red";
}

// Function to get alert class
String getAlertClass(String alertMessage) {
  if (alertMessage.indexOf("Normal Atmosphere") != -1) {
    return "status";
  } else {
    return "alert";
  }
}

// Function to reduce thresholds
void reduceThresholds() {
  nh3_threshold *= 0.75;
  nox_threshold *= 0.75;
  co_threshold *= 0.75;
  smoke_threshold *= 0.75;
  co2_threshold *= 0.75;
  uv_threshold = 6;
  temp_threshold = 37.5;
}

// Function to reset thresholds
void resetThresholds() {
  nh3_threshold = 25;
  nox_threshold = 5;
  co_threshold = 50;
  smoke_threshold = 100;
  co2_threshold = 5000;
  uv_threshold = 7;
  temp_threshold = 38.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize the LED matrix
  matrix.begin();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("HTTP server started.");
}

void readAndUpdateSensorData() {
  int sensorValue = analogRead(MQ135_PIN);
  float ratio = (float)sensorValue / 1024;
  float Rs = (5.0 - ratio * 5.0) / (ratio * 5.0);

  float nh3_concentration = pow(Rs / BASELINE_VALUE, -1.67) * 1.53;
  nh3_total += nh3_concentration;
  nh3_count++;

  float nox_concentration = pow(Rs / BASELINE_VALUE, -1.5) * 0.72;
  nox_total += nox_concentration;
  nox_count++;

  float co_concentration = pow(Rs / BASELINE_VALUE, -1.41) * 4.52;
  co_total += co_concentration;
  co_count++;

  float smoke_concentration = pow(Rs / BASELINE_VALUE, -1.43) * 3.4;
  smoke_total += smoke_concentration;
  smoke_count++;

  float co2_concentration = pow(Rs / BASELINE_VALUE, -1.00) * 850;
  co2_total += co2_concentration;
  co2_count++;

  int uvSensorValue = analogRead(UV_SENSOR_PIN);
  float uvVoltage = uvSensorValue * 5000.0 / 1023.0;
  if (uvVoltage < 50) {
    uvIndex = 0;
  } else if (uvVoltage <= 227) {
    uvIndex = 1;
  } else if (uvVoltage <= 318) {
    uvIndex = 2;
  } else if (uvVoltage <= 408) {
    uvIndex = 3;
  } else if (uvVoltage <= 503) {
    uvIndex = 4;
  } else if (uvVoltage <= 606) {
    uvIndex = 5;
  } else if (uvVoltage <= 696) {
    uvIndex = 6;
  } else if (uvVoltage <= 795) {
    uvIndex = 7;
  } else if (uvVoltage <= 881) {
    uvIndex = 8;
  } else if (uvVoltage <= 976) {
    uvIndex = 9;
  } else if (uvVoltage <= 1079) {
    uvIndex = 10;
  } else {
    uvIndex = 11;
  }

  int tempValue = analogRead(BODY_TEMP_PIN);
  bodyTemp = (tempValue / 1024.0) * 500;
}

void updateLEDMatrix(float nh3_avg, float nox_avg, float co_avg, float smoke_avg, float co2_avg, float uvIndex, float bodyTemp) {
  matrix.clear();
  
  // Check if any value exceeds the thresholds
  bool exceedsThreshold = nh3_avg > nh3_threshold || nox_avg > nox_threshold || co_avg > co_threshold ||
                          smoke_avg > smoke_threshold || co2_avg > co2_threshold || uvIndex > uv_threshold ||
                          bodyTemp > temp_threshold;

  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(50);
  matrix.textFont(Font_5x7);
  
  if (exceedsThreshold) {
    matrix.beginText(0, 1, 0xFFFFFF);
    matrix.println("!!!");
  } else {
    matrix.beginText(0, 1, 0xFFFFFF);
    matrix.println("OK!");
  }

  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

void loop() {
  unsigned long currentMillis = millis();

  readAndUpdateSensorData();

  if (currentMillis - previousMillis >= DISPLAY_INTERVAL * 1000) {
    float nh3_avg = nh3_total / nh3_count;
    float nox_avg = nox_total / nox_count;
    float co_avg = co_total / co_count;
    float smoke_avg = smoke_total / smoke_count;
    float co2_avg = co2_total / co2_count;

    nh3_total = 0;
    nox_total = 0;
    co_total = 0;
    smoke_total = 0;
    co2_total = 0;
    nh3_count = 0;
    nox_count = 0;
    co_count = 0;
    smoke_count = 0;
    co2_count = 0;

    float nh3_aqi = (nh3_avg / 35.0) * 100.0;
    float nox_aqi = (nox_avg / 2.3) * 100.0;
    float co_aqi = (co_avg / 75.0) * 100.0;
    float smoke_aqi = (smoke_avg / 100.0) * 100.0;
    float co2_aqi = (co2_avg / 1000.0) * 100.0;
    float overall_aqi = (nh3_aqi + nox_aqi + co_aqi + smoke_aqi + co2_aqi) / 5.0;

    String airQualityStatus = "";
    String uvStatus = "";
    String bodyTempStatus = "";

    // Update air quality status
    if (overall_aqi <= 50) {
      airQualityStatus = "<span style='color: green;'>Air quality is excellent. Breathe easy!</span>";
    } else if (overall_aqi <= 100) {
      airQualityStatus = "<span style='color: green;'>Air quality is good. Enjoy the fresh air!</span>";
    } else if (overall_aqi <= 150) {
      airQualityStatus = "<span style='color: green;'>Air quality is moderate. Sensitive individuals should take care.</span>";
    } else if (overall_aqi <= 200) {
      airQualityStatus = "<span style='color: red;'>Air quality is poor. Consider staying indoors.</span>";
    } else {
      airQualityStatus = "<span style='color: red;'>Air quality is hazardous! Avoid going outside and use protective measures.</span>";
    }

    // UV status logic
    if (uvIndex <= 2) {
      uvStatus = "<span style='color: green;'>UV levels are low. No protection needed.</span>";
    } else if (uvIndex <= 5) {
      uvStatus = "<span style='color: green;'>Moderate UV levels. Consider sunscreen.</span>";
    } else if (uvIndex <= uv_threshold) {
      uvStatus = "<span style='color: red;'>High UV levels. Apply sunscreen and wear sunglasses.</span>";
    } else if (uvIndex <= 10) {
      uvStatus = "<span style='color: red;'>Very high UV levels. Avoid prolonged sun exposure.</span>";
    } else {
      uvStatus = "<span style='color: red;'>Extreme UV levels! Stay indoors or seek shade.</span>";
    }

    // Body temperature status logic
    if (bodyTemp < temp_threshold) {
      bodyTempStatus = "<span style='color: green;'>Body temperature is normal. Keep going!</span>";
    } else if (bodyTemp < (temp_threshold + 0.5)) {
      bodyTempStatus = "<span style='color: red;'>Body temperature is slightly elevated. Stay hydrated.</span>";
    } else if (bodyTemp < (temp_threshold + 1.0)) {
      bodyTempStatus = "<span style='color: red;'>High body temperature. Rest and cool down.</span>";
    } else {
      bodyTempStatus = "<span style='color: red;'>Very high body temperature! Seek medical attention.</span>";
    }

    WiFiClient client = server.available();
    if (client) {
      Serial.println("New Client.");
      String request = client.readStringUntil('\r');
      client.flush();

      if (request.indexOf("GET /data") >= 0) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println();
        client.print("{");
        client.print("\"nh3\": " + String(nh3_avg) + ", ");
        client.print("\"nox\": " + String(nox_avg) + ", ");
        client.print("\"co\": " + String(co_avg) + ", ");
        client.print("\"smoke\": " + String(smoke_avg) + ", ");
        client.print("\"co2\": " + String(co2_avg) + ", ");
        client.print("\"overall_aqi\": " + String(overall_aqi) + ", ");
        client.print("\"uv_index\": " + String(uvIndex) + ", ");
        client.print("\"body_temp\": " + String(bodyTemp) + ", ");
        client.print("\"airQualityStatus\": \"" + airQualityStatus + "\", ");
        client.print("\"uvStatus\": \"" + uvStatus + "\", ");
        client.print("\"bodyTempStatus\": \"" + bodyTempStatus + "\"");
        client.println("}");
        client.stop();
        return;
      }

      // Handle thresholds reduction
      if (request.indexOf("GET /reduce") >= 0) {
        reduceThresholds();
      }

      // Handle thresholds reset
      if (request.indexOf("GET /reset") >= 0) {
        resetThresholds();
      }

      // Handle buzzer activation
      if (request.indexOf("GET /buzz") >= 0) {
        tone(BUZZER_PIN, 1000); // Turn on the buzzer
        delay(1000);            // Buzzer sound duration
        noTone(BUZZER_PIN);     // Turn off the buzzer
      }

      // Handle buzzer deactivation
      if (request.indexOf("GET /stopbuzz") >= 0) {
        noTone(BUZZER_PIN); // Turn off the buzzer
      }

      // Display web page with updated values
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<head>");
      client.println("<meta charset=\"UTF-8\">"); // Added character encoding
      client.println("<title>Personal Outdoor Health Monitor</title>");
      client.println("<style>");
      client.println("body { font-family: Arial, sans-serif; background-color: #f0f8ff; color: #333333; padding: 20px; }");
      client.println("h1 { color: #0073e6; margin-bottom: 20px; text-align: center; font-size: 32px; }"); // Larger font size for main title
      client.println("h2 { color: #0073e6; margin-bottom: 15px; text-align: center; font-size: 24px; }"); // Larger font size for section titles
      client.println(".bold { font-weight: bold; }");
      client.println(".air-quality, .uv-monitor, .body-temperature { margin-top: 20px; padding: 20px; background-color: #e0f7fa; border-radius: 10px; border: 1px solid #0073e6; }");
      client.println(".status { color: green; font-size: 14px; text-align: left; }"); // Smaller font size and left alignment for status text
      client.println(".alert { color: red; font-size: 14px; text-align: left; }"); // Smaller font size and left alignment for alert text
      client.println(".aqi { font-size: 12px; }"); // Smaller font size for overall AQI text
      client.println("table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }");
      client.println("th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }");
      client.println("th { background-color: #0073e6; color: white; vertical-align: middle; }");
      client.println("th, td:first-child { width: 20%; }");
      client.println("th:nth-child(2), td:nth-child(2) { width: 30%; }");
      client.println("th:nth-child(3), td:nth-child(3) { width: 50%; }");
      client.println(".footer { text-align: center; margin-top: 40px; font-size: 12px; color: #666666; }");
      client.println(".button-section { margin-top: 20px; }"); // Adding space between button and body temperature section
      client.println("</style>");
      client.println("<script>");

      client.println("function fetchData() {");
      client.println("  fetch('/data').then(response => response.json()).then(data => {");
      client.println("    document.getElementById('nh3').innerText = data.nh3;");
      client.println("    document.getElementById('nox').innerText = data.nox;");
      client.println("    document.getElementById('co').innerText = data.co;");
      client.println("    document.getElementById('smoke').innerText = data.smoke;");
      client.println("    document.getElementById('co2').innerText = data.co2;");
      client.println("    document.getElementById('overall_aqi').innerText = data.overall_aqi;");
      client.println("    document.getElementById('uv_index').innerText = data.uv_index;");
            client.println("    document.getElementById('body_temp').innerText = data.body_temp + ' °C';"); // Add degree symbol here
      client.println("    document.getElementById('airQualityStatus').innerHTML = data.airQualityStatus;");
      client.println("    document.getElementById('uvStatus').innerHTML = data.uvStatus;");
      client.println("    document.getElementById('bodyTempStatus').innerHTML = data.bodyTempStatus;");
      client.println("  });");
      client.println("}");
      client.println("setInterval(fetchData, 5000);"); // Fetch data every 5 seconds
      client.println("window.onload = fetchData;");
      client.println("</script>");
      client.println("</head>");
      client.println("<body>");
      client.println("<h1>Personal Outdoor Health Monitor</h1>");

      client.println("<div class='air-quality'>");
      client.println("<h2>Air Quality</h2>");
      client.println("<table>");
      client.println("<tr><th>Pollutant</th><th>Concentration (ppm)</th><th>Alert Threshold</th></tr>");
      client.println("<tr><td class='bold' style='color:" + getStatusColor(nh3_avg, nh3_threshold) + "'>NH3</td><td id='nh3' class='bold'>" + String(nh3_avg) + "</td><td class='bold'>" + String(nh3_threshold) + "</td></tr>");
      client.println("<tr><td class='bold' style='color:" + getStatusColor(nox_avg, nox_threshold) + "'>NOx</td><td id='nox' class='bold'>" + String(nox_avg) + "</td><td class='bold'>" + String(nox_threshold) + "</td></tr>");
      client.println("<tr><td class='bold' style='color:" + getStatusColor(co_avg, co_threshold) + "'>CO</td><td id='co' class='bold'>" + String(co_avg) + "</td><td class='bold'>" + String(co_threshold) + "</td></tr>");
      client.println("<tr><td class='bold' style='color:" + getStatusColor(smoke_avg, smoke_threshold) + "'>Smoke</td><td id='smoke' class='bold'>" + String(smoke_avg) + "</td><td class='bold'>" + String(smoke_threshold) + "</td></tr>");
      client.println("<tr><td class='bold' style='color:" + getStatusColor(co2_avg, co2_threshold) + "'>CO2</td><td id='co2' class='bold'>" + String(co2_avg) + "</td><td class='bold'>" + String(co2_threshold) + "</td></tr>");
      client.println("</table>");
      client.println("<p class='aqi' style='color:" + getStatusColor(overall_aqi, 150) + "'>Overall AQI: <span id='overall_aqi'>" + String(overall_aqi) + "</span></p>");
      client.println("<h2 class='" + getAlertClass(alertMessage) + "' id='airQualityStatus'>" + alertMessage + "</h2>");
      client.println("</div>");

      client.println("<div class='uv-monitor'>");
      client.println("<h2>UV Monitor</h2>");
      client.println("<table>");
      client.println("<tr><th>UV Index</th><th>Value</th><th>Alert Threshold</th></tr>");
      client.println("<tr><td class='bold' style='color:" + getStatusColor(uvIndex, uv_threshold) + "'>UV Index</td><td id='uv_index' class='bold'>" + String(uvIndex) + "</td><td class='bold'>" + String(uv_threshold) + "</td></tr>");
      client.println("</table>");
      client.println("<h2 class='status' id='uvStatus'>" + uvStatus + "</h2>");
      client.println("</div>");

      client.println("<div class='body-temperature'>");
      client.println("<h2>Body Temperature</h2>");
      client.println("<table>");
      client.println("<tr><th>Temperature</th><th>Value</th><th>Alert Threshold</th></tr>");
      client.println("<tr><td class='bold' style='color:" + getStatusColor(bodyTemp, temp_threshold) + "'>Body Temp</td><td id='body_temp' class='bold'>" + String(bodyTemp) + " °C</td><td class='bold'>" + String(temp_threshold) + " °C</td></tr>");
      client.println("</table>");
      client.println("<h2 class='status' id='bodyTempStatus'>" + bodyTempStatus + "</h2>");
      client.println("</div>");

      client.println("<div class='button-section'>");
      client.println("<button onclick=\"window.location.href='/buzz'\">Activate Buzzer</button>");
      client.println("<button onclick=\"window.location.href='/stopbuzz'\">Deactivate Buzzer</button>");
      client.println("<button onclick=\"window.location.href='/reduce'\">Reduce Thresholds</button>");
      client.println("<button onclick=\"window.location.href='/reset'\">Reset Thresholds</button>");
      client.println("</div>");

      client.println("<div class='footer'>Personal Outdoor Health Monitor by Phil Hutchison & Raima Zeeshan. Powered by Arduino UNO R4 WiFi.</div>");
      client.println("</body>");
      client.println("</html>");
      delay(1);

      client.stop();
      Serial.println("Client Disconnected.");
    }

    // Update the LED matrix display
    updateLEDMatrix(nh3_avg, nox_avg, co_avg, smoke_avg, co2_avg, uvIndex, bodyTemp);

    previousMillis = currentMillis;
  }
}
