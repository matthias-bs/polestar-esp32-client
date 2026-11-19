#include "PolestarClient.h"

#include <HTTPClient.h>

PolestarClient::PolestarClient(WiFiClientSecure& client,
                               OAuthClientCredentials& oauth,
                               const char* apiBaseUrl, const char* vin,
                               const char* accountId)
    : client_(client),
      oauth_(oauth),
      apiBaseUrl_(apiBaseUrl),
      vin_(vin),
      accountId_(accountId),
      lastHttpCode_(0) {}

bool PolestarClient::getTelemetry(const char* domain, JsonDocument& response) {
  return get(String(vin_) + "/telemetry/" + domain, response);
}

bool PolestarClient::getCharging(const char* resource, JsonDocument& response) {
  return get(String(vin_) + "/charging/" + resource, response);
}

int PolestarClient::lastHttpCode() const {
  return lastHttpCode_;
}

const String& PolestarClient::lastError() const {
  return lastError_;
}

bool PolestarClient::get(const String& path, JsonDocument& response) {
  lastHttpCode_ = 0;
  lastError_ = "";

  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    String token;
    if (!oauth_.getToken(token)) {
      lastHttpCode_ = oauth_.lastHttpCode();
      lastError_ = oauth_.lastError();
      return false;
    }

    HTTPClient http;
    const String requestUrl = String(apiBaseUrl_) + path;
    if (!http.begin(client_, requestUrl)) {
      lastError_ = "Could not initialize API request.";
      return false;
    }

    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Accept", "application/json");
    http.addHeader("x-client-id", accountId_);

    lastHttpCode_ = http.GET();
    const String body = http.getString();
    http.end();

    if (lastHttpCode_ == HTTP_CODE_UNAUTHORIZED && attempt == 0) {
      oauth_.invalidate();
      continue;
    }
    if (lastHttpCode_ != HTTP_CODE_OK) {
      lastError_ = body.length() > 0 ? body : "API request failed.";
      return false;
    }

    const DeserializationError parseError = deserializeJson(response, body);
    if (parseError) {
      lastError_ = parseError.c_str();
      return false;
    }
    return true;
  }

  lastError_ = "API authorization failed after refreshing the token.";
  return false;
}
