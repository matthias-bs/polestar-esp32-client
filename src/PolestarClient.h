#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include "OAuthClientCredentials.h"

class PolestarClient {
 public:
  PolestarClient(WiFiClientSecure& client, OAuthClientCredentials& oauth,
                 const char* apiBaseUrl, const char* vin,
                 const char* accountId);

  bool getTelemetry(const char* domain, JsonDocument& response);
  bool getCharging(const char* resource, JsonDocument& response);
  int lastHttpCode() const;
  const String& lastError() const;

 private:
  bool get(const String& path, JsonDocument& response);

  WiFiClientSecure& client_;
  OAuthClientCredentials& oauth_;
  const char* apiBaseUrl_;
  const char* vin_;
  const char* accountId_;
  int lastHttpCode_;
  String lastError_;
};
