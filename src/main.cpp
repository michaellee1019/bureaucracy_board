#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_seesaw.h>
#include <ArduinoJson.h>
#include "sequences.h"

#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, y) Serial.printf(x, y)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, y)
#endif

// Hardware configuration
#define LED_PIN 2        // D1 (GPIO2) - NeoPixels
#define BUTTON_PIN 1     // D0 (GPIO1) - Physical button
#define NUM_LEDS 21
#define GRID_COLS 7
#define GRID_ROWS 3

// I2C pins — adjust if your wiring differs
#define I2C_SDA 5   // XIAO ESP32-S3 D4 / GPIO5
#define I2C_SCL 6   // XIAO ESP32-S3 D5 / GPIO6

// Rotary encoder (Adafruit I2C STEMMA QT)
#define ENCODER_ADDR 0x36
#define ENCODER_SWITCH 24

// NeoPixel strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Rotary encoder
Adafruit_seesaw encoder;
bool encoderFound = false;
int32_t encoderPosition = 0;

// Encoder button debounce
bool lastEncoderBtnState = true;
unsigned long lastEncoderBtnTime = 0;
const unsigned long encoderBtnDebounceMs = 200;

// Physical button — RTOS-based debouncing
QueueHandle_t buttonQueue;
volatile bool buttonPressedFlag = false;
const unsigned long debounceDelay = 50;

// Application state machine
enum AppState { SELECTING, PLAYER_SELECT, SEQUENCE_RUNNING };
AppState appState = SELECTING;

// Game selection
int currentGameIndex = 0;

// Player/selector selection
int currentSelectorIndex = 0;
int32_t selectorEncoderBase = 0;  // encoder position when entering PLAYER_SELECT

// Sequence playback
DynamicJsonDocument gameDoc(8192);
JsonArray gameFrames;
int currentFrameIndex = 0;
int totalFrames = 0;

// ---------------------------------------------------------------------------
// Utility functions
// ---------------------------------------------------------------------------

uint32_t parseHexColor(const char* hexColor) {
    if (hexColor[0] == '#') hexColor++;
    else if (hexColor[0] == '0' && (hexColor[1] == 'x' || hexColor[1] == 'X')) hexColor += 2;
    return strtoul(hexColor, NULL, 16);
}

void colorToRGB(uint32_t color, uint8_t &r, uint8_t &g, uint8_t &b) {
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
}

int gridToIndex(const char* gridPos) {
    if (strlen(gridPos) < 2) return -1;
    int colIndex = toupper(gridPos[0]) - 'A';
    int rowIndex = gridPos[1] - '1';
    if (colIndex < 0 || colIndex >= GRID_COLS) return -1;
    if (rowIndex < 0 || rowIndex >= GRID_ROWS) return -1;
    return rowIndex * GRID_COLS + colIndex;
}

int computeWrappedIndex(int32_t position, int count) {
    int idx = (int)(position % count);
    if (idx < 0) idx += count;
    return idx;
}

// ---------------------------------------------------------------------------
// Encoder helpers
// ---------------------------------------------------------------------------

bool readEncoderButton() {
    if (!encoderFound) return false;
    bool currentState = encoder.digitalRead(ENCODER_SWITCH);
    bool pressed = false;

    if (!currentState && lastEncoderBtnState &&
        (millis() - lastEncoderBtnTime > encoderBtnDebounceMs)) {
        pressed = true;
        lastEncoderBtnTime = millis();
    }
    lastEncoderBtnState = currentState;
    return pressed;
}

// ---------------------------------------------------------------------------
// Physical button — ISR + FreeRTOS debounce task
// ---------------------------------------------------------------------------

void IRAM_ATTR buttonISR() {
    buttonPressedFlag = true;
}

