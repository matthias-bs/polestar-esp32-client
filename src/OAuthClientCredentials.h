#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <time.h>

class OAuthClientCredentials {
 public:
  OAuthClientCredentials(WiFiClientSecure& client, const char* tokenUrl,
                         const char* clientId, const char* clientSecret,
                         const char* scope);

  bool getToken(String& token);
  void invalidate();
  time_t expiresAt() const;
  int lastHttpCode() const;
  const String& lastError() const;

 private:
  static String urlEncode(const char* value);

  WiFiClientSecure& client_;
  const char* tokenUrl_;
  const char* clientId_;
  const char* clientSecret_;
  const char* scope_;
  String token_;
  time_t expiresAt_;
  int lastHttpCode_;
  String lastError_;
};
