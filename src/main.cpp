#include <Arduino.h>

class Config {
public:
    static constexpr uint8_t RedLedPin           = 15;
    static constexpr uint8_t BlueLedPin          = 16;
    static constexpr uint8_t ButtonPin           = 4;

    static constexpr uint32_t BaudRate           = 115200;
    static constexpr uint32_t BlinkIntervalMs    = 500;
    static constexpr uint32_t DebounceMs         = 200;
    static constexpr uint32_t LoopReportInterval = 1000;
};

enum class LedState : uint8_t { Off, On };

class Led {
public:
    explicit Led(uint8_t pin) : pin_(pin) {}

    void init() const {
        pinMode(pin_, OUTPUT);
        set(LedState::Off);
    }

    void set(LedState state) const {
        digitalWrite(pin_, state == LedState::On ? HIGH : LOW);
    }

private:
    const uint8_t pin_;
};

static const Led redLed(Config::RedLedPin);
static const Led blueLed(Config::BlueLedPin);

namespace {
    volatile bool buttonPressed   = false;
    volatile uint32_t lastEdgeMs  = 0;
}

void IRAM_ATTR onButtonPressed() {
    const uint32_t now = millis();
    if (now - lastEdgeMs >= Config::DebounceMs) {
        buttonPressed = true;
        lastEdgeMs = now;
    }
}

enum class Mode : uint8_t { Blinking, AlwaysOn, AlwaysOff };

Mode nextMode(Mode mode) {
    switch (mode) {
        case Mode::Blinking:  return Mode::AlwaysOn;
        case Mode::AlwaysOn:  return Mode::AlwaysOff;
        case Mode::AlwaysOff: return Mode::Blinking;
    }
    return Mode::Blinking;
}

const char* modeName(Mode mode) {
    switch (mode) {
        case Mode::Blinking:  return "Blinking";
        case Mode::AlwaysOn:  return "AlwaysOn";
        case Mode::AlwaysOff: return "AlwaysOff";
    }
    return "Unknown";
}

enum class BlinkPhase : uint8_t { RedOn, RedOff, BlueOn, BlueOff };

BlinkPhase nextPhase(BlinkPhase phase) {
    switch (phase) {
        case BlinkPhase::RedOn:   return BlinkPhase::RedOff;
        case BlinkPhase::RedOff:  return BlinkPhase::BlueOn;
        case BlinkPhase::BlueOn:  return BlinkPhase::BlueOff;
        case BlinkPhase::BlueOff: return BlinkPhase::RedOn;
    }
    return BlinkPhase::RedOn;
}

void applyPhase(BlinkPhase phase) {
    switch (phase) {
        case BlinkPhase::RedOn:   redLed.set(LedState::On);  blueLed.set(LedState::Off); break;
        case BlinkPhase::RedOff:  redLed.set(LedState::Off); blueLed.set(LedState::Off); break;
        case BlinkPhase::BlueOn:  redLed.set(LedState::Off); blueLed.set(LedState::On);  break;
        case BlinkPhase::BlueOff: redLed.set(LedState::Off); blueLed.set(LedState::Off); break;
    }
}

void setup() {
    Serial.begin(Config::BaudRate);

    redLed.init();
    blueLed.init();

    pinMode(Config::ButtonPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Config::ButtonPin), onButtonPressed, FALLING);

    Serial.println("Start");
}

void loop() {
    static Mode mode              = Mode::Blinking;
    static BlinkPhase phase       = BlinkPhase::RedOn;
    static uint32_t lastPhaseMs   = millis();
    static uint32_t iterationCount = 0;
    static uint32_t accumulatedUs  = 0;

    const uint32_t iterationStartUs = micros();

    if (buttonPressed) {
        buttonPressed = false;
        mode = nextMode(mode);

        Serial.print("Mode changed: ");
        Serial.println(modeName(mode));

        if (mode == Mode::Blinking) {
            phase = BlinkPhase::RedOn;
            lastPhaseMs = millis();
            applyPhase(phase);
        } else {
            const LedState state = (mode == Mode::AlwaysOn) ? LedState::On : LedState::Off;
            redLed.set(state);
            blueLed.set(state);
        }
    }

    if (mode == Mode::Blinking) {
        const uint32_t now = millis();
        if (now - lastPhaseMs >= Config::BlinkIntervalMs) {
            lastPhaseMs = now;
            phase = nextPhase(phase);
            applyPhase(phase);
        }
    }

    accumulatedUs += micros() - iterationStartUs;
    ++iterationCount;
    if (iterationCount >= Config::LoopReportInterval) {
        Serial.print("Avg loop iteration time (us): ");
        Serial.println(accumulatedUs / iterationCount);
        iterationCount = 0;
        accumulatedUs = 0;
    }
}
