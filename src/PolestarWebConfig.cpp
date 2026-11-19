#include "PolestarWebConfig.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdlib.h>

namespace {
constexpr char PREFERENCES_NAMESPACE[] = "ps-config";
constexpr char DEFAULT_TOKEN_URL[] =
    "https://pc-api.polestar.com/eu-north-1/data-portal/m2m/token";
constexpr char DEFAULT_API_URL[] =
    "https://pc-api.polestar.com/eu-north-1/data-portal/m2m/v1/vehicles/";
constexpr size_t MAX_WIFI_SSID_LENGTH = 32;
constexpr size_t MAX_WIFI_PASSWORD_LENGTH = 63;
constexpr size_t MAX_URL_LENGTH = 192;
constexpr size_t MAX_VIN_LENGTH = 32;
constexpr size_t MAX_CREDENTIAL_LENGTH = 128;
constexpr size_t MAX_CLIENT_SECRET_LENGTH = 256;
constexpr size_t MAX_CUSTOM_VALUES_LENGTH = 3000;

const char HTML_STYLE[] PROGMEM =
    "body{font-family:Arial,sans-serif;max-width:560px;margin:32px auto;"
    "padding:16px;background:#f4f4f4;color:#2d3436;}"
    "h1{color:#d63031;font-size:1.55em;margin:0 0 18px;}"
    "h2{font-size:1.05em;margin:24px 0 8px;padding-bottom:6px;"
    "border-bottom:1px solid #dfe3e5;}"
    ".card{background:#fff;padding:22px;border-radius:8px;"
    "box-shadow:0 2px 6px rgba(0,0,0,.15);}"
    "label{display:block;margin-top:12px;font-weight:bold;}"
    "input{width:100%;padding:9px;margin-top:4px;box-sizing:border-box;"
    "border:1px solid #ccc;border-radius:4px;font-size:1em;}"
    "input[type=checkbox]{width:auto;margin-right:8px;}"
    ".check{display:flex;align-items:center;font-weight:normal;}"
    ".btn{display:block;margin-top:22px;width:100%;padding:12px;"
    "background:#d63031;color:#fff;border:0;border-radius:4px;"
    "font-size:1em;cursor:pointer;}"
    ".btn:hover{background:#c0392b;}"
    ".note{color:#636e72;font-size:.88em;margin:5px 0 0;}"
    ".message{padding:10px;border-radius:4px;background:#e8f6ef;color:#176b3a;}"
    ".error{background:#fdecea;color:#a52822;}"
    "@media(max-width:600px){body{margin:12px auto;padding:10px}.card{padding:18px;}}";

String formValue(WebServer& server, const char* name) {
  return server.hasArg(name) ? server.arg(name) : String();
}

bool isValidCustomId(const char* id) {
  if (id == nullptr || *id == '\0') {
    return false;
  }
  for (const char* character = id; *character != '\0'; ++character) {
    if (!((*character >= 'A' && *character <= 'Z') ||
          (*character >= 'a' && *character <= 'z') ||
          (*character >= '0' && *character <= '9') || *character == '_')) {
      return false;
    }
  }
  return true;
}

String customInputName(const char* id) {
  return String("custom_") + id;
}
}

bool PolestarWebConfig::Settings::isConfigured() const {
  return wifiSsid.length() > 0 && tokenUrl.length() > 0 && apiUrl.length() > 0 &&
         vin.length() > 0 && clientId.length() > 0 && clientSecret.length() > 0 &&
         accountId.length() > 0;
}

PolestarWebConfig::PolestarWebConfig(const char* setupApSsid,
                                     const char* setupApPassword,
                                     const CustomField* customFields,
                                     size_t customFieldCount)
    : setupApSsid_(setupApSsid),
      setupApPassword_(setupApPassword),
      server_(80),
      customFields_(customFields),
      customFieldCount_(customFieldCount),
      customValuesJson_("{}"),
      saveEventPending_(false) {}

bool PolestarWebConfig::begin() {
  loadSettings();
  if (setupApSsid_ == nullptr || setupApPassword_ == nullptr ||
      strlen(setupApPassword_) < 8 || !validateCustomFieldSchema()) {
    return false;
  }

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(setupApSsid_, setupApPassword_)) {
    return false;
  }

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  return true;
}

void PolestarWebConfig::handleClient() {
  server_.handleClient();
}

