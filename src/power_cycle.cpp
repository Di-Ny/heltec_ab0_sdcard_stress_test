/**
 * @file power_cycle.cpp
 * @brief Implémentation de la gestion du power-cycle Vext
 */

#include "power_cycle.h"

// =============================================================================
// VARIABLES GLOBALES
// =============================================================================

static bool vext_is_on = false;
static void (*button_callback)(void) = nullptr;

// =============================================================================
// FONCTIONS D'INTERRUPTION
// =============================================================================

/**
 * @brief Handler d'interruption pour le bouton utilisateur
 * Note: IRAM_ATTR est spécifique ESP32, non supporté sur ASR6501
 */
static void button_isr(void) {
    if (button_callback != nullptr) {
        button_callback();
    }
}

// =============================================================================
// IMPLÉMENTATION API PUBLIQUE
// =============================================================================

void power_init(void) {
    // Configure Vext control pin
    pinMode(PIN_VEXT_CTRL, OUTPUT);

    // Configure LED pin
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);  // LED off

    // Active l'alimentation par défaut
    power_on();
}

void power_on(void) {
    // Vext: LOW = ON
    digitalWrite(PIN_VEXT_CTRL, LOW);
    vext_is_on = true;

    // Délai de stabilisation
    delay(VEXT_POWER_ON_DELAY_MS);
}

void power_off(void) {
    // Vext: HIGH = OFF
    digitalWrite(PIN_VEXT_CTRL, HIGH);
    vext_is_on = false;

    // Délai pour décharge des condensateurs
    delay(VEXT_POWER_OFF_DELAY_MS);
}

uint32_t power_cycle(void) {
    uint32_t start = millis();

    // Séquence power-cycle
    power_off();
    power_on();

    return millis() - start;
}

bool power_is_on(void) {
    return vext_is_on;
}

uint32_t power_get_cycle_duration_ms(void) {
    return VEXT_POWER_OFF_DELAY_MS + VEXT_POWER_ON_DELAY_MS;
}

void led_set(bool on) {
    digitalWrite(PIN_LED, on ? HIGH : LOW);
}

void led_blink(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; i++) {
        led_set(true);
        delay(on_ms);
        led_set(false);
        if (i < count - 1) {
            delay(off_ms);
        }
    }
}

void system_reboot(void) {
    #if SERIAL_DEBUG
    Serial.println(F("[POWER] System reboot requested"));
    Serial.flush();
    #endif

    // Indicateur visuel avant reboot
    led_blink(5, 100, 100);

    // Méthode de reboot pour ASR6501 (CubeCell)
    // Utilise le watchdog ou NVIC_SystemReset
    #ifdef ARDUINO_ARCH_ASR650X
    // CubeCell specific reset
    HW_Reset(0);
    #else
    // Fallback: infinite loop pour forcer watchdog reset
    while (1) {
        // Attend le watchdog
    }
    #endif
}

bool button_is_pressed(void) {
    // Le bouton est généralement actif LOW
    return digitalRead(PIN_USER_BUTTON) == LOW;
}

void button_init(void (*callback)(void)) {
    // Configure le pin en entrée avec pull-up
    pinMode(PIN_USER_BUTTON, INPUT_PULLUP);

    // Sauvegarde le callback
    button_callback = callback;

    // Configure l'interruption si callback fourni
    if (callback != nullptr) {
        attachInterrupt(digitalPinToInterrupt(PIN_USER_BUTTON), button_isr, FALLING);
    }
}
