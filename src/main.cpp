/**
 * @file main.cpp
 * @brief Programme principal de test de stress SD pour HTCC-AB01
 *
 * Ce programme effectue un test intensif de la carte SD:
 * - Power-cycle hardware via Vext (GPIO6)
 * - Réinitialisation de la carte SD à chaque cycle
 * - Écriture d'une ligne CSV en mode append
 * - Logging des performances et erreurs
 *
 * Modes de fonctionnement:
 * - AGGRESSIVE_MODE=1: unmount/power-cycle/mount à chaque cycle
 * - AGGRESSIVE_MODE=0: fichier reste ouvert, flush à chaque écriture
 */

#include <Arduino.h>
#include "config.h"
#include "sd_controller.h"
#include "power_cycle.h"
#include "logger.h"

// =============================================================================
// VARIABLES GLOBALES
// =============================================================================

static test_stats_t stats;
static volatile bool stop_requested = false;
static uint32_t last_cycle_time = 0;

// =============================================================================
// FONCTIONS PRIVÉES
// =============================================================================

/**
 * @brief Callback pour le bouton utilisateur (arrêt du test)
 */
static void button_press_handler(void) {
    stop_requested = true;
}

/**
 * @brief Initialise les statistiques
 */
static void init_stats(void) {
    memset(&stats, 0, sizeof(stats));
    stats.min_init_time_us = UINT32_MAX;
    stats.min_write_time_us = UINT32_MAX;
    stats.current_spi_freq = SD_SPI_FREQUENCY;
}

/**
 * @brief Met à jour les statistiques avec le résultat d'un cycle
 */
static void update_stats(const cycle_result_t* result) {
    stats.total_cycles++;

    if (result->success) {
        stats.successful_cycles++;
        stats.consecutive_failures = 0;

        // Timing init
        stats.total_init_time_us += result->init_time_us;
        if (result->init_time_us < stats.min_init_time_us) {
            stats.min_init_time_us = result->init_time_us;
        }
        if (result->init_time_us > stats.max_init_time_us) {
            stats.max_init_time_us = result->init_time_us;
        }

        // Timing write
        stats.total_write_time_us += result->write_time_us;
        if (result->write_time_us < stats.min_write_time_us) {
            stats.min_write_time_us = result->write_time_us;
        }
        if (result->write_time_us > stats.max_write_time_us) {
            stats.max_write_time_us = result->write_time_us;
        }
    } else {
        stats.failed_cycles++;
        stats.consecutive_failures++;
        stats.last_error = result->error_code;
    }

    stats.current_spi_freq = result->spi_freq_used;
}

/**
 * @brief Exécute un cycle de test en mode agressif
 *
 * Séquence:
 * 1. Power-cycle de la carte SD (si activé)
 * 2. Mount de la carte SD
 * 3. Écriture d'une ligne CSV
 * 4. Unmount de la carte SD
 */
static cycle_result_t run_aggressive_cycle(uint32_t cycle_num) {
    cycle_result_t result = {0};
    uint32_t timestamp = millis();

    #if POWER_CYCLE_ENABLED
    // Power-cycle hardware
    power_cycle();
    #endif

    // Tentatives de mount avec retry
    sd_error_t err = ERR_NONE;
    for (uint8_t retry = 0; retry < SD_OPERATION_RETRIES; retry++) {
        err = sd_mount(0);
        if (err == ERR_NONE) break;

        LOG_WARN("Mount retry %d/%d", retry + 1, SD_OPERATION_RETRIES);
        delay(SD_RETRY_DELAY_MS);

        #if SPI_FREQUENCY_FALLBACK
        // Réduit la fréquence si échec
        if (sd_reduce_frequency()) {
            stats.spi_fallback_count++;
            LOG_WARN("SPI fallback to %lu kHz", sd_get_current_frequency() / 1000);
        }
        #endif
    }

    result.init_time_us = sd_get_last_init_time_us();
    result.spi_freq_used = sd_get_current_frequency();

    if (err != ERR_NONE) {
        result.success = false;
        result.error_code = err;
        return result;
    }

    // Écriture CSV avec retry
    for (uint8_t retry = 0; retry < SD_OPERATION_RETRIES; retry++) {
        // Pour le premier essai, on utilise les vrais timings
        // Pour les retries, on garde le timing du premier essai
        cycle_result_t temp_result = result;
        temp_result.success = true;
        temp_result.error_code = ERR_NONE;

        err = sd_write_csv_line(cycle_num, &temp_result, timestamp);
        if (err == ERR_NONE) break;

        LOG_WARN("Write retry %d/%d", retry + 1, SD_OPERATION_RETRIES);
        delay(SD_RETRY_DELAY_MS);
    }

    result.write_time_us = sd_get_last_write_time_us();

    if (err != ERR_NONE) {
        result.success = false;
        result.error_code = err;
        sd_unmount();
        return result;
    }

    // Unmount
    err = sd_unmount();
    if (err != ERR_NONE) {
        result.success = false;
        result.error_code = err;
        return result;
    }

    result.success = true;
    result.error_code = ERR_NONE;
    return result;
}

/**
 * @brief Exécute un cycle de test en mode continu
 *
 * Le fichier reste ouvert entre les cycles.
 * Utilise sync() pour garantir l'écriture.
 */
