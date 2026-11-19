# Polestar Vehicle Portal API for ESP32

An Arduino library for ESP32 sketches that authenticate to the [Polestar data portal](https://data-portal.polestar.com), retrieve vehicle telemetry or charging settings, and optionally provide a reusable Wi-Fi and Polestar credential setup portal.

This library uses Polestar's official Data Portal API (REST API with OAuth).

## Library

The library is in `src/` and provides three building blocks:

- `OAuthClientCredentials` requests and caches OAuth access tokens (not the client secret), refreshes them shortly before expiration, and supports explicit invalidation.
- `PolestarClient` makes authenticated requests for vehicle telemetry and charging resources, parses successful responses into an ArduinoJson document, and retries once after an authorization failure.
- `PolestarWebConfig` serves a setup form over a password-protected ESP32 access point. It stores Wi-Fi and Polestar credentials in `Preferences` and can render caller-defined text, number, and secret fields. Retrieve custom field values with `customValue(id)`.

The OAuth and Polestar clients use a caller-provided `WiFiClientSecure`, allowing each sketch to configure its own trusted root CA. The system clock must be synchronized before TLS requests.

## Requirements and Installation

- ESP32 Arduino core
- ArduinoJson 7 or later

Install this repository as an Arduino library in the Arduino sketchbook's `libraries` directory, then install ArduinoJson using the Arduino Library Manager. With Arduino CLI, compile an example from the repository root with:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --libraries .. examples/PolestarRestApi
```

Substitute another example directory to build that example instead.

## Getting Polestar Credentials

1. Go to https://data-portal.polestar.com
2. Log in using your email address and password
3. Authenticate with the token send to your email address
4. Select **Data Portal API** (left side)
5. Select **Credential** (middle tab)

Securely store the generated credentials:
* App client ID
* Expected x-client-id header (a.k.a. `apiAccountId`)
* Client secret
* M2M Token Endpoint

> [!CAUTION]
> The "Client secret" expires after 90 days.

> [!NOTE]
> You also need your VIN (Vehicle Identification Number). (see Polestar App)

See **API Documentation** (right tab) for details.

> [!CAUTION]
> There is an API rate limit of 10,000 API calls per client, per day.

## Examples

### 1. PolestarRestApi

Path: `examples/PolestarRestApi/PolestarRestApi.ino`

The basic API example connects to Wi-Fi, synchronizes time, fetches an OAuth token, then prints battery level, estimated range, and vehicle coordinates over Serial. It polls every five minutes.

Copy `examples/PolestarRestApi/secrets.h.template` to `secrets.h` beside the sketch and set the Wi-Fi credentials, VIN, client ID, client secret, and account ID. The example requests the `pdp-telemetry/battery` and `pdp-telemetry/location` scopes.

### 2. PolestarWebConfigPortal

Path: `examples/PolestarWebConfigPortal/PolestarWebConfigPortal.ino`

Demonstrates the reusable setup portal without making Polestar API calls. The portal provisions Wi-Fi and Polestar credentials and stores them in ESP32 `Preferences`. It also demonstrates connecting to the configured Wi-Fi while keeping the setup access point available.

Replace the example setup AP password with a unique value of at least eight characters before use.

### 3. PolestarHomeZoneOutput

Path: `examples/PolestarHomeZoneOutput/PolestarHomeZoneOutput.ino`

Combines the setup portal with Polestar location telemetry to control a GPIO output for a configurable square home zone. The portal asks for the home latitude and longitude and a shared half-width in meters. The GPIO action is isolated in `setHomeZoneOutput(bool)` so an application can replace the output-control path.

Compile-time defaults are a five-minute location polling interval, a 15-minute freshness timeout, two consecutive fresh readings to debounce state changes, GPIO 26, and active-high output. The output starts OFF and is forced OFF when the last fresh location becomes stale. Adjust the pin, polarity, and timing constants for the target hardware and application.

> [!WARNING]
This example only drives a logic-level GPIO; it is not a mains-rated outlet controller. Use suitable isolated, certified switching hardware and validate the installation independently before controlling a power outlet.

## Configuration Portal Notes

The portal access point and web server start on every boot. In sketches that connect to Wi-Fi, the setup access point remains enabled so settings can be changed later. Change the example AP password and restrict physical access to the device.

Portal credentials are stored in ESP32 `Preferences`, which does not encrypt data at rest. Password and secret fields are not prefilled in the form; submitting them blank retains their saved values. The home-zone example also stores its custom fields in `Preferences`.
