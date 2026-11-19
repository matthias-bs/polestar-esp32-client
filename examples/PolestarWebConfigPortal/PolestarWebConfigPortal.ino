#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <memory>
#include <time.h>

#include <OAuthClientCredentials.h>
#include <PolestarClient.h>
#include <PolestarWebConfig.h>

constexpr time_t MIN_VALID_EPOCH = 1700000000;
constexpr char OAUTH_SCOPE[] = "pdp-telemetry/battery pdp-telemetry/location";
constexpr unsigned long POLL_INTERVAL_MS = 300000;

const char* setupApSsid = "Polestar-Setup";
// Replace with a unique password
const char* setupApPassword = "dummy-password";

PolestarWebConfig portal(setupApSsid, setupApPassword);
WiFiClientSecure secureClient;
String tokenUrl;
String apiUrl;
String vin;
String clientId;
String clientSecret;
String accountId;
std::unique_ptr<OAuthClientCredentials> oauthClient;
std::unique_ptr<PolestarClient> polestarClient;
unsigned long lastPollAt = 0;
bool hasPolled = false;
bool timeSyncMessageShown = false;

// ISRG Root X1 CA used as the TLS trust anchor to verify Polestar HTTPS servers.
static const char rootCa[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

void configurePolestarClient() {
  const PolestarWebConfig::Settings& settings = portal.settings();
  if (!settings.isConfigured()) {
    polestarClient.reset();
    oauthClient.reset();
    return;
  }

  polestarClient.reset();
  oauthClient.reset();
  tokenUrl = settings.tokenUrl;
  apiUrl = settings.apiUrl;
  vin = settings.vin;
  clientId = settings.clientId;
  clientSecret = settings.clientSecret;
  accountId = settings.accountId;

  secureClient.setCACert(rootCa);
  oauthClient.reset(new OAuthClientCredentials(
    secureClient, tokenUrl.c_str(), clientId.c_str(), clientSecret.c_str(),
    OAUTH_SCOPE));
  polestarClient.reset(new PolestarClient(
    secureClient, *oauthClient, apiUrl.c_str(), vin.c_str(), accountId.c_str()));
}

void printRequestError(const char* operation) {
  Serial.print(operation);
  Serial.print(" failed. HTTP code: ");
  Serial.println(polestarClient->lastHttpCode());
  if (polestarClient->lastError().length() > 0) {
    Serial.println(polestarClient->lastError());
  }
}

void fetchVehicleData() {
  JsonDocument doc;
  if (!polestarClient->getTelemetry("battery", doc)) {
    printRequestError("Battery request");
  } else {
    JsonObjectConst data = doc["data"].as<JsonObjectConst>();
    JsonVariantConst batteryLevel = data["batteryChargeLevelPercentage"];
    JsonVariantConst range = data["estimatedDistanceToEmptyKm"];
    Serial.print("Battery level (SoC): ");
    if (batteryLevel.isNull()) {
      Serial.println("not reported");
    } else {
      Serial.print(batteryLevel.as<float>(), 1);
      Serial.println("%");
    }
    Serial.print("Estimated range: ");
    if (range.isNull()) {
      Serial.println("not reported");
    } else {
      Serial.print(range.as<float>(), 1);
      Serial.println(" km");
    }
  }

  if (!polestarClient->getTelemetry("location", doc)) {
    printRequestError("Location request");
  } else {
    JsonObjectConst coordinate = doc["data"]["coordinate"].as<JsonObjectConst>();
    JsonVariantConst latitude = coordinate["latitude"];
    JsonVariantConst longitude = coordinate["longitude"];
    if (latitude.isNull() || longitude.isNull()) {
      Serial.println("Vehicle location was not reported.");
    } else {
      Serial.print("Vehicle coordinates: ");
      Serial.print(latitude.as<double>(), 6);
      Serial.print(", ");
      Serial.println(longitude.as<double>(), 6);
    }
  }

  const time_t expiresAt = oauthClient->expiresAt();
  if (expiresAt > 0) {
    const time_t remaining = expiresAt - time(nullptr);
    Serial.print("OAuth access token remaining lifetime: ");
    Serial.print(remaining > 0 ? remaining : 0);
    Serial.println(" seconds.");
  }
}

void connectConfiguredWifi() {
  const PolestarWebConfig::Settings& settings = portal.settings();
  if (settings.wifiSsid.length() == 0) {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  hasPolled = false;
  timeSyncMessageShown = false;
  Serial.print("Connecting to configured Wi-Fi: ");
  Serial.println(settings.wifiSsid);
}

void setup() {
  Serial.begin(115200);

  if (!portal.begin()) {
    Serial.println("Could not start the configuration portal. Check its AP password.");
    return;
  }

  Serial.print("Connect to the Wi-Fi access point '");
  Serial.print(setupApSsid);
  Serial.println("' and open the configuration portal at:");
  Serial.print("http://");
  Serial.println(portal.portalIp());

  if (portal.settings().isConfigured()) {
    configurePolestarClient();
    connectConfiguredWifi();
  } else {
    Serial.println("Configure Wi-Fi and Polestar credentials in the portal.");
  }
}

void loop() {
  portal.handleClient();
  if (portal.consumeSaveEvent()) {
    configurePolestarClient();
    connectConfiguredWifi();
  }

  if (WiFi.status() == WL_CONNECTED && polestarClient) {
    if (time(nullptr) < MIN_VALID_EPOCH) {
      if (!timeSyncMessageShown) {
        Serial.println("Waiting for system time synchronization before TLS requests.");
        timeSyncMessageShown = true;
      }
    } else if (!hasPolled || millis() - lastPollAt >= POLL_INTERVAL_MS) {
      fetchVehicleData();
      hasPolled = true;
      lastPollAt = millis();
    }
  } else {
    hasPolled = false;
  }
  delay(2);
}