const PolestarWebConfig::Settings& PolestarWebConfig::settings() const {
  return settings_;
}

String PolestarWebConfig::customValue(const char* id) const {
  if (id == nullptr) {
    return "";
  }
  for (size_t index = 0; index < customFieldCount_; ++index) {
    if (strcmp(customFields_[index].id, id) == 0) {
      JsonDocument values;
      if (!deserializeJson(values, customValuesJson_)) {
        JsonVariantConst value = values[id];
        if (!value.isNull()) {
          return value.as<String>();
        }
      }
      return customFields_[index].defaultValue == nullptr
                 ? String()
                 : String(customFields_[index].defaultValue);
    }
  }
  return "";
}

IPAddress PolestarWebConfig::portalIp() const {
  return WiFi.softAPIP();
}

bool PolestarWebConfig::consumeSaveEvent() {
  const bool event = saveEventPending_;
  saveEventPending_ = false;
  return event;
}

void PolestarWebConfig::loadSettings() {
  settings_.tokenUrl = DEFAULT_TOKEN_URL;
  settings_.apiUrl = DEFAULT_API_URL;
  if (!preferences_.begin(PREFERENCES_NAMESPACE, true)) {
    return;
  }
  settings_.wifiSsid = preferences_.getString("wifi_ssid", "");
  settings_.wifiPassword = preferences_.getString("wifi_pass", "");
  settings_.tokenUrl = preferences_.getString("token_url", DEFAULT_TOKEN_URL);
  settings_.apiUrl = preferences_.getString("api_url", DEFAULT_API_URL);
  settings_.vin = preferences_.getString("vin", "");
  settings_.clientId = preferences_.getString("client_id", "");
  settings_.clientSecret = preferences_.getString("client_secret", "");
  settings_.accountId = preferences_.getString("account_id", "");
  customValuesJson_ = preferences_.getString("custom", "{}");
  preferences_.end();
  JsonDocument customValues;
  if (deserializeJson(customValues, customValuesJson_)) {
    customValuesJson_ = "{}";
  }
}

bool PolestarWebConfig::saveSettings(const Settings& settings,
                                    const String& customValuesJson) {
  if (!preferences_.begin(PREFERENCES_NAMESPACE, false)) {
    return false;
  }
  bool stored = true;
  stored &= preferences_.putString("wifi_ssid", settings.wifiSsid) > 0;
  stored &= preferences_.putString("wifi_pass", settings.wifiPassword) > 0;
  stored &= preferences_.putString("token_url", settings.tokenUrl) > 0;
  stored &= preferences_.putString("api_url", settings.apiUrl) > 0;
  stored &= preferences_.putString("vin", settings.vin) > 0;
  stored &= preferences_.putString("client_id", settings.clientId) > 0;
  stored &= preferences_.putString("client_secret", settings.clientSecret) > 0;
  stored &= preferences_.putString("account_id", settings.accountId) > 0;
  stored &= preferences_.putString("custom", customValuesJson) > 0;
  preferences_.end();
  return stored;
}

