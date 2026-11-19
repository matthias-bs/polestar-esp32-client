#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

#include <stddef.h>

class PolestarWebConfig {
 public:
  enum class FieldType : uint8_t {
    Text,
    Number,
    Secret
  };

  struct CustomField {
    const char* id;
    const char* label;
    FieldType type;
    bool required;
    const char* defaultValue;
    size_t maxLength;
    double minValue;
    double maxValue;
    double step;
  };

  struct Settings {
    String wifiSsid;
    String wifiPassword;
    String tokenUrl;
    String apiUrl;
    String vin;
    String clientId;
    String clientSecret;
    String accountId;

    bool isConfigured() const;
  };

  PolestarWebConfig(const char* setupApSsid, const char* setupApPassword,
                    const CustomField* customFields = nullptr,
                    size_t customFieldCount = 0);

  bool begin();
  void handleClient();
  const Settings& settings() const;
  String customValue(const char* id) const;
  IPAddress portalIp() const;
  bool consumeSaveEvent();

 private:
  void loadSettings();
  bool saveSettings(const Settings& settings, const String& customValuesJson);
  bool validateCustomFieldSchema() const;
  void handleRoot();
  void handleSave();
  void handleNotFound();
  String renderPage(const String& content) const;
  static String htmlEscape(String value);
  static bool isHttpsUrl(const String& value);

  const char* setupApSsid_;
  const char* setupApPassword_;
  WebServer server_;
  Preferences preferences_;
  Settings settings_;
  const CustomField* customFields_;
  size_t customFieldCount_;
  String customValuesJson_;
  bool saveEventPending_;
};
