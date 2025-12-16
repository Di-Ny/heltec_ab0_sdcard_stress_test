/**
 * @file logger.h
 * @brief Module de logging série pour le test de stress SD
 *
 * Fournit des macros et fonctions pour le logging avec niveaux:
 * - LOG_ERROR (1): Erreurs critiques
 * - LOG_WARN (2): Avertissements
 * - LOG_INFO (3): Informations générales
 * - LOG_DEBUG (4): Debug détaillé
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// DÉFINITIONS DES NIVEAUX DE LOG
// =============================================================================

#define LOG_LEVEL_OFF   0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

// =============================================================================
// MACROS DE LOGGING
// =============================================================================

#if SERIAL_DEBUG && APP_LOG_LEVEL >= LOG_LEVEL_ERROR
    #define LOG_ERROR(fmt, ...) logger_print(LOG_LEVEL_ERROR, F(fmt), ##__VA_ARGS__)
    #define LOG_ERROR_LN(msg)   logger_println(LOG_LEVEL_ERROR, F(msg))
#else
    #define LOG_ERROR(fmt, ...)
    #define LOG_ERROR_LN(msg)
#endif

#if SERIAL_DEBUG && APP_LOG_LEVEL >= LOG_LEVEL_WARN
    #define LOG_WARN(fmt, ...)  logger_print(LOG_LEVEL_WARN, F(fmt), ##__VA_ARGS__)
    #define LOG_WARN_LN(msg)    logger_println(LOG_LEVEL_WARN, F(msg))
#else
    #define LOG_WARN(fmt, ...)
    #define LOG_WARN_LN(msg)
#endif

#if SERIAL_DEBUG && APP_LOG_LEVEL >= LOG_LEVEL_INFO
    #define LOG_INFO(fmt, ...)  logger_print(LOG_LEVEL_INFO, F(fmt), ##__VA_ARGS__)
    #define LOG_INFO_LN(msg)    logger_println(LOG_LEVEL_INFO, F(msg))
#else
    #define LOG_INFO(fmt, ...)
    #define LOG_INFO_LN(msg)
#endif

#if SERIAL_DEBUG && APP_LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define LOG_DEBUG(fmt, ...) logger_print(LOG_LEVEL_DEBUG, F(fmt), ##__VA_ARGS__)
    #define LOG_DEBUG_LN(msg)   logger_println(LOG_LEVEL_DEBUG, F(msg))
#else
    #define LOG_DEBUG(fmt, ...)
    #define LOG_DEBUG_LN(msg)
#endif

// =============================================================================
// API PUBLIQUE
// =============================================================================

/**
 * @brief Initialise le module de logging
 *
 * Configure le port série avec le baud rate défini.
 */
void logger_init(void);

/**
 * @brief Affiche un message formaté avec niveau
 *
 * @param level Niveau de log
 * @param format Format string (F() macro)
 * @param ... Arguments
 */
void logger_print(uint8_t level, const __FlashStringHelper* format, ...);

/**
 * @brief Affiche un message simple avec retour à la ligne
 *
 * @param level Niveau de log
 * @param msg Message (F() macro)
 */
void logger_println(uint8_t level, const __FlashStringHelper* msg);

/**
 * @brief Affiche les statistiques de test formatées
 *
 * @param stats Pointeur vers les statistiques
 */
void logger_print_stats(const test_stats_t* stats);

/**
 * @brief Affiche le résultat d'un cycle
 *
 * @param cycle Numéro du cycle
 * @param result Résultat du cycle
 */
void logger_print_cycle_result(uint32_t cycle, const cycle_result_t* result);

/**
 * @brief Affiche les informations de la carte SD
 *
 * @param card_type Type de carte
 * @param size_mb Taille en MB
 */
void logger_print_sd_info(const char* card_type, uint32_t size_mb);

/**
 * @brief Affiche le banner de démarrage
 */
void logger_print_banner(void);

/**
 * @brief Affiche la configuration actuelle
 */
void logger_print_config(void);

/**
 * @brief Convertit un code d'erreur en string
 *
 * @param error Code d'erreur
 * @return Chaîne descriptive (en PROGMEM)
 */
const __FlashStringHelper* logger_error_to_string(sd_error_t error);

/**
 * @brief Affiche un séparateur horizontal
 */
void logger_print_separator(void);

/**
 * @brief Flush le buffer série
 */
void logger_flush(void);

#endif // LOGGER_H