void PolestarWebConfig::handleRoot() {
  String content;
  content.reserve(2500);
  content += "<h1>&#x1F50C; Device Configuration</h1>";
  content += "<form action='/save' method='POST'>";
  content += "<h2>Wi-Fi access point</h2>";
  content += "<label>Network name (SSID)</label><input name='wifi_ssid' "
             "maxlength='32' required value='" + htmlEscape(settings_.wifiSsid) + "'>";
  content += "<label>Network password</label><input type='password' "
             "name='wifi_password' maxlength='63' autocomplete='new-password' "
             "placeholder='Leave blank to keep saved password'>";
  content += "<label class='check'><input type='checkbox' name='clear_wifi_password' "
             "value='1'>Connect to an open Wi-Fi network</label>";

  content += "<h2>Polestar server</h2>";
  content += "<label>Token URL</label><input name='token_url' maxlength='192' "
             "required value='" + htmlEscape(settings_.tokenUrl) + "'>";
  content += "<label>API base URL</label><input name='api_url' maxlength='192' "
             "required value='" + htmlEscape(settings_.apiUrl) + "'>";
  content += "<label>Vehicle identification number (VIN)</label>";
  content += "<input name='vin' maxlength='32' required value='" +
             htmlEscape(settings_.vin) + "'>";
  content += "<label>Client ID</label><input name='client_id' maxlength='128' "
             "required value='" + htmlEscape(settings_.clientId) + "'>";
  content += "<label>Client secret</label><input type='password' "
             "name='client_secret' maxlength='256' autocomplete='new-password' "
             "placeholder='Leave blank to keep saved secret'>";
  content += "<p class='note'>Enter a new secret here when the current one is rotated.</p>";
  content += "<label>Account ID (x-client-id)</label>";
  content += "<input name='account_id' maxlength='128' required value='" +
             htmlEscape(settings_.accountId) + "'>";

  for (size_t index = 0; index < customFieldCount_; ++index) {
    const CustomField& field = customFields_[index];
    const String name = customInputName(field.id);
    const String value = customValue(field.id);
    content += "<label for='" + name + "'>" + htmlEscape(field.label) + "</label>";
    if (field.type == FieldType::Secret) {
      content += "<input type='password' id='" + name + "' name='" + name +
                 "' autocomplete='new-password' maxlength='" +
                 String(field.maxLength) + "' placeholder='Leave blank to keep saved value'";
    } else if (field.type == FieldType::Number) {
      content += "<input type='number' id='" + name + "' name='" + name +
                 "' value='" + htmlEscape(value) + "' min='" +
                 String(field.minValue, 8) + "' max='" +
                 String(field.maxValue, 8) + "' step='" +
                 String(field.step, 8) + "'";
    } else {
      content += "<input type='text' id='" + name + "' name='" + name +
                 "' value='" + htmlEscape(value) + "' maxlength='" +
                 String(field.maxLength) + "'";
    }
    if (field.required &&
        (field.type != FieldType::Secret || value.length() == 0)) {
      content += " required";
    }
    content += ">";
  }
  content += "<button class='btn' type='submit'>Save Configuration</button></form>";
  server_.send(200, "text/html; charset=utf-8", renderPage(content));
}