void buttonDebounceTask(void *parameter) {
    bool lastState = HIGH;

    while (true) {
        if (buttonPressedFlag) {
            buttonPressedFlag = false;
            vTaskDelay(pdMS_TO_TICKS(debounceDelay));

            bool currentState = digitalRead(BUTTON_PIN);

            if (lastState == LOW && currentState == HIGH) {
                DEBUG_PRINTLN("Button released (debounced)");
                uint8_t evt = 1;
                xQueueSend(buttonQueue, &evt, 0);
            }
            lastState = currentState;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void displayFrame(JsonObject frame) {
    strip.clear();
    for (JsonPair kv : frame) {
        const char* gridPos = kv.key().c_str();
        JsonVariant colorValue = kv.value();
        int ledIndex = gridToIndex(gridPos);
        if (ledIndex < 0 || ledIndex >= NUM_LEDS) continue;

        uint8_t r, g, b;
        if (colorValue.is<const char*>()) {
            uint32_t color = parseHexColor(colorValue.as<const char*>());
            colorToRGB(color, r, g, b);
        } else if (colorValue.is<JsonArray>()) {
            JsonArray arr = colorValue.as<JsonArray>();
            if (arr.size() < 3) continue;
            r = arr[0]; g = arr[1]; b = arr[2];
        } else {
            continue;
        }
        strip.setPixelColor(ledIndex, strip.Color(r, g, b));
    }
    strip.show();
}

void showGameIndicator(int gameIndex) {
    strip.clear();
    if (gameIndex >= 0 && gameIndex < NUM_GAMES) {
        int ledIndex = gridToIndex(GAMES[gameIndex].gridPos);
        if (ledIndex >= 0) {
            strip.setPixelColor(ledIndex, strip.Color(255, 255, 255));
        }
    }
    strip.show();
}

void showSelectorIndicator(int gameIndex, int selectorIndex) {
    const GameDef& game = GAMES[gameIndex];
    if (selectorIndex < 0 || selectorIndex >= game.numSelectors) return;

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, game.selectors[selectorIndex].indicator);
    if (error) return;
    displayFrame(doc.as<JsonObject>());
}

bool loadSequence(const char* sequenceJson) {
    gameDoc.clear();
    DeserializationError error = deserializeJson(gameDoc, sequenceJson);
    if (error) {
        DEBUG_PRINT("Failed to parse sequence: ");
        DEBUG_PRINTLN(error.c_str());
        return false;
    }

    gameFrames = gameDoc.as<JsonArray>();
    totalFrames = gameFrames.size();
    currentFrameIndex = 0;
    return totalFrames > 0;
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

void enterPlayerSelect() {
    appState = PLAYER_SELECT;
    currentSelectorIndex = 0;
    selectorEncoderBase = encoderPosition;
    showSelectorIndicator(currentGameIndex, currentSelectorIndex);
    DEBUG_PRINT("PLAYER_SELECT for game ");
    DEBUG_PRINTLN(GAMES[currentGameIndex].gridPos);
}

void enterSequenceRunning() {
    const SelectorDef& sel = GAMES[currentGameIndex].selectors[currentSelectorIndex];
    if (loadSequence(sel.sequence)) {
        appState = SEQUENCE_RUNNING;
        displayFrame(gameFrames[0].as<JsonObject>());
        DEBUG_PRINT("SEQUENCE_RUNNING: selector ");
        DEBUG_PRINTLN(currentSelectorIndex);
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    #ifdef DEBUG
    Serial.begin(115200);
    delay(2000);
    #endif

    DEBUG_PRINTLN("\n=== Bureaucracy Board ===");

    // Physical button setup (RTOS)
    buttonQueue = xQueueCreate(5, sizeof(uint8_t));
    if (buttonQueue == NULL) {
        DEBUG_PRINTLN("ERROR: Failed to create button queue!");
    }

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    xTaskCreate(
        buttonDebounceTask,
        "ButtonDebounce",
        2048,
        NULL,
        1,
        NULL
    );
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE);
    DEBUG_PRINTLN("Physical button initialized");

    // NeoPixels
    strip.begin();
    strip.setBrightness(255);
    strip.clear();
    strip.show();
    DEBUG_PRINTLN("NeoPixels initialized");

    // I2C rotary encoder
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(100);

    #ifdef DEBUG
    DEBUG_PRINTLN("Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            DEBUG_PRINT("  Found device at 0x");
            DEBUG_PRINTLN(String(addr, HEX));
        }
    }
    #endif

    if (encoder.begin(ENCODER_ADDR)) {
        encoderFound = true;
        encoder.pinMode(ENCODER_SWITCH, INPUT_PULLUP);
        encoderPosition = encoder.getEncoderPosition();
        currentGameIndex = computeWrappedIndex(encoderPosition, NUM_GAMES);
        showGameIndicator(currentGameIndex);
        DEBUG_PRINT("Encoder OK — selecting game ");
        DEBUG_PRINTLN(GAMES[currentGameIndex].gridPos);
    } else {
        DEBUG_PRINTLN("ERROR: Rotary encoder not found at 0x36!");
        DEBUG_PRINTLN("Check wiring: SDA->D4(GPIO5), SCL->D5(GPIO6)");
        for (int i = 0; i < 5; i++) {
            strip.setPixelColor(0, strip.Color(255, 0, 0));
            strip.show();
            delay(200);
            strip.clear();
            strip.show();
            delay(200);
        }
    }
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void loop() {
    // --- Read encoder inputs ---
    bool encoderTurned = false;
    bool encoderBtnPressed = false;

    if (encoderFound) {
        int32_t newPosition = encoder.getEncoderPosition();
        encoderTurned = (newPosition != encoderPosition);
        if (encoderTurned) encoderPosition = newPosition;
        encoderBtnPressed = readEncoderButton();
    }

    // --- Read physical button ---
    uint8_t buttonEvent;
    bool physicalBtnPressed = (xQueueReceive(buttonQueue, &buttonEvent, 0) == pdTRUE);

    // ===================== SELECTING =====================
    // Encoder turn: cycle games (white LED). Encoder press: confirm → PLAYER_SELECT.
    if (appState == SELECTING) {

        if (encoderTurned) {
            int newIdx = computeWrappedIndex(encoderPosition, NUM_GAMES);
            if (newIdx != currentGameIndex) {
                currentGameIndex = newIdx;
                DEBUG_PRINT("Game: ");
                DEBUG_PRINTLN(GAMES[currentGameIndex].gridPos);
            }
            showGameIndicator(currentGameIndex);
        }

        if (encoderBtnPressed) {
            enterPlayerSelect();
        }
    }

    // ===================== PLAYER_SELECT =====================
    // Encoder turn: cycle selectors (indicator LEDs). Encoder press: confirm → SEQUENCE_RUNNING.
    else if (appState == PLAYER_SELECT) {
        int numSelectors = GAMES[currentGameIndex].numSelectors;

        if (encoderTurned) {
            int32_t delta = encoderPosition - selectorEncoderBase;
            int newIdx = computeWrappedIndex(delta, numSelectors);
            if (newIdx != currentSelectorIndex) {
                currentSelectorIndex = newIdx;
                showSelectorIndicator(currentGameIndex, currentSelectorIndex);
                DEBUG_PRINT("Selector: ");
                DEBUG_PRINTLN(currentSelectorIndex);
            }
        }

        if (encoderBtnPressed) {
            enterSequenceRunning();
        }
    }

    // ===================== SEQUENCE_RUNNING =====================
    // Physical button: advance frame. After last frame → back to PLAYER_SELECT.
    else if (appState == SEQUENCE_RUNNING) {

        if (physicalBtnPressed && totalFrames > 0) {
            currentFrameIndex++;

            if (currentFrameIndex >= totalFrames) {
                DEBUG_PRINTLN("Sequence complete → PLAYER_SELECT");
                enterPlayerSelect();
            } else {
                DEBUG_PRINT("Frame ");
                DEBUG_PRINT(currentFrameIndex + 1);
                DEBUG_PRINT("/");
                DEBUG_PRINTLN(totalFrames);
                displayFrame(gameFrames[currentFrameIndex].as<JsonObject>());
            }
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
}
