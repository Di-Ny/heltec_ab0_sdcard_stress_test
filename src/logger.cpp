/**
 * @file logger.cpp
 * @brief Implémentation du module de logging série
 */

#include "logger.h"
#include <stdarg.h>

// =============================================================================
// CONSTANTES
// =============================================================================

// Préfixes de niveau (stockés en PROGMEM pour économiser la RAM)
static const char LEVEL_ERROR[] PROGMEM = "[ERR]";
static const char LEVEL_WARN[] PROGMEM  = "[WRN]";
static const char LEVEL_INFO[] PROGMEM  = "[INF]";
static const char LEVEL_DEBUG[] PROGMEM = "[DBG]";

// Messages d'erreur
static const char ERR_STR_NONE[] PROGMEM = "None";
static const char ERR_STR_SD_INIT[] PROGMEM = "SD init failed";
static const char ERR_STR_SD_MOUNT[] PROGMEM = "SD mount failed";
static const char ERR_STR_FILE_OPEN[] PROGMEM = "File open failed";
static const char ERR_STR_FILE_WRITE[] PROGMEM = "File write failed";
static const char ERR_STR_FILE_CLOSE[] PROGMEM = "File close failed";
static const char ERR_STR_SD_UNMOUNT[] PROGMEM = "SD unmount failed";
static const char ERR_STR_VEXT_TIMEOUT[] PROGMEM = "Vext timeout";
static const char ERR_STR_SPI_INIT[] PROGMEM = "SPI init failed";
static const char ERR_STR_SD_NOT_PRESENT[] PROGMEM = "SD not present";
static const char ERR_STR_CARD_TYPE[] PROGMEM = "Unknown card type";
static const char ERR_STR_FAT_VOLUME[] PROGMEM = "FAT volume failed";
static const char ERR_STR_BUFFER[] PROGMEM = "Buffer overflow";
static const char ERR_STR_UNKNOWN[] PROGMEM = "Unknown error";

// =============================================================================
// FONCTIONS PRIVÉES
// =============================================================================

/**
 * @brief Affiche le préfixe de niveau
 */
static void print_level_prefix(uint8_t level) {
    switch (level) {
        case LOG_LEVEL_ERROR:
            Serial.print((__FlashStringHelper*)LEVEL_ERROR);
            break;
        case LOG_LEVEL_WARN:
            Serial.print((__FlashStringHelper*)LEVEL_WARN);
            break;
        case LOG_LEVEL_INFO:
            Serial.print((__FlashStringHelper*)LEVEL_INFO);
            break;
        case LOG_LEVEL_DEBUG:
            Serial.print((__FlashStringHelper*)LEVEL_DEBUG);
            break;
    }
    Serial.print(' ');
}

// =============================================================================
// IMPLÉMENTATION API PUBLIQUE
// =============================================================================

void logger_init(void) {
    #if SERIAL_DEBUG
    Serial.begin(SERIAL_BAUD_RATE);

    // Attend que le port série soit prêt (avec timeout)
    uint32_t start = millis();
    while (!Serial && (millis() - start) < 3000) {
        delay(10);
    }

    // Petit délai supplémentaire pour stabilisation
    delay(100);
    #endif
}

void logger_print(uint8_t level, const __FlashStringHelper* format, ...) {
    #if SERIAL_DEBUG
    if (level > APP_LOG_LEVEL) return;

    // Timestamp
    Serial.print('[');
    Serial.print(millis());
    Serial.print(F("] "));

    // Préfixe niveau
    print_level_prefix(level);

    // Message formaté
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf_P(buffer, sizeof(buffer), (const char*)format, args);
    va_end(args);

    Serial.println(buffer);
    #endif
}

void logger_println(uint8_t level, const __FlashStringHelper* msg) {
    #if SERIAL_DEBUG
    if (level > APP_LOG_LEVEL) return;

    // Timestamp
    Serial.print('[');
    Serial.print(millis());
    Serial.print(F("] "));

    // Préfixe niveau
    print_level_prefix(level);

    // Message
    Serial.println(msg);
    #endif
}

void logger_print_stats(const test_stats_t* stats) {
    #if SERIAL_DEBUG
    logger_print_separator();
    Serial.println(F("=== TEST STATISTICS ==="));

    Serial.print(F("Total cycles: "));
    Serial.println(stats->total_cycles);

    Serial.print(F("Successful:   "));
    Serial.print(stats->successful_cycles);
    Serial.print(F(" ("));
    if (stats->total_cycles > 0) {
        Serial.print((stats->successful_cycles * 100) / stats->total_cycles);
    } else {
        Serial.print(0);
    }
    Serial.println(F("%)"));

    Serial.print(F("Failed:       "));
    Serial.println(stats->failed_cycles);

    Serial.print(F("Consecutive:  "));
    Serial.println(stats->consecutive_failures);

    Serial.println(F("--- Timing (us) ---"));
    Serial.print(F("Init min/avg/max: "));
    Serial.print(stats->min_init_time_us);
    Serial.print('/');
    if (stats->successful_cycles > 0) {
        Serial.print(stats->total_init_time_us / stats->successful_cycles);
    } else {
        Serial.print(0);
    }
    Serial.print('/');
    Serial.println(stats->max_init_time_us);

    Serial.print(F("Write min/avg/max: "));
    Serial.print(stats->min_write_time_us);
    Serial.print('/');
    if (stats->successful_cycles > 0) {
        Serial.print(stats->total_write_time_us / stats->successful_cycles);
    } else {
        Serial.print(0);
    }
    Serial.print('/');
    Serial.println(stats->max_write_time_us);

    Serial.print(F("SPI freq:     "));
    Serial.print(stats->current_spi_freq / 1000);
    Serial.println(F(" kHz"));

    Serial.print(F("SPI fallbacks: "));
    Serial.println(stats->spi_fallback_count);

    Serial.print(F("Last error:   "));
    Serial.println(logger_error_to_string(stats->last_error));

    logger_print_separator();
    #endif
}

