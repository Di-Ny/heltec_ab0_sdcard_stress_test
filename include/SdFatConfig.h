/**
 * @file SdFatConfig.h
 * @brief Configuration SdFat pour HTCC-AB01 avec Software SPI
 *
 * Ce fichier doit être inclus AVANT SdFat.h pour configurer
 * correctement la bibliothèque pour l'utilisation du Software SPI.
 */

#ifndef SDFAT_CONFIG_H
#define SDFAT_CONFIG_H

// =============================================================================
// CONFIGURATION SDFAT POUR CUBECELL ASR6501
// =============================================================================

/**
 * Sélection du driver SPI:
 * 0 = Hardware SPI (SPI.h)
 * 1 = Hardware SPI avec buffer DMA
 * 2 = Software SPI (nécessaire pour CubeCell car SPI hardware = LoRa)
 */
#define SPI_DRIVER_SELECT 2

/**
 * Type de système de fichiers supporté:
 * 0 = SdFat (supporte FAT16/FAT32 et exFAT)
 * 1 = FAT16/FAT32 seulement (économise ~12KB de Flash)
 * 2 = exFAT seulement
 * 3 = FAT16/FAT32 et exFAT
 */
#define SD_FAT_TYPE 1

/**
 * Désactive les fonctions avancées pour économiser de la mémoire
 */
#define USE_LONG_FILE_NAMES 0
#define USE_MULTI_SECTOR_IO 0
#define MAINTAIN_FREE_CLUSTER_COUNT 0
#define SDFAT_FILE_TYPE 1

/**
 * Configuration pour petits MCU (ASR6501 = 16KB RAM)
 */
#define ENABLE_ARDUINO_FEATURES 1
#define ENABLE_ARDUINO_SERIAL 1

#endif // SDFAT_CONFIG_H
