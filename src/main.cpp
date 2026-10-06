/*
 * Sterownik pomp ESP32 + OLED + SUPLA
 * Wersja projektu: PlatformIO / Arduino framework
 *
 * Logika priorytetow:
 * 1. ALARM -> obie pompy ON (ignoruje BUF_OK)
 * 2. BUF_OK = 0 -> obie pompy OFF
 * 3. Sterowanie reczne lokalne / SUPLA
 * 4. Automatyka:
 *      Grzejniki  -> pompa domowa + pompa zasilajaca
 *      Podlogowka -> pompa zasilajaca
 *      CWU        -> pompa zasilajaca
 *
 * Przyciski chwilowe:
 * - OLED: krotkie nacisniecie -> kolejny ekran
 * - DOMOWA: krotkie -> AUTO -> MAN ON -> MAN OFF -> AUTO
 *            dlugie  -> obie pompy MAN ON
 * - ZASILAJACA: tak samo
 *
 * SUPLA:
 * - dwa zdalne wymuszenia ON (OFF = brak wymuszenia / powrot do logiki lokalnej)
 * - podglad faktycznego stanu pomp
 * - podglad wejsc: grzejniki, podlogowka, CWU, alarm, BUF_OK
 *
 * UWAGA SPRZETOWA:
 * Wejsc 230 V / 12 V / 24 V NIE wolno podawac bezposrednio na ESP32.
 * Stosuj styki bezpotencjalowe, transoptory lub odpowiedni interfejs wejsc.
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <SuplaDevice.h>
#include <supla/control/button.h>
#include <supla/control/virtual_relay.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/general_purpose_measurement.h>
#include <supla/storage/eeprom.h>
#include <supla/storage/littlefs_config.h>

namespace {

// -----------------------------------------------------------------------------
// PINY - ESP32 DevKit V1
// -----------------------------------------------------------------------------
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;

constexpr uint8_t PIN_PUMP_HOME = 25;
constexpr uint8_t PIN_PUMP_SUPPLY = 26;

constexpr uint8_t PIN_IN_RADIATORS = 16;
constexpr uint8_t PIN_IN_FLOOR = 17;
constexpr uint8_t PIN_IN_CWU = 18;
constexpr uint8_t PIN_IN_ALARM = 19;
constexpr uint8_t PIN_IN_BUF_OK = 23;

constexpr uint8_t PIN_BTN_SCREEN = 27;
constexpr uint8_t PIN_BTN_HOME = 32;
constexpr uint8_t PIN_BTN_SUPPLY = 33;

constexpr uint8_t PIN_CONFIG_BUTTON = 0;  // BOOT - konfiguracja SUPLA
constexpr uint8_t PIN_STATUS_LED = 2;

// -----------------------------------------------------------------------------
// POLARYZACJE
// -----------------------------------------------------------------------------
// Wejscia sa domyslnie jako styk zwierany do GND.
// true = stan aktywny gdy GPIO = LOW.
constexpr bool RADIATORS_ACTIVE_LOW = true;
constexpr bool FLOOR_ACTIVE_LOW = true;
constexpr bool CWU_ACTIVE_LOW = true;
// Zalecane polaczenie ALARM jako styk NC do GND:
// normalnie = LOW (brak alarmu), alarm lub przerwany przewod = HIGH (alarm).
// Jesli uzywasz styku NO, zmien na true.
constexpr bool ALARM_ACTIVE_LOW = false;

// BUF_OK: styk zamkniety do GND = temperatura OK.
// Przerwany przewod lub brak zasilania sterownika bufora = BUF_OK false.
constexpr bool BUF_OK_ACTIVE_LOW = true;

// Ustaw zgodnie z modulem przekaznikowym.
constexpr bool PUMP_HOME_ACTIVE_HIGH = false;
constexpr bool PUMP_SUPPLY_ACTIVE_HIGH = false;

// -----------------------------------------------------------------------------
// CZASY
// -----------------------------------------------------------------------------
constexpr uint32_t INPUT_DEBOUNCE_MS = 40;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 35;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 1800;
constexpr uint32_t OLED_REFRESH_MS = 250;
constexpr uint32_t SUPLA_PUBLISH_MS = 500;

// -----------------------------------------------------------------------------
// OLED
// -----------------------------------------------------------------------------
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t OLED_PAGE_COUNT = 3;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool displayReady = false;
uint8_t displayPage = 0;
uint32_t lastDisplayMs = 0;

// -----------------------------------------------------------------------------
// SUPLA
// -----------------------------------------------------------------------------
Supla::Eeprom eeprom;
Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;
Supla::Device::StatusLed statusLed(PIN_STATUS_LED, true);
Supla::EspWebServer suplaWebServer;

// Zdalne wymuszenie ON. OFF = brak zdalnego wymuszenia.
Supla::Control::VirtualRelay *suplaHomeForceOn = nullptr;
Supla::Control::VirtualRelay *suplaSupplyForceOn = nullptr;

// Stany wyjsciowe i wejscia jako 0/1.
Supla::Sensor::GeneralPurposeMeasurement *chPumpHomeState = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *chPumpSupplyState = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *chRadiators = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *chFloor = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *chCwu = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *chAlarm = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *chBufOk = nullptr;

uint32_t lastSuplaPublishMs = 0;

// -----------------------------------------------------------------------------
// NARZEDZIA
// -----------------------------------------------------------------------------
bool elapsed(uint32_t now, uint32_t since, uint32_t period) {
  return static_cast<uint32_t>(now - since) >= period;
}

void configureBoolChannel(Supla::Sensor::GeneralPurposeMeasurement *channel) {
  if (!channel) return;
  channel->setDefaultUnitAfterValue("");
  channel->setDefaultValuePrecision(0);
  channel->setDefaultRefreshIntervalMs(500);
}

// -----------------------------------------------------------------------------
// DEBOUNCED INPUT
// -----------------------------------------------------------------------------
class DebouncedInput {
 public:
  void begin(uint8_t pin, bool activeLow) {
    pin_ = pin;
    activeLow_ = activeLow;
    pinMode(pin_, INPUT_PULLUP);
    raw_ = digitalRead(pin_);
    stable_ = raw_;
    changedMs_ = millis();
  }

  void update(uint32_t now) {
    const bool current = digitalRead(pin_);
    if (current != raw_) {
      raw_ = current;
      changedMs_ = now;
    }

    if (raw_ != stable_ && elapsed(now, changedMs_, INPUT_DEBOUNCE_MS)) {
      stable_ = raw_;
    }
  }

  bool active() const {
    return activeLow_ ? (stable_ == LOW) : (stable_ == HIGH);
  }

 private:
  uint8_t pin_ = 0;
  bool activeLow_ = true;
  bool raw_ = HIGH;
  bool stable_ = HIGH;
  uint32_t changedMs_ = 0;
};

DebouncedInput inRadiators;
DebouncedInput inFloor;
DebouncedInput inCwu;
DebouncedInput inAlarm;
DebouncedInput inBufOk;

// -----------------------------------------------------------------------------
// PRZYCISKI
// -----------------------------------------------------------------------------
enum class ButtonEvent : uint8_t {
  None,
  ShortPress,
  LongPress,
};

class MomentaryButton {
 public:
  void begin(uint8_t pin) {
    pin_ = pin;
    pinMode(pin_, INPUT_PULLUP);
    raw_ = digitalRead(pin_);
    stable_ = raw_;
    changedMs_ = millis();
  }

  ButtonEvent update(uint32_t now) {
    const bool current = digitalRead(pin_);
    if (current != raw_) {
      raw_ = current;
      changedMs_ = now;
    }

    ButtonEvent event = ButtonEvent::None;

    if (raw_ != stable_ && elapsed(now, changedMs_, BUTTON_DEBOUNCE_MS)) {
      stable_ = raw_;

      if (stable_ == LOW) {
        pressedMs_ = now;
        longHandled_ = false;
      } else {
        if (!longHandled_) {
          event = ButtonEvent::ShortPress;
        }
      }
    }

    if (stable_ == LOW && !longHandled_ &&
        elapsed(now, pressedMs_, BUTTON_LONG_PRESS_MS)) {
      longHandled_ = true;
      event = ButtonEvent::LongPress;
    }

    return event;
  }

 private:
  uint8_t pin_ = 0;
  bool raw_ = HIGH;
  bool stable_ = HIGH;
  uint32_t changedMs_ = 0;
  uint32_t pressedMs_ = 0;
  bool longHandled_ = false;
};

MomentaryButton btnScreen;
MomentaryButton btnHome;
MomentaryButton btnSupply;

// -----------------------------------------------------------------------------
// TRYBY RECZNE
// -----------------------------------------------------------------------------
enum class ManualMode : uint8_t {
  Auto = 0,
  ForceOn = 1,
  ForceOff = 2,
};

ManualMode homeManualMode = ManualMode::Auto;
ManualMode supplyManualMode = ManualMode::Auto;

ManualMode nextManualMode(ManualMode mode) {
  switch (mode) {
    case ManualMode::Auto:
      return ManualMode::ForceOn;
    case ManualMode::ForceOn:
      return ManualMode::ForceOff;
    case ManualMode::ForceOff:
    default:
      return ManualMode::Auto;
  }
}

const char *manualModeShort(ManualMode mode) {
  switch (mode) {
    case ManualMode::ForceOn:
      return "M+";
    case ManualMode::ForceOff:
      return "M-";
    case ManualMode::Auto:
    default:
      return "A";
  }
}

// -----------------------------------------------------------------------------
// STAN STEROWNIKA
// -----------------------------------------------------------------------------
struct InputState {
  bool radiators = false;
  bool floor = false;
  bool cwu = false;
  bool alarm = false;
  bool bufOk = false;
};

InputState inputs;
bool pumpHomeOn = false;
bool pumpSupplyOn = false;

bool remoteHomeForceOn() {
  return suplaHomeForceOn && suplaHomeForceOn->isOn();
}

bool remoteSupplyForceOn() {
  return suplaSupplyForceOn && suplaSupplyForceOn->isOn();
}

// -----------------------------------------------------------------------------
// WYJSCIA
// -----------------------------------------------------------------------------
void writePumpPin(uint8_t pin, bool on, bool activeHigh) {
  const bool physicalHigh = activeHigh ? on : !on;
  digitalWrite(pin, physicalHigh ? HIGH : LOW);
}

void setPumpHome(bool on) {
  pumpHomeOn = on;
  writePumpPin(PIN_PUMP_HOME, on, PUMP_HOME_ACTIVE_HIGH);
}

void setPumpSupply(bool on) {
  pumpSupplyOn = on;
  writePumpPin(PIN_PUMP_SUPPLY, on, PUMP_SUPPLY_ACTIVE_HIGH);
}

// -----------------------------------------------------------------------------
// LOGIKA STEROWANIA
// -----------------------------------------------------------------------------
bool resolvePump(ManualMode localMode,
                 bool remoteForceOn,
                 bool automaticRequest) {
  // Lokalny tryb reczny ma pierwszenstwo przed zdalnym SUPLA.
  if (localMode == ManualMode::ForceOn) return true;
  if (localMode == ManualMode::ForceOff) return false;

  // SUPLA: ON = zdalne wymuszenie ON, OFF = brak wymuszenia.
  if (remoteForceOn) return true;

  return automaticRequest;
}

void readInputs(uint32_t now) {
  inRadiators.update(now);
  inFloor.update(now);
  inCwu.update(now);
  inAlarm.update(now);
  inBufOk.update(now);

  inputs.radiators = inRadiators.active();
  inputs.floor = inFloor.active();
  inputs.cwu = inCwu.active();
  inputs.alarm = inAlarm.active();
  inputs.bufOk = inBufOk.active();
}

void runPumpLogic() {
  // PRIORYTET 1: ALARM
  if (inputs.alarm) {
    setPumpHome(true);
    setPumpSupply(true);
    return;
  }

  // PRIORYTET 2: BRAK ZEZWOLENIA Z BUFORA
  if (!inputs.bufOk) {
    setPumpHome(false);
    setPumpSupply(false);
    return;
  }

  // PRIORYTET 3 + 4: reczne/SUPLA, potem automatyka
  const bool homeAutomaticRequest = inputs.radiators;
  const bool supplyAutomaticRequest =
      inputs.radiators || inputs.floor || inputs.cwu;

  setPumpHome(resolvePump(homeManualMode,
                          remoteHomeForceOn(),
                          homeAutomaticRequest));

  setPumpSupply(resolvePump(supplyManualMode,
                            remoteSupplyForceOn(),
                            supplyAutomaticRequest));
}

// -----------------------------------------------------------------------------
// PRZYCISKI - OBSLUGA
// -----------------------------------------------------------------------------
void handleButtons(uint32_t now) {
  const ButtonEvent screenEvent = btnScreen.update(now);
  const ButtonEvent homeEvent = btnHome.update(now);
  const ButtonEvent supplyEvent = btnSupply.update(now);

  if (screenEvent == ButtonEvent::ShortPress) {
    displayPage = (displayPage + 1) % OLED_PAGE_COUNT;
  }

  if (homeEvent == ButtonEvent::ShortPress) {
    homeManualMode = nextManualMode(homeManualMode);
  } else if (homeEvent == ButtonEvent::LongPress) {
    homeManualMode = ManualMode::ForceOn;
    supplyManualMode = ManualMode::ForceOn;
  }

  if (supplyEvent == ButtonEvent::ShortPress) {
    supplyManualMode = nextManualMode(supplyManualMode);
  } else if (supplyEvent == ButtonEvent::LongPress) {
    homeManualMode = ManualMode::ForceOn;
    supplyManualMode = ManualMode::ForceOn;
  }
}

// -----------------------------------------------------------------------------
// OPIS POWODU STANU POMPY - OLED
// -----------------------------------------------------------------------------
const char *homeReason() {
  if (inputs.alarm) return "ALARM";
  if (!inputs.bufOk) return "BUF";
  if (homeManualMode == ManualMode::ForceOn) return "MAN+";
  if (homeManualMode == ManualMode::ForceOff) return "MAN-";
  if (remoteHomeForceOn()) return "SUPLA";
  if (inputs.radiators) return "GRZ";
  return "AUTO";
}

const char *supplyReason() {
  if (inputs.alarm) return "ALARM";
  if (!inputs.bufOk) return "BUF";
  if (supplyManualMode == ManualMode::ForceOn) return "MAN+";
  if (supplyManualMode == ManualMode::ForceOff) return "MAN-";
  if (remoteSupplyForceOn()) return "SUPLA";
  if (inputs.radiators) return "GRZ";
  if (inputs.floor) return "POD";
  if (inputs.cwu) return "CWU";
  return "AUTO";
}

// -----------------------------------------------------------------------------
// OLED
// -----------------------------------------------------------------------------
void drawHeader(const char *title) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);
  display.setCursor(98, 0);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "OFF");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void drawPagePumps() {
  drawHeader("POMPY");

  display.setCursor(0, 17);
  display.print("DOM: ");
  display.print(pumpHomeOn ? "ON " : "OFF");
  display.setCursor(70, 17);
  display.print(homeReason());

  display.setCursor(0, 31);
  display.print("ZAS: ");
  display.print(pumpSupplyOn ? "ON " : "OFF");
  display.setCursor(70, 31);
  display.print(supplyReason());

  display.setCursor(0, 47);
  display.print("BUF:");
  display.print(inputs.bufOk ? "OK " : "BLOK");
  display.setCursor(72, 47);
  display.print("AL:");
  display.print(inputs.alarm ? "ON" : "OFF");
}

void drawPageInputs() {
  drawHeader("WEJSCIA");

  display.setCursor(0, 15);
  display.printf("GRZ:%s  POD:%s", inputs.radiators ? "ON " : "OFF",
                 inputs.floor ? "ON" : "OFF");

  display.setCursor(0, 29);
  display.printf("CWU:%s  AL:%s", inputs.cwu ? "ON " : "OFF",
                 inputs.alarm ? "ON" : "OFF");

  display.setCursor(0, 43);
  display.print("BUF_OK: ");
  display.print(inputs.bufOk ? "ON" : "OFF");

  display.setCursor(0, 55);
  display.print("AUTO: GRZ/POD/CWU");
}

void drawPageManual() {
  drawHeader("RECZNE / SUPLA");

  display.setCursor(0, 16);
  display.print("DOM lokal: ");
  display.print(manualModeShort(homeManualMode));

  display.setCursor(0, 29);
  display.print("ZAS lokal: ");
  display.print(manualModeShort(supplyManualMode));

  display.setCursor(0, 42);
  display.print("S DOM: ");
  display.print(remoteHomeForceOn() ? "ON" : "OFF");

  display.setCursor(67, 42);
  display.print("ZAS:");
  display.print(remoteSupplyForceOn() ? "ON" : "OFF");

  display.setCursor(0, 55);
  display.print("HOLD=OBIE MAN ON");
}

void drawDisplay() {
  if (!displayReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  switch (displayPage) {
    case 0:
      drawPagePumps();
      break;
    case 1:
      drawPageInputs();
      break;
    default:
      drawPageManual();
      break;
  }

  display.display();
}

// -----------------------------------------------------------------------------
// SUPLA - KANALY
// -----------------------------------------------------------------------------
void createSuplaChannels() {
  // Kanal 0: zdalne wymuszenie pompy domowej ON.
  suplaHomeForceOn = new Supla::Control::VirtualRelay;
  suplaHomeForceOn->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  // Kanal 1: zdalne wymuszenie pompy zasilajacej ON.
  suplaSupplyForceOn = new Supla::Control::VirtualRelay;
  suplaSupplyForceOn->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  // Kanaly 2-8: stany 0/1.
  chPumpHomeState = new Supla::Sensor::GeneralPurposeMeasurement;
  configureBoolChannel(chPumpHomeState);

  chPumpSupplyState = new Supla::Sensor::GeneralPurposeMeasurement;
  configureBoolChannel(chPumpSupplyState);

  chRadiators = new Supla::Sensor::GeneralPurposeMeasurement;
  configureBoolChannel(chRadiators);

  chFloor = new Supla::Sensor::GeneralPurposeMeasurement;
  configureBoolChannel(chFloor);

  chCwu = new Supla::Sensor::GeneralPurposeMeasurement;
  configureBoolChannel(chCwu);

  chAlarm = new Supla::Sensor::GeneralPurposeMeasurement;
  configureBoolChannel(chAlarm);

  chBufOk = new Supla::Sensor::GeneralPurposeMeasurement;
  configureBoolChannel(chBufOk);
}

void publishSuplaState(uint32_t now) {
  if (!elapsed(now, lastSuplaPublishMs, SUPLA_PUBLISH_MS)) return;
  lastSuplaPublishMs = now;

  if (chPumpHomeState) chPumpHomeState->setValue(pumpHomeOn ? 1.0 : 0.0);
  if (chPumpSupplyState) chPumpSupplyState->setValue(pumpSupplyOn ? 1.0 : 0.0);
  if (chRadiators) chRadiators->setValue(inputs.radiators ? 1.0 : 0.0);
  if (chFloor) chFloor->setValue(inputs.floor ? 1.0 : 0.0);
  if (chCwu) chCwu->setValue(inputs.cwu ? 1.0 : 0.0);
  if (chAlarm) chAlarm->setValue(inputs.alarm ? 1.0 : 0.0);
  if (chBufOk) chBufOk->setValue(inputs.bufOk ? 1.0 : 0.0);
}

// -----------------------------------------------------------------------------
// SETUP / LOOP
// -----------------------------------------------------------------------------
void setupOutputsSafe() {
  // Najpierw wymus stan OFF, dopiero potem OUTPUT.
  digitalWrite(PIN_PUMP_HOME, PUMP_HOME_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(PIN_PUMP_SUPPLY, PUMP_SUPPLY_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_PUMP_HOME, OUTPUT);
  pinMode(PIN_PUMP_SUPPLY, OUTPUT);
  setPumpHome(false);
  setPumpSupply(false);
}

void setupInputs() {
  inRadiators.begin(PIN_IN_RADIATORS, RADIATORS_ACTIVE_LOW);
  inFloor.begin(PIN_IN_FLOOR, FLOOR_ACTIVE_LOW);
  inCwu.begin(PIN_IN_CWU, CWU_ACTIVE_LOW);
  inAlarm.begin(PIN_IN_ALARM, ALARM_ACTIVE_LOW);
  inBufOk.begin(PIN_IN_BUF_OK, BUF_OK_ACTIVE_LOW);

  btnScreen.begin(PIN_BTN_SCREEN);
  btnHome.begin(PIN_BTN_HOME);
  btnSupply.begin(PIN_BTN_SUPPLY);
}

void setupSupla() {
  // Strony lokalnego portalu konfiguracji SUPLA.
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  createSuplaChannels();

  auto configButton = new Supla::Control::Button(PIN_CONFIG_BUTTON, true, true);
  configButton->configureAsConfigButton(&SuplaDevice);

  eeprom.setStateSavePeriod(5000);
  SuplaDevice.setName("Sterownik pomp CO");
  SuplaDevice.setSwVersion("1.1.0-PIO");
  SuplaDevice.setCustomHostnamePrefix("SUPLA-POMPY");
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin(23);
}

void setupDisplay() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);

  displayReady = display.begin(SSD1306_SWITCHCAPVCC,
                               OLED_ADDRESS,
                               true,
                               false);
  if (!displayReady) {
    Serial.println("OLED 0x3C nie odpowiada - sterownik pracuje bez wyswietlacza");
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("STEROWNIK POMP");
  display.println("ESP32 + SUPLA");
  display.println("Start...");
  display.display();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("Sterownik pomp ESP32 + SUPLA - start");

  setupOutputsSafe();
  setupInputs();
  setupDisplay();
  setupSupla();

  const uint32_t now = millis();
  readInputs(now);
  runPumpLogic();
  drawDisplay();
}

void loop() {
  SuplaDevice.iterate();

  const uint32_t now = millis();

  readInputs(now);
  handleButtons(now);
  runPumpLogic();
  publishSuplaState(now);

  if (elapsed(now, lastDisplayMs, OLED_REFRESH_MS)) {
    lastDisplayMs = now;
    drawDisplay();
  }
}
