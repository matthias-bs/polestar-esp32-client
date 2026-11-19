#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include <OAuthClientCredentials.h>
#include <PolestarClient.h>
#include <PolestarWebConfig.h>

constexpr uint8_t OUTPUT_PIN = 26;
constexpr bool OUTPUT_ACTIVE_HIGH = true;
constexpr uint32_t LOCATION_POLL_INTERVAL_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t LOCATION_STALE_TIMEOUT_MS = 15UL * 60UL * 1000UL;
constexpr uint8_t STATE_CHANGE_DEBOUNCE_SAMPLES = 2;
constexpr time_t MIN_VALID_EPOCH = 1700000000;
constexpr char OAUTH_SCOPE[] = "pdp-telemetry/location";

const PolestarWebConfig::CustomField HOME_ZONE_FIELDS[] = {
    {"home_latitude", "Home latitude", PolestarWebConfig::FieldType::Number,
     true, "", 20, -89.9, 89.9, 0.000001},
    {"home_longitude", "Home longitude", PolestarWebConfig::FieldType::Number,
     true, "", 20, -179.9, 179.9, 0.000001},
    {"home_half_width", "Home-zone half-width (meters)",
     PolestarWebConfig::FieldType::Number, true, "20", 20, 1.0, 500.0, 1.0}};

  const char* setupApSsid = "Polestar-Home-Setup";
  const char* setupApPassword = "dummy-password";

// Replace with a unique password for the setup AP
  PolestarWebConfig portal(setupApSsid, setupApPassword, HOME_ZONE_FIELDS,
                         sizeof(HOME_ZONE_FIELDS) / sizeof(HOME_ZONE_FIELDS[0]));
WiFiClientSecure secureClient;
OAuthClientCredentials* oauthClient = nullptr;
PolestarClient* polestarClient = nullptr;

// ISRG Root X1 CA used as the TLS trust anchor to verify Polestar HTTPS servers.
static const char ROOT_CA[] PROGMEM = R"EOF(
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

uint32_t lastPollAttemptMs = 0;
uint32_t lastFreshLocationMs = 0;
bool hasPollAttempted = false;
bool hasFreshLocation = false;
bool ntpConfigured = false;
bool homeZoneOutputActive = false;
bool hasPendingState = false;
bool pendingState = false;
uint8_t pendingStateSamples = 0;
double homeLatitude = 0.0;
double homeLongitude = 0.0;
double homeHalfWidthMeters = 0.0;

void setHomeZoneOutput(bool active) {
  homeZoneOutputActive = active;
  const bool pinLevel = OUTPUT_ACTIVE_HIGH ? active : !active;
  digitalWrite(OUTPUT_PIN, pinLevel ? HIGH : LOW);
  Serial.print("Home-zone outlet control: ");
  Serial.println(active ? "ON" : "OFF");
}

void resetPendingState() {
  hasPendingState = false;
  pendingStateSamples = 0;
}

bool loadHomeZone() {
  const PolestarWebConfig::Settings& settings = portal.settings();
  const String latitudeText = portal.customValue("home_latitude");
  const String longitudeText = portal.customValue("home_longitude");
  const String halfWidthText = portal.customValue("home_half_width");
  if (!settings.isConfigured() || latitudeText.length() == 0 ||
      longitudeText.length() == 0 || halfWidthText.length() == 0) {
    return false;
  }

  char* end = nullptr;
  homeLatitude = strtod(latitudeText.c_str(), &end);
  if (end == latitudeText.c_str() || *end != '\0') {
    return false;
  }
  homeLongitude = strtod(longitudeText.c_str(), &end);
  if (end == longitudeText.c_str() || *end != '\0') {
    return false;
  }
  homeHalfWidthMeters = strtod(halfWidthText.c_str(), &end);
  if (end == halfWidthText.c_str() || *end != '\0') {
    return false;
  }
  return isfinite(homeLatitude) && homeLatitude >= -89.9 && homeLatitude <= 89.9 &&
         isfinite(homeLongitude) && homeLongitude >= -179.9 && homeLongitude <= 179.9 &&
         isfinite(homeHalfWidthMeters) && homeHalfWidthMeters >= 1.0 &&
         homeHalfWidthMeters <= 500.0;
}

void initializePolestarClient() {
  if (oauthClient != nullptr) {
    delete polestarClient;
    delete oauthClient;
    polestarClient = nullptr;
    oauthClient = nullptr;
  }
  const PolestarWebConfig::Settings& settings = portal.settings();
  if (!loadHomeZone()) {
    Serial.println("Polestar credentials or home-zone settings are incomplete.");
    setHomeZoneOutput(false);
    return;
  }

  oauthClient = new OAuthClientCredentials(
      secureClient, settings.tokenUrl.c_str(), settings.clientId.c_str(),
      settings.clientSecret.c_str(), OAUTH_SCOPE);
  polestarClient = new PolestarClient(
      secureClient, *oauthClient, settings.apiUrl.c_str(), settings.vin.c_str(),
      settings.accountId.c_str());
}

