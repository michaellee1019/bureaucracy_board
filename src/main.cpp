#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include "sequences.h"

// Debug macros - only print when DEBUG is defined
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
#define BUTTON_PIN 1     // D0 (GPIO1) - Button
#define NUM_LEDS 21

// Grid configuration: 7 columns (A-G) × 3 rows (1-3)
#define GRID_COLS 7
#define GRID_ROWS 3

// NeoPixel strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Button state - RTOS based with task debouncing
QueueHandle_t buttonQueue;
volatile bool buttonPressedFlag = false;  // Flag set by ISR
const unsigned long debounceDelay = 50;   // 50ms debounce delay

// Sequence state
DynamicJsonDocument doc(4096);  // 4KB buffer for JSON
JsonArray sequences;
int currentSequenceIndex = -1;  // Start at -1 for blank state
int totalSequences = 0;

// Parse hex color code (e.g., "#FFFFFF" or "0xFFFFFF") to RGB
uint32_t parseHexColor(const char* hexColor) {
    // Handle "#RRGGBB" format
    if (hexColor[0] == '#') {
        hexColor++;  // Skip the '#'
    }
    // Handle "0x" prefix
    else if (hexColor[0] == '0' && (hexColor[1] == 'x' || hexColor[1] == 'X')) {
        hexColor += 2;  // Skip "0x"
    }
    
    // Parse the hex string
    uint32_t color = strtoul(hexColor, NULL, 16);
    return color;
}

// Extract RGB components from 24-bit color value
void colorToRGB(uint32_t color, uint8_t &r, uint8_t &g, uint8_t &b) {
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
}

// Convert grid coordinate (e.g., "A1", "B2", "G3") to LED index
int gridToIndex(const char* gridPos) {
    if (strlen(gridPos) < 2) return -1;
    
    char col = gridPos[0];
    char row = gridPos[1];
    
    // Column: A=0, B=1, ..., G=6
    int colIndex = toupper(col) - 'A';
    // Row: 1=0, 2=1, 3=2
    int rowIndex = row - '1';
    
    // Validate
    if (colIndex < 0 || colIndex >= GRID_COLS) return -1;
    if (rowIndex < 0 || rowIndex >= GRID_ROWS) return -1;
    
    // Calculate linear index (row-major order)
    return rowIndex * GRID_COLS + colIndex;
}

// Load sequences from configuration file
bool loadSequences() {
    DEBUG_PRINTLN("Parsing JSON configuration from sequences.h...");
    
    DeserializationError error = deserializeJson(doc, SEQUENCES_JSON);
    
    if (error) {
        DEBUG_PRINT("ERROR: Failed to parse JSON: ");
        DEBUG_PRINTLN(error.c_str());
        return false;
    }
    
    sequences = doc.as<JsonArray>();
    totalSequences = sequences.size();
    
    DEBUG_PRINT("SUCCESS: Loaded ");
    DEBUG_PRINT(totalSequences);
    DEBUG_PRINTLN(" sequences from sequences.h");
    
    return true;
}

// Display a specific sequence
void displaySequence(int index) {
    DEBUG_PRINT("\n=== displaySequence called with index ");
    DEBUG_PRINT(index);
    DEBUG_PRINT(" (totalSequences=");
    DEBUG_PRINT(totalSequences);
    DEBUG_PRINTLN(") ===");
    
    if (index < 0 || index >= totalSequences) {
        DEBUG_PRINTLN("ERROR: Index out of range!");
        return;
    }
    
    // Clear all LEDs first
    strip.clear();
    DEBUG_PRINTLN("LEDs cleared");
    
    // Get the sequence object
    JsonObject sequence = sequences[index];
    
    DEBUG_PRINT("Displaying sequence ");
    DEBUG_PRINT(index + 1);
    DEBUG_PRINT("/");
    DEBUG_PRINTLN(totalSequences);
    
    // Iterate through each grid position in the sequence
    for (JsonPair kv : sequence) {
        const char* gridPos = kv.key().c_str();
        JsonVariant colorValue = kv.value();
        
        int ledIndex = gridToIndex(gridPos);
        
        if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
            uint8_t r, g, b;
            
            // Check if color is a hex string or RGB array
            if (colorValue.is<const char*>()) {
                // Hex color string (e.g., "#FFFFFF")
                const char* hexColor = colorValue.as<const char*>();
                uint32_t color = parseHexColor(hexColor);
                colorToRGB(color, r, g, b);
                
                DEBUG_PRINT("  ");
                DEBUG_PRINT(gridPos);
                DEBUG_PRINT(" -> LED ");
                DEBUG_PRINT(ledIndex);
                DEBUG_PRINT(" = ");
                DEBUG_PRINT(hexColor);
                DEBUG_PRINT(" RGB(");
                DEBUG_PRINT(r);
                DEBUG_PRINT(",");
                DEBUG_PRINT(g);
                DEBUG_PRINT(",");
                DEBUG_PRINT(b);
                DEBUG_PRINTLN(")");
            } 
            else if (colorValue.is<JsonArray>()) {
                // RGB array format (backward compatibility)
                JsonArray colorArray = colorValue.as<JsonArray>();
                if (colorArray.size() >= 3) {
                    r = colorArray[0];
                    g = colorArray[1];
                    b = colorArray[2];
                    
                    DEBUG_PRINT("  ");
                    DEBUG_PRINT(gridPos);
                    DEBUG_PRINT(" -> LED ");
                    DEBUG_PRINT(ledIndex);
                    DEBUG_PRINT(" = RGB(");
                    DEBUG_PRINT(r);
                    DEBUG_PRINT(",");
                    DEBUG_PRINT(g);
                    DEBUG_PRINT(",");
                    DEBUG_PRINT(b);
                    DEBUG_PRINTLN(")");
                } else {
                    DEBUG_PRINT("  WARNING: Invalid RGB array for ");
                    DEBUG_PRINTLN(gridPos);
                    continue;
                }
            } else {
                DEBUG_PRINT("  WARNING: Invalid color format for ");
                DEBUG_PRINTLN(gridPos);
                continue;
            }
            
            strip.setPixelColor(ledIndex, strip.Color(r, g, b));
        } else {
            DEBUG_PRINT("  WARNING: Invalid LED index for ");
            DEBUG_PRINTLN(gridPos);
        }
    }
    
    DEBUG_PRINTLN("Calling strip.show()...");
    strip.show();
    DEBUG_PRINTLN("Sequence displayed!\n");
}