void PolestarWebConfig::handleSave() {
  Settings updated = settings_;
  updated.wifiSsid = formValue(server_, "wifi_ssid");
  updated.wifiSsid.trim();
  updated.tokenUrl = formValue(server_, "token_url");
  updated.tokenUrl.trim();
  updated.apiUrl = formValue(server_, "api_url");
  updated.apiUrl.trim();
  updated.vin = formValue(server_, "vin");
  updated.vin.trim();
  updated.clientId = formValue(server_, "client_id");
  updated.clientId.trim();
  updated.accountId = formValue(server_, "account_id");
  updated.accountId.trim();

  const String newWifiPassword = formValue(server_, "wifi_password");
  if (server_.hasArg("clear_wifi_password")) {
    updated.wifiPassword = "";
  } else if (newWifiPassword.length() > 0) {
    updated.wifiPassword = newWifiPassword;
  }

  const String newClientSecret = formValue(server_, "client_secret");
  if (newClientSecret.length() > 0) {
    updated.clientSecret = newClientSecret;
  }

  JsonDocument customValues;
  if (deserializeJson(customValues, customValuesJson_)) {
    customValues.to<JsonObject>();
  }
  for (size_t index = 0; index < customFieldCount_; ++index) {
    const CustomField& field = customFields_[index];
    const String inputName = customInputName(field.id);
    if (!server_.hasArg(inputName)) {
      continue;
    }
    const String value = server_.arg(inputName);
    if (field.type == FieldType::Secret && value.length() == 0) {
      continue;
    }
    customValues[field.id] = value;
  }

  String error;
  if (updated.wifiSsid.length() == 0 || updated.wifiSsid.length() > MAX_WIFI_SSID_LENGTH) {
    error = "Enter a Wi-Fi name of 1 to 32 characters.";
  } else if (updated.wifiPassword.length() > MAX_WIFI_PASSWORD_LENGTH) {
    error = "The Wi-Fi password must be no more than 63 characters.";
  } else if (!isHttpsUrl(updated.tokenUrl) || updated.tokenUrl.length() > MAX_URL_LENGTH) {
    error = "The token URL must be a valid HTTPS URL no longer than 192 characters.";
  } else if (!isHttpsUrl(updated.apiUrl) || updated.apiUrl.length() > MAX_URL_LENGTH) {
    error = "The API URL must be a valid HTTPS URL no longer than 192 characters.";
  } else if (updated.vin.length() == 0 || updated.vin.length() > MAX_VIN_LENGTH) {
    error = "Enter a VIN of 1 to 32 characters.";
  } else if (updated.clientId.length() == 0 || updated.clientId.length() > MAX_CREDENTIAL_LENGTH) {
    error = "Enter a client ID of 1 to 128 characters.";
  } else if (updated.clientSecret.length() == 0 || updated.clientSecret.length() > MAX_CLIENT_SECRET_LENGTH) {
    error = "Enter a client secret of 1 to 256 characters.";
  } else if (updated.accountId.length() == 0 || updated.accountId.length() > MAX_CREDENTIAL_LENGTH) {
    error = "Enter an account ID of 1 to 128 characters.";
  }

  for (size_t index = 0; error.length() == 0 && index < customFieldCount_; ++index) {
    const CustomField& field = customFields_[index];
    JsonVariantConst storedValue = customValues[field.id];
    String value = storedValue.isNull()
                       ? (field.defaultValue == nullptr ? String() : String(field.defaultValue))
                       : storedValue.as<String>();
    if (value.length() == 0 && field.required) {
      error = String("Enter a value for ") + field.label + ".";
    } else if (value.length() > field.maxLength) {
      error = String(field.label) + " is too long.";
    } else if (field.type == FieldType::Number && value.length() > 0) {
      char* end = nullptr;
      const double number = strtod(value.c_str(), &end);
      if (end == value.c_str() || *end != '\0' || !isfinite(number) ||
          number < field.minValue || number > field.maxValue) {
        error = String(field.label) + " is outside its allowed range.";
      } else if (field.step > 0.0) {
        const double stepCount = (number - field.minValue) / field.step;
        if (fabs(stepCount - round(stepCount)) > 0.000001) {
          error = String(field.label) + " does not match the allowed increment.";
        }
      }
    }
  }

  String updatedCustomValues;
  if (error.length() == 0) {
    serializeJson(customValues, updatedCustomValues);
    if (updatedCustomValues.length() > MAX_CUSTOM_VALUES_LENGTH) {
      error = "The custom configuration is too large to save.";
    }
  }

  if (error.length() > 0) {
    const String content = "<p class='message error'>" + htmlEscape(error) +
                           "</p><p><a href='/'>Back to configuration</a></p>";
    server_.send(400, "text/html; charset=utf-8", renderPage(content));
    return;
  }

  if (!updated.apiUrl.endsWith("/")) {
    updated.apiUrl += "/";
  }
  if (!saveSettings(updated, updatedCustomValues)) {
    const String content = "<p class='message error'>Could not save settings to device storage.</p>"
                           "<p><a href='/'>Back to configuration</a></p>";
    server_.send(500, "text/html; charset=utf-8", renderPage(content));
    return;
  }
  settings_ = updated;
  customValuesJson_ = updatedCustomValues;
  saveEventPending_ = true;

  const String content = "<p class='message'>Configuration saved.</p>"
                        "<p>The sketch can now connect to the configured Wi-Fi network.</p>"
                        "<p><a href='/'>Return to settings</a></p>";
  server_.send(200, "text/html; charset=utf-8", renderPage(content));
}

void PolestarWebConfig::handleNotFound() {
  server_.sendHeader("Location", "/", true);
  server_.send(302, "text/plain", "Redirecting");
}

String PolestarWebConfig::renderPage(const String& content) const {
  String page;
  page.reserve(800 + content.length());
  page += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Device Configuration</title><style>";
  page += FPSTR(HTML_STYLE);
  page += "</style></head><body><main class='card'>";
  page += content;
  page += "</main></body></html>";
  return page;
}

String PolestarWebConfig::htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}

bool PolestarWebConfig::isHttpsUrl(const String& value) {
  return value.startsWith("https://") && value.length() > 8;
}

bool PolestarWebConfig::validateCustomFieldSchema() const {
  if (customFieldCount_ > 0 && customFields_ == nullptr) {
    return false;
  }
  for (size_t index = 0; index < customFieldCount_; ++index) {
    const CustomField& field = customFields_[index];
    if (!isValidCustomId(field.id) || field.label == nullptr ||
        field.maxLength == 0 ||
        (field.type == FieldType::Number &&
         (!isfinite(field.minValue) || !isfinite(field.maxValue) ||
          field.minValue > field.maxValue || field.step <= 0.0))) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (strcmp(field.id, customFields_[previous].id) == 0) {
        return false;
      }
    }
  }
  return true;
}
