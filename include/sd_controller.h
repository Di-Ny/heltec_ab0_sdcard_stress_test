/**
 * @file sd_controller.h
 * @brief Contrôleur de carte SD pour le test de stress sur HTCC-AB01
 *
 * Ce module gère toutes les opérations SD:
 * - Initialisation avec Software SPI natif (compatible CubeCell)
 * - Mount/Unmount du système de fichiers FAT32
 * - Écriture CSV en mode append
 * - Gestion des erreurs et fallback de fréquence SPI
 *
 * Note: Utilise une implémentation Software SPI personnalisée car
 * SdFat n'est pas compatible avec le framework CubeCell Arduino.
 */

#ifndef SD_CONTROLLER_H
#define SD_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief Initialise le contrôleur SD
 *
 * Configure les pins SPI et prépare le module.
 * N'initialise PAS la carte SD elle-même (voir sd_mount).
 *
 * @return true si l'initialisation réussit
 */
bool sd_controller_init(void);

/**
 * @brief Monte la carte SD
 *
 * Initialise la communication SPI avec la carte SD et monte le système FAT.
 * Gère automatiquement le fallback de fréquence SPI si activé.
 *
 * @param freq_hz Fréquence SPI souhaitée (0 = utiliser SD_SPI_FREQUENCY)
 * @return sd_error_t Code d'erreur (ERR_NONE si succès)
 */
sd_error_t sd_mount(uint32_t freq_hz = 0);

/**
 * @brief Démonte la carte SD
 *
 * Ferme tous les fichiers ouverts et libère les ressources.
 *
 * @return sd_error_t Code d'erreur (ERR_NONE si succès)
 */
sd_error_t sd_unmount(void);

/**
 * @brief Vérifie si la carte SD est montée
 *
 * @return true si la carte est montée et accessible
 */
bool sd_is_mounted(void);

/**
 * @brief Écrit une ligne CSV sur la carte SD
 *
 * Ouvre le fichier en mode append, écrit la ligne, et ferme le fichier.
 * Crée le fichier avec l'en-tête si nécessaire.
 *
 * @param cycle Numéro du cycle de test
 * @param result Résultat du cycle à logger
 * @param timestamp_ms Timestamp en millisecondes
 * @return sd_error_t Code d'erreur (ERR_NONE si succès)
 */
sd_error_t sd_write_csv_line(uint32_t cycle, const cycle_result_t* result, uint32_t timestamp_ms);

/**
 * @brief Effectue un test de santé de la carte SD
 *
 * Vérifie que la carte répond et que le système de fichiers est accessible.
 *
 * @return sd_error_t Code d'erreur (ERR_NONE si la carte est saine)
 */
sd_error_t sd_health_check(void);

/**
 * @brief Obtient la fréquence SPI actuellement utilisée
 *
 * @return Fréquence en Hz
 */
uint32_t sd_get_current_frequency(void);

/**
 * @brief Réduit la fréquence SPI d'un cran
 *
 * Utilisé pour le fallback automatique en cas d'erreurs.
 * Fréquences: 25MHz -> 10MHz -> 4MHz -> 1MHz -> 400kHz
 *
 * @return true si une réduction était possible, false si déjà au minimum
 */
bool sd_reduce_frequency(void);

/**
 * @brief Réinitialise la fréquence SPI à la valeur par défaut
 */
void sd_reset_frequency(void);

/**
 * @brief Obtient des informations sur la carte SD
 *
 * @param card_type Buffer pour le type de carte (optionnel)
 * @param card_size_mb Taille en MB (optionnel)
 * @return sd_error_t Code d'erreur
 */
sd_error_t sd_get_card_info(char* card_type, uint32_t* card_size_mb);

/**
 * @brief Obtient le temps de la dernière opération d'init (microsecondes)
 */
uint32_t sd_get_last_init_time_us(void);

/**
 * @brief Obtient le temps de la dernière opération d'écriture (microsecondes)
 */
uint32_t sd_get_last_write_time_us(void);

#endif // SD_CONTROLLER_H
