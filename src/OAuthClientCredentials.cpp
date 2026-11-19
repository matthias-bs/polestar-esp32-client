#include "OAuthClientCredentials.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

namespace {
constexpr time_t TOKEN_REFRESH_MARGIN_SECONDS = 60;
}

OAuthClientCredentials::OAuthClientCredentials(WiFiClientSecure& client,
                                               const char* tokenUrl,
                                               const char* clientId,
                                               const char* clientSecret,
                                               const char* scope)
    : client_(client),
      tokenUrl_(tokenUrl),
      clientId_(clientId),
      clientSecret_(clientSecret),
      scope_(scope),
      expiresAt_(0),
      lastHttpCode_(0) {}

bool OAuthClientCredentials::getToken(String& token) {
  const time_t now = time(nullptr);
  if (token_.length() > 0 &&
      (expiresAt_ == 0 || now < expiresAt_ - TOKEN_REFRESH_MARGIN_SECONDS)) {
    token = token_;
    return true;
  }

  token_ = "";
  expiresAt_ = 0;
  lastHttpCode_ = 0;
  lastError_ = "";

  HTTPClient http;
  if (!http.begin(client_, tokenUrl_)) {
    lastError_ = "Could not initialize token request.";
    return false;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  const String requestData = "clientId=" + urlEncode(clientId_) +
                            "&clientSecret=" + urlEncode(clientSecret_) +
                            "&scope=" + urlEncode(scope_);
  lastHttpCode_ = http.POST(requestData);
  const String response = http.getString();
  http.end();

  if (lastHttpCode_ != HTTP_CODE_OK) {
    lastError_ = response.length() > 0 ? response : "Token request failed.";
    return false;
  }

  JsonDocument document;
  const DeserializationError parseError = deserializeJson(document, response);
  if (parseError) {
    lastError_ = parseError.c_str();
    return false;
  }

  token_ = document["accessToken"].as<String>();
  if (token_.length() == 0) {
    token_ = document["access_token"].as<String>();
  }
  if (token_.length() == 0) {
    lastError_ = "Token response did not contain an access token.";
    return false;
  }

  const long expiresIn = document["expiresIn"] | (document["expires_in"] | 0L);
  if (expiresIn > 0) {
    expiresAt_ = time(nullptr) + expiresIn;
  }

  token = token_;
  return true;
}

void OAuthClientCredentials::invalidate() {
  token_ = "";
  expiresAt_ = 0;
}

time_t OAuthClientCredentials::expiresAt() const {
  return expiresAt_;
}

int OAuthClientCredentials::lastHttpCode() const {
  return lastHttpCode_;
}

const String& OAuthClientCredentials::lastError() const {
  return lastError_;
}

String OAuthClientCredentials::urlEncode(const char* value) {
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  while (value != nullptr && *value != '\0') {
    const uint8_t character = static_cast<uint8_t>(*value++);
    if ((character >= 'A' && character <= 'Z') ||
        (character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9') || character == '-' ||
        character == '_' || character == '.' || character == '~') {
      encoded += static_cast<char>(character);
    } else {
      encoded += '%';
      encoded += hex[character >> 4];
      encoded += hex[character & 0x0F];
    }
  }
  return encoded;
}
