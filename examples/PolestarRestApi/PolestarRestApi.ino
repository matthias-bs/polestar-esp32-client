#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

#include "secrets.h"
#include <OAuthClientCredentials.h>
#include <PolestarClient.h>

constexpr time_t MIN_VALID_EPOCH = 1700000000;
constexpr char OAUTH_SCOPE[] = "pdp-telemetry/battery pdp-telemetry/location";

WiFiClientSecure secureClient;
OAuthClientCredentials oauthClient(secureClient, tokenUrl, clientId, clientSecret,
                                   OAUTH_SCOPE);
PolestarClient polestarClient(secureClient, oauthClient, apiUrl, vin,
                              apiAccountId);

bool waitForTimeSync(unsigned long timeoutMs) {
  const unsigned long start = millis();
  while (time(nullptr) < MIN_VALID_EPOCH) {
    if (millis() - start >= timeoutMs) {
      return false;
    }
    delay(250);
    Serial.print(".");
  }
  return true;
}

void printTokenLifetime() {
  const time_t expiresAt = oauthClient.expiresAt();
  if (expiresAt == 0) {
    return;
  }

  const time_t remaining = expiresAt - time(nullptr);
  Serial.print("OAuth token remaining lifetime: ");
  Serial.print(remaining > 0 ? remaining : 0);
  Serial.println(" seconds.");
}

void printRequestError(const char* operation) {
  Serial.print(operation);
  Serial.print(" failed. HTTP code: ");
  Serial.println(polestarClient.lastHttpCode());
  if (polestarClient.lastError().length() > 0) {
    Serial.println(polestarClient.lastError());
  }
}

void fetchVehicleData() {
  JsonDocument doc;
  if (!polestarClient.getTelemetry("battery", doc)) {
    printRequestError("Battery request");
    return;
  }

  JsonObjectConst data = doc["data"].as<JsonObjectConst>();
  if (data.isNull()) {
    Serial.println("Battery response did not contain a data object.");
    return;
  }

  JsonVariantConst batteryLevel = data["batteryChargeLevelPercentage"];
  JsonVariantConst range = data["estimatedDistanceToEmptyKm"];

  Serial.print("🔋 Battery level (SoC): ");
  if (batteryLevel.isNull()) {
    Serial.println("not reported");
  } else {
    Serial.print(batteryLevel.as<float>(), 1);
    Serial.println("%");
  }

  Serial.print("🚗 Estimated range: ");
  if (range.isNull()) {
    Serial.println("not reported");
  } else {
    Serial.print(range.as<float>(), 1);
    Serial.println(" km");
  }
}

void fetchVehicleLocation() {
  JsonDocument doc;
  if (!polestarClient.getTelemetry("location", doc)) {
    printRequestError("Location request");
    return;
  }

  JsonObjectConst data = doc["data"].as<JsonObjectConst>();
  if (data.isNull()) {
    Serial.println("Location response did not contain a data object.");
    return;
  }

  JsonObjectConst coordinate = data["coordinate"].as<JsonObjectConst>();
  if (coordinate.isNull()) {
    Serial.println("Vehicle location was not reported.");
    return;
  }

  JsonVariantConst latitude = coordinate["latitude"];
  JsonVariantConst longitude = coordinate["longitude"];
  if (latitude.isNull() || longitude.isNull()) {
    Serial.println("Vehicle coordinates were not fully reported.");
    return;
  }

  const double latitudeValue = latitude.as<double>();
  const double longitudeValue = longitude.as<double>();
  Serial.print("📍 Coordinates: ");
  Serial.print(latitudeValue, 6);
  Serial.print(", ");
  Serial.println(longitudeValue, 6);

  Serial.print("🗺️ Google Maps marker: https://www.google.com/maps/search/?api=1&query=");
  Serial.print(latitudeValue, 6);
  Serial.print(",");
  Serial.println(longitudeValue, 6);

  Serial.print("🗺️ OpenStreetMap marker: https://www.openstreetmap.org/?mlat=");
  Serial.print(latitudeValue, 6);
  Serial.print("&mlon=");
  Serial.print(longitudeValue, 6);
  Serial.print("#map=16/");
  Serial.print(latitudeValue, 6);
  Serial.print("/");
  Serial.println(longitudeValue, 6);
}

void setup() {
  Serial.begin(115200);
  secureClient.setCACert(rootCa);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Synchronizing system time");
  if (waitForTimeSync(30000)) {
    Serial.println("\nSystem time synchronized.");
  } else {
    Serial.println("\nTime synchronization timed out; TLS requests will be retried.");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (time(nullptr) < MIN_VALID_EPOCH) {
      Serial.println("System time not synchronized yet; skipping TLS requests.");
      delay(5000);
    } else {
      fetchVehicleData();
      fetchVehicleLocation();
      printTokenLifetime();
      delay(300000);
    }
  } else {
    Serial.println("Wi-Fi connection lost!");
    delay(5000);
  }
}