// ISR handler for button press - just set flag
void IRAM_ATTR buttonISR() {
    // Set flag for debounce task to handle
    buttonPressedFlag = true;
}

// FreeRTOS task for button debouncing
void buttonDebounceTask(void *parameter) {
    bool lastState = HIGH;
    
    while (true) {
        // Check if ISR flagged a button event
        if (buttonPressedFlag) {
            buttonPressedFlag = false;
            
            // Wait for debounce delay
            vTaskDelay(pdMS_TO_TICKS(debounceDelay));
            
            // Read stable state after debounce
            bool currentState = digitalRead(BUTTON_PIN);
            
            // Only trigger on LOW to HIGH transition (button release)
            if (lastState == LOW && currentState == HIGH) {
                DEBUG_PRINTLN("\n=== Button Released (Debounced) ===");
                
                // Send event to queue
                uint8_t buttonEvent = 1;
                xQueueSend(buttonQueue, &buttonEvent, 0);
            }
            
            lastState = currentState;
        }
        
        // Small delay to prevent task hogging CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Handle button events from queue
void handleButtonPress() {
    // Move to next sequence
    currentSequenceIndex++;
    
    // Check if we've gone past the last sequence
    if (currentSequenceIndex >= totalSequences) {
        // Show blank (all LEDs off) and reset to -1
        strip.clear();
        strip.show();
        currentSequenceIndex = -1;
        DEBUG_PRINTLN("Displaying blank state (end of sequences)");
    } else {
        // Display the current sequence
        displaySequence(currentSequenceIndex);
    }
}

void setup() {
    #ifdef DEBUG
    Serial.begin(115200);
    delay(2000);
    #endif
    
    DEBUG_PRINTLN("\n=== LED Sequence Controller (RTOS) ===");
    
    // Create queue for button events (queue size of 5)
    buttonQueue = xQueueCreate(5, sizeof(uint8_t));
    if (buttonQueue == NULL) {
        DEBUG_PRINTLN("ERROR: Failed to create button queue!");
    } else {
        DEBUG_PRINTLN("Button queue created successfully");
    }
    
    // Initialize button with pull-up
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Create button debounce task
    xTaskCreate(
        buttonDebounceTask,   // Task function
        "ButtonDebounce",     // Task name
        2048,                 // Stack size
        NULL,                 // Parameters
        1,                    // Priority
        NULL                  // Task handle
    );
    DEBUG_PRINTLN("Button debounce task created");
    
    // Attach interrupt to button pin (trigger on any change)
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE);
    DEBUG_PRINTLN("Button interrupt attached (CHANGE -> debounce task)");
    
    // Initialize NeoPixel strip
    strip.begin();
    strip.setBrightness(255);
    strip.clear();
    strip.show();
    DEBUG_PRINTLN("NeoPixels initialized");
    
    // Load sequences from embedded JSON
    if (loadSequences()) {
        DEBUG_PRINTLN("Ready! All LEDs off. Press button to start first sequence.");
        // Start with blank state (currentSequenceIndex is already -1)
        // Don't display anything - LEDs are already cleared above
    } else {
        DEBUG_PRINTLN("ERROR: Could not load sequences!");
        // Show error pattern (red blink on first LED)
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

void loop() {
    // Wait for button events from the queue
    uint8_t buttonEvent;
    
    // Block waiting for button press (with 10ms timeout to keep watchdog happy)
    if (xQueueReceive(buttonQueue, &buttonEvent, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Button event received!
        handleButtonPress();
    }
    
    // Small yield to other RTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1));
}