static cycle_result_t run_continuous_cycle(uint32_t cycle_num) {
    cycle_result_t result = {0};
    uint32_t timestamp = millis();
    sd_error_t err;

    // Mount si pas déjà fait
    if (!sd_is_mounted()) {
        err = sd_mount(0);
        result.init_time_us = sd_get_last_init_time_us();
        result.spi_freq_used = sd_get_current_frequency();

        if (err != ERR_NONE) {
            result.success = false;
            result.error_code = err;
            return result;
        }
    } else {
        result.init_time_us = 0;  // Pas d'init dans ce cycle
        result.spi_freq_used = sd_get_current_frequency();
    }

    // Écriture CSV
    cycle_result_t temp_result = result;
    temp_result.success = true;
    temp_result.error_code = ERR_NONE;

    err = sd_write_csv_line(cycle_num, &temp_result, timestamp);
    result.write_time_us = sd_get_last_write_time_us();

    if (err != ERR_NONE) {
        result.success = false;
        result.error_code = err;
        return result;
    }

    result.success = true;
    result.error_code = ERR_NONE;
    return result;
}

/**
 * @brief Affiche les stats périodiquement
 */
static void periodic_stats_display(void) {
    // Affiche les stats toutes les 100 cycles ou 60 secondes
    static uint32_t last_stats_time = 0;
    static uint32_t last_stats_cycle = 0;

    if ((stats.total_cycles - last_stats_cycle >= 100) ||
        (millis() - last_stats_time >= 60000)) {
        logger_print_stats(&stats);
        last_stats_time = millis();
        last_stats_cycle = stats.total_cycles;
    }
}

// =============================================================================
// SETUP & LOOP
// =============================================================================

void setup() {
    // Initialisation du logging (doit être en premier)
    logger_init();
    logger_print_banner();

    // Initialisation du contrôle d'alimentation
    power_init();
    LOG_INFO_LN("Power control initialized");

    // Initialisation du bouton utilisateur
    button_init(button_press_handler);
    LOG_INFO_LN("User button initialized (press to stop)");

    // Affiche la configuration
    logger_print_config();

    // Initialisation du contrôleur SD
    if (!sd_controller_init()) {
        LOG_ERROR_LN("SD controller init failed!");
        led_blink(10, 100, 100);
        system_reboot();
    }
    LOG_INFO_LN("SD controller initialized");

    // Premier mount pour vérifier la carte
    LOG_INFO_LN("Mounting SD card...");
    sd_error_t err = sd_mount(0);
    if (err != ERR_NONE) {
        LOG_ERROR("Initial mount failed: %s", logger_error_to_string(err));
        led_blink(5, 200, 200);
        system_reboot();
    }

    // Affiche les infos de la carte
    char card_type[16];
    uint32_t card_size_mb;
    if (sd_get_card_info(card_type, &card_size_mb) == ERR_NONE) {
        logger_print_sd_info(card_type, card_size_mb);
    }

    // Démonte pour commencer proprement
    #if AGGRESSIVE_MODE
    sd_unmount();
    #endif

    // Initialise les statistiques
    init_stats();

    LOG_INFO_LN("Starting stress test...");
    logger_print_separator();

    // Indicateur LED de démarrage
    led_blink(3, 100, 100);
}

void loop() {
    // Vérifie si l'utilisateur veut arrêter
    if (stop_requested) {
        LOG_INFO_LN("Stop requested by user");
        logger_print_stats(&stats);
        sd_unmount();

        // LED fixe pour indiquer l'arrêt
        led_set(true);

        // Attend un nouvel appui pour reprendre
        while (button_is_pressed()) {
            delay(10);
        }
        delay(500);  // Debounce
        while (!button_is_pressed()) {
            delay(100);
        }

        // Redémarre
        stop_requested = false;
        led_set(false);
        LOG_INFO_LN("Resuming stress test...");
    }

    // Calcul du temps écoulé depuis le dernier cycle
    uint32_t now = millis();
    if (now - last_cycle_time < CYCLE_INTERVAL_MS) {
        // Pas encore temps pour un nouveau cycle
        delay(10);
        return;
    }
    last_cycle_time = now;

    // Exécute le cycle selon le mode
    cycle_result_t result;
    #if AGGRESSIVE_MODE
    result = run_aggressive_cycle(stats.total_cycles + 1);
    #else
    result = run_continuous_cycle(stats.total_cycles + 1);
    #endif

    // Met à jour les statistiques
    update_stats(&result);

    // Affiche le résultat du cycle
    logger_print_cycle_result(stats.total_cycles, &result);

    // Feedback LED
    if (result.success) {
        led_blink(1, 20, 0);  // Court flash pour succès
    } else {
        led_blink(2, 50, 50);  // Double flash pour erreur
    }

    // Vérifie les échecs consécutifs
    if (stats.consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
        LOG_ERROR("Max consecutive failures reached (%lu)", stats.consecutive_failures);
        logger_print_stats(&stats);

        // Tente un dernier power-cycle
        power_cycle();
        delay(1000);

        // Reboot automatique
        system_reboot();
    }

    // Affichage périodique des stats
    periodic_stats_display();
}