void connectConfiguredWifi() {
  const PolestarWebConfig::Settings& settings = portal.settings();
  if (settings.wifiSsid.length() == 0) {
    return;
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  Serial.print("Connecting to configured Wi-Fi: ");
  Serial.println(settings.wifiSsid);
}

bool isInsideHomeZone(double latitude, double longitude) {
  constexpr double METERS_PER_DEGREE_LATITUDE = 111132.92;
  constexpr double METERS_PER_DEGREE_LONGITUDE = 111319.49;
  const double latitudeRadians = homeLatitude * PI / 180.0;
  const double northMeters = (latitude - homeLatitude) * METERS_PER_DEGREE_LATITUDE;
  const double eastMeters = (longitude - homeLongitude) *
                            METERS_PER_DEGREE_LONGITUDE * cos(latitudeRadians);
  return fabs(northMeters) <= homeHalfWidthMeters &&
         fabs(eastMeters) <= homeHalfWidthMeters;
}

void updateHomeZoneOutput(bool inside) {
  if (inside == homeZoneOutputActive) {
    resetPendingState();
    return;
  }
  if (!hasPendingState || pendingState != inside) {
    pendingState = inside;
    pendingStateSamples = 1;
    hasPendingState = true;
    return;
  }
  if (++pendingStateSamples >= STATE_CHANGE_DEBOUNCE_SAMPLES) {
    setHomeZoneOutput(inside);
    resetPendingState();
  }
}

void forceOutputOffIfLocationStale() {
  if (homeZoneOutputActive && hasFreshLocation &&
      millis() - lastFreshLocationMs >= LOCATION_STALE_TIMEOUT_MS) {
    Serial.println("Location is stale; turning the home-zone output OFF.");
    setHomeZoneOutput(false);
    resetPendingState();
  }
}

void pollVehicleLocation() {
  if (polestarClient == nullptr) {
    return;
  }
  JsonDocument response;
  if (!polestarClient->getTelemetry("location", response)) {
    Serial.print("Location request failed: ");
    Serial.println(polestarClient->lastError());
    resetPendingState();
    return;
  }

  JsonObjectConst data = response["data"].as<JsonObjectConst>();
  JsonObjectConst coordinate = data["coordinate"].as<JsonObjectConst>();
  JsonVariantConst latitudeValue = coordinate["latitude"];
  JsonVariantConst longitudeValue = coordinate["longitude"];
  JsonVariantConst timestampSeconds = data["timestamp"]["seconds"];

  Serial.print("Location timestamp.seconds: ");
  if (timestampSeconds.isNull()) {
    Serial.println("missing");
  } else {
    serializeJson(timestampSeconds, Serial);
    Serial.println();
  }

  if (data.isNull() || coordinate.isNull() || latitudeValue.isNull() ||
      longitudeValue.isNull()) {
    Serial.println("Location response is incomplete or does not contain coordinates.");
    resetPendingState();
    return;
  }

  const double latitude = latitudeValue.as<double>();
  const double longitude = longitudeValue.as<double>();
  if (!isfinite(latitude) || !isfinite(longitude) || latitude < -90.0 ||
      latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
    Serial.println("Location response contains invalid coordinates.");
    resetPendingState();
    return;
  }

  hasFreshLocation = true;
  lastFreshLocationMs = millis();
  const bool inside = isInsideHomeZone(latitude, longitude);
  Serial.print("Vehicle is ");
  Serial.println(inside ? "inside" : "outside");
  updateHomeZoneOutput(inside);
}

void setup() {
  Serial.begin(115200);
  pinMode(OUTPUT_PIN, OUTPUT);
  setHomeZoneOutput(false);
  secureClient.setCACert(ROOT_CA);

  if (!portal.begin()) {
    Serial.println("Could not start the configuration portal.");
    return;
  }
  Serial.print("Connect to the Wi-Fi access point '");
  Serial.print(setupApSsid);
  Serial.println("' and open the home-zone portal at:");
  Serial.print("http://");
  Serial.println(portal.portalIp());

  if (portal.settings().isConfigured()) {
    initializePolestarClient();
    connectConfiguredWifi();
  } else {
    Serial.println("Configure Wi-Fi, Polestar credentials, and home-zone coordinates.");
  }
}

void loop() {
  portal.handleClient();
  if (portal.consumeSaveEvent()) {
    initializePolestarClient();
    connectConfiguredWifi();
  }

  if (WiFi.status() == WL_CONNECTED && !ntpConfigured) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpConfigured = true;
  }

  const uint32_t now = millis();
  if (polestarClient != nullptr && WiFi.status() == WL_CONNECTED &&
      time(nullptr) >= MIN_VALID_EPOCH &&
      (!hasPollAttempted || now - lastPollAttemptMs >= LOCATION_POLL_INTERVAL_MS)) {
    lastPollAttemptMs = now;
    hasPollAttempted = true;
    pollVehicleLocation();
  }

  forceOutputOffIfLocationStale();
  delay(2);
}