void logger_print_cycle_result(uint32_t cycle, const cycle_result_t* result) {
    #if SERIAL_DEBUG && APP_LOG_LEVEL >= LOG_LEVEL_INFO
    Serial.print(F("["));
    Serial.print(millis());
    Serial.print(F("] Cycle "));
    Serial.print(cycle);
    Serial.print(F(": "));

    if (result->success) {
        Serial.print(F("OK"));
    } else {
        Serial.print(F("FAIL ("));
        Serial.print(logger_error_to_string(result->error_code));
        Serial.print(')');
    }

    Serial.print(F(" | Init: "));
    Serial.print(result->init_time_us);
    Serial.print(F("us | Write: "));
    Serial.print(result->write_time_us);
    Serial.print(F("us | SPI: "));
    Serial.print(result->spi_freq_used / 1000);
    Serial.println(F("kHz"));
    #endif
}

void logger_print_sd_info(const char* card_type, uint32_t size_mb) {
    #if SERIAL_DEBUG
    Serial.print(F("SD Card: "));
    Serial.print(card_type);
    Serial.print(F(" - "));
    Serial.print(size_mb);
    Serial.println(F(" MB"));
    #endif
}

void logger_print_banner(void) {
    #if SERIAL_DEBUG
    Serial.println();
    logger_print_separator();
    Serial.println(F("  SD CARD STRESS TEST  "));
    Serial.println(F("  HTCC-AB02 (CubeCell)  "));
    logger_print_separator();
    Serial.println();
    #endif
}

void logger_print_config(void) {
    #if SERIAL_DEBUG
    Serial.println(F("Configuration:"));
    Serial.print(F("  Mode: "));
    #if AGGRESSIVE_MODE
    Serial.println(F("AGGRESSIVE (unmount each cycle)"));
    #else
    Serial.println(F("CONTINUOUS (file stays open)"));
    #endif

    Serial.print(F("  Power cycle: "));
    #if POWER_CYCLE_ENABLED && AGGRESSIVE_MODE
    Serial.println(F("ENABLED"));
    #else
    Serial.println(F("DISABLED"));
    #endif

    Serial.print(F("  Cycle interval: "));
    Serial.print(CYCLE_INTERVAL_MS);
    Serial.println(F(" ms"));

    Serial.print(F("  SPI frequency: "));
    Serial.print(SD_SPI_FREQUENCY / 1000);
    Serial.println(F(" kHz"));

    Serial.print(F("  Max failures: "));
    Serial.println(MAX_CONSECUTIVE_FAILURES);

    Serial.print(F("  CSV file: "));
    Serial.println(F(CSV_FILENAME));

    Serial.print(F("  Log level: "));
    Serial.println(APP_LOG_LEVEL);

    Serial.println();
    #endif
}

const __FlashStringHelper* logger_error_to_string(sd_error_t error) {
    switch (error) {
        case ERR_NONE:
            return (__FlashStringHelper*)ERR_STR_NONE;
        case ERR_SD_INIT_FAILED:
            return (__FlashStringHelper*)ERR_STR_SD_INIT;
        case ERR_SD_MOUNT_FAILED:
            return (__FlashStringHelper*)ERR_STR_SD_MOUNT;
        case ERR_FILE_OPEN_FAILED:
            return (__FlashStringHelper*)ERR_STR_FILE_OPEN;
        case ERR_FILE_WRITE_FAILED:
            return (__FlashStringHelper*)ERR_STR_FILE_WRITE;
        case ERR_FILE_CLOSE_FAILED:
            return (__FlashStringHelper*)ERR_STR_FILE_CLOSE;
        case ERR_SD_UNMOUNT_FAILED:
            return (__FlashStringHelper*)ERR_STR_SD_UNMOUNT;
        case ERR_VEXT_TIMEOUT:
            return (__FlashStringHelper*)ERR_STR_VEXT_TIMEOUT;
        case ERR_SPI_INIT_FAILED:
            return (__FlashStringHelper*)ERR_STR_SPI_INIT;
        case ERR_SD_NOT_PRESENT:
            return (__FlashStringHelper*)ERR_STR_SD_NOT_PRESENT;
        case ERR_SD_CARD_TYPE_UNKNOWN:
            return (__FlashStringHelper*)ERR_STR_CARD_TYPE;
        case ERR_FAT_VOLUME_FAILED:
            return (__FlashStringHelper*)ERR_STR_FAT_VOLUME;
        case ERR_BUFFER_OVERFLOW:
            return (__FlashStringHelper*)ERR_STR_BUFFER;
        default:
            return (__FlashStringHelper*)ERR_STR_UNKNOWN;
    }
}

void logger_print_separator(void) {
    #if SERIAL_DEBUG
    Serial.println(F("================================"));
    #endif
}

void logger_flush(void) {
    #if SERIAL_DEBUG
    Serial.flush();
    #endif
}
