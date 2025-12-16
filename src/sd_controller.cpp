/**
 * @file sd_controller.cpp
 * @brief Implémentation du contrôleur de carte SD
 *
 * Utilise une implémentation Software SPI manuelle compatible avec CubeCell ASR6501
 * car SdFat n'est pas directement compatible avec le framework CubeCell.
 */

#include "sd_controller.h"
#include <SPI.h>

// =============================================================================
// CONSTANTES SD
// =============================================================================

// Commandes SD
#define CMD0    0x00    // GO_IDLE_STATE
#define CMD1    0x01    // SEND_OP_COND
#define CMD8    0x08    // SEND_IF_COND
#define CMD9    0x09    // SEND_CSD
#define CMD10   0x0A    // SEND_CID
#define CMD12   0x0C    // STOP_TRANSMISSION
#define CMD16   0x10    // SET_BLOCKLEN
#define CMD17   0x11    // READ_SINGLE_BLOCK
#define CMD24   0x18    // WRITE_BLOCK
#define CMD55   0x37    // APP_CMD
#define CMD58   0x3A    // READ_OCR
#define ACMD41  0x29    // SD_SEND_OP_COND

// Réponses SD
#define R1_IDLE_STATE     0x01
#define R1_ILLEGAL_CMD    0x04

// Tokens
#define TOKEN_START_BLOCK 0xFE
#define TOKEN_DATA_ACCEPTED 0x05

// Types de carte
#define CT_NONE     0
#define CT_SD1      1   // SD v1
#define CT_SD2      2   // SD v2 (SDSC)
#define CT_SDHC     3   // SDHC/SDXC

// =============================================================================
// VARIABLES GLOBALES
// =============================================================================

static bool sd_initialized = false;
static bool sd_mounted = false;
static uint8_t card_type = CT_NONE;
static uint32_t current_spi_freq = SD_SPI_FREQUENCY;
static uint32_t last_init_time_us = 0;
static uint32_t last_write_time_us = 0;
static uint32_t card_sectors = 0;

// Position d'écriture dans le fichier
static uint32_t csv_next_sector = 0;
static uint16_t csv_byte_offset = 0;
static uint8_t sector_buffer[512];
static bool header_written = false;

// Table des fréquences pour fallback
static const uint32_t spi_freq_table[] = {
    4000000UL,   // 4 MHz
    1000000UL,   // 1 MHz
    400000UL     // 400 kHz (minimum)
};
static const uint8_t spi_freq_count = sizeof(spi_freq_table) / sizeof(spi_freq_table[0]);

// =============================================================================
// SOFTWARE SPI
// =============================================================================

static inline void spi_delay(void) {
    // Ajuster selon fréquence désirée
    if (current_spi_freq <= 400000) {
        delayMicroseconds(2);
    } else if (current_spi_freq <= 1000000) {
        delayMicroseconds(1);
    }
    // Pour 4MHz+, pas de délai
}

static void spi_init(void) {
    pinMode(PIN_SD_MOSI, OUTPUT);
    pinMode(PIN_SD_MISO, INPUT_PULLUP);
    pinMode(PIN_SD_SCK, OUTPUT);
    pinMode(PIN_SD_CS, OUTPUT);

    digitalWrite(PIN_SD_MOSI, HIGH);
    digitalWrite(PIN_SD_SCK, LOW);
    digitalWrite(PIN_SD_CS, HIGH);
}

static uint8_t spi_transfer(uint8_t data) {
    uint8_t received = 0;

    for (uint8_t i = 0; i < 8; i++) {
        // Set MOSI
        digitalWrite(PIN_SD_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;

        spi_delay();

        // Clock high, read MISO
        digitalWrite(PIN_SD_SCK, HIGH);
        received <<= 1;
        if (digitalRead(PIN_SD_MISO)) {
            received |= 1;
        }

        spi_delay();

        // Clock low
        digitalWrite(PIN_SD_SCK, LOW);
    }

    return received;
}

static void spi_select(void) {
    digitalWrite(PIN_SD_CS, LOW);
}

static void spi_deselect(void) {
    digitalWrite(PIN_SD_CS, HIGH);
    spi_transfer(0xFF);  // Extra clocks
}

// =============================================================================
// FONCTIONS SD BAS NIVEAU
// =============================================================================

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t response;
    uint8_t retry = 0;

    // Si ACMD, envoyer d'abord CMD55
    if (cmd & 0x80) {
        cmd &= 0x7F;
        response = sd_send_cmd(CMD55, 0);
        if (response > 1) return response;
    }

    // Sélectionner et envoyer la commande
    spi_deselect();
    spi_select();

    // Attendre que la carte soit prête
    while (spi_transfer(0xFF) != 0xFF) {
        if (++retry > 200) return 0xFF;
    }

    // Envoyer commande
    spi_transfer(0x40 | cmd);
    spi_transfer((uint8_t)(arg >> 24));
    spi_transfer((uint8_t)(arg >> 16));
    spi_transfer((uint8_t)(arg >> 8));
    spi_transfer((uint8_t)arg);

    // CRC
    uint8_t crc = 0xFF;
    if (cmd == CMD0) crc = 0x95;
    if (cmd == CMD8) crc = 0x87;
    spi_transfer(crc);

    // Attendre réponse
    retry = 0;
    do {
        response = spi_transfer(0xFF);
    } while ((response & 0x80) && (++retry < 10));

    return response;
}

static bool sd_read_sector(uint32_t sector, uint8_t* buffer) {
    uint8_t response;
    uint16_t retry = 0;

    // Adresse en octets pour SD, en secteurs pour SDHC
    uint32_t addr = (card_type == CT_SDHC) ? sector : (sector << 9);

    response = sd_send_cmd(CMD17, addr);
    if (response != 0) {
        spi_deselect();
        return false;
    }

    // Attendre le token de début
    while ((response = spi_transfer(0xFF)) == 0xFF) {
        if (++retry > 10000) {
            spi_deselect();
            return false;
        }
    }

    if (response != TOKEN_START_BLOCK) {
        spi_deselect();
        return false;
    }

    // Lire les données
    for (uint16_t i = 0; i < 512; i++) {
        buffer[i] = spi_transfer(0xFF);
    }

    // Ignorer CRC
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    spi_deselect();
    return true;
}

static bool sd_write_sector(uint32_t sector, const uint8_t* buffer) {
    uint8_t response;
    uint16_t retry = 0;

    // Adresse en octets pour SD, en secteurs pour SDHC
    uint32_t addr = (card_type == CT_SDHC) ? sector : (sector << 9);

    response = sd_send_cmd(CMD24, addr);
    if (response != 0) {
        spi_deselect();
        return false;
    }

    // Token de début
    spi_transfer(0xFF);
    spi_transfer(TOKEN_START_BLOCK);

    // Écrire les données
    for (uint16_t i = 0; i < 512; i++) {
        spi_transfer(buffer[i]);
    }

    // Dummy CRC
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    // Vérifier la réponse
    response = spi_transfer(0xFF);
    if ((response & 0x1F) != TOKEN_DATA_ACCEPTED) {
        spi_deselect();
        return false;
    }

    // Attendre la fin de l'écriture
    while (spi_transfer(0xFF) == 0) {
        if (++retry > 50000) {
            spi_deselect();
            return false;
        }
    }

    spi_deselect();
    return true;
}

// =============================================================================
// FONCTIONS FAT32 SIMPLIFIÉES
// =============================================================================

// Structure du BPB (BIOS Parameter Block)
static uint16_t bytes_per_sector = 512;
static uint8_t sectors_per_cluster = 1;
static uint16_t reserved_sectors = 0;
static uint8_t num_fats = 2;
static uint32_t sectors_per_fat = 0;
static uint32_t root_cluster = 2;
static uint32_t fat_start_sector = 0;
static uint32_t data_start_sector = 0;
static uint32_t total_sectors = 0;

static bool fat32_read_bpb(void) {
    if (!sd_read_sector(0, sector_buffer)) {
        return false;
    }

    // Vérifier signature
    if (sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) {
        // Peut-être une table de partition, chercher la première partition
        uint32_t part_start = sector_buffer[0x1C6] |
                              ((uint32_t)sector_buffer[0x1C7] << 8) |
                              ((uint32_t)sector_buffer[0x1C8] << 16) |
                              ((uint32_t)sector_buffer[0x1C9] << 24);

        if (part_start == 0 || !sd_read_sector(part_start, sector_buffer)) {
            return false;
        }
        fat_start_sector = part_start;
    } else {
        fat_start_sector = 0;
    }

    // Vérifier signature FAT32
    if (sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) {
        return false;
    }

    // Lire BPB
    bytes_per_sector = sector_buffer[0x0B] | ((uint16_t)sector_buffer[0x0C] << 8);
    sectors_per_cluster = sector_buffer[0x0D];
    reserved_sectors = sector_buffer[0x0E] | ((uint16_t)sector_buffer[0x0F] << 8);
    num_fats = sector_buffer[0x10];

    // FAT32 specific
    sectors_per_fat = sector_buffer[0x24] |
                      ((uint32_t)sector_buffer[0x25] << 8) |
                      ((uint32_t)sector_buffer[0x26] << 16) |
                      ((uint32_t)sector_buffer[0x27] << 24);

    root_cluster = sector_buffer[0x2C] |
                   ((uint32_t)sector_buffer[0x2D] << 8) |
                   ((uint32_t)sector_buffer[0x2E] << 16) |
                   ((uint32_t)sector_buffer[0x2F] << 24);

    total_sectors = sector_buffer[0x20] |
                    ((uint32_t)sector_buffer[0x21] << 8) |
                    ((uint32_t)sector_buffer[0x22] << 16) |
                    ((uint32_t)sector_buffer[0x23] << 24);

    fat_start_sector += reserved_sectors;
    data_start_sector = fat_start_sector + (num_fats * sectors_per_fat);

    return true;
}

static uint32_t cluster_to_sector(uint32_t cluster) {
    return data_start_sector + ((cluster - 2) * sectors_per_cluster);
}

// Trouve ou crée le fichier CSV
static bool fat32_find_or_create_file(void) {
    // Pour simplifier, on utilise un fichier à cluster fixe
    // Dans une implémentation complète, il faudrait parcourir la FAT

    // Lire le répertoire racine
    uint32_t root_sector = cluster_to_sector(root_cluster);

    if (!sd_read_sector(root_sector, sector_buffer)) {
        return false;
    }

    // Chercher SD_TEST.CSV ou une entrée libre
    bool found = false;
    uint8_t free_entry = 255;

    for (uint8_t i = 0; i < 16; i++) {  // 16 entrées par secteur
        uint8_t* entry = &sector_buffer[i * 32];

        if (entry[0] == 0x00) {
            // Fin du répertoire
            if (free_entry == 255) free_entry = i;
            break;
        }

        if (entry[0] == 0xE5) {
            // Entrée supprimée, peut être réutilisée
            if (free_entry == 255) free_entry = i;
            continue;
        }

        // Comparer le nom (8.3 format: "SD_TEST CSV")
        if (memcmp(entry, "SD_TEST CSV", 11) == 0) {
            // Fichier trouvé!
            found = true;
            header_written = true;

            // Récupérer le cluster de départ
            uint32_t start_cluster = entry[0x1A] |
                                     ((uint32_t)entry[0x1B] << 8) |
                                     ((uint32_t)entry[0x14] << 16) |
                                     ((uint32_t)entry[0x15] << 24);

            uint32_t file_size = entry[0x1C] |
                                 ((uint32_t)entry[0x1D] << 8) |
                                 ((uint32_t)entry[0x1E] << 16) |
                                 ((uint32_t)entry[0x1F] << 24);

            csv_next_sector = cluster_to_sector(start_cluster) + (file_size / 512);
            csv_byte_offset = file_size % 512;

            break;
        }
    }

    if (!found && free_entry < 16) {
        // Créer le fichier
        uint8_t* entry = &sector_buffer[free_entry * 32];

        // Nom de fichier 8.3
        memcpy(entry, "SD_TEST CSV", 11);
        entry[0x0B] = 0x20;  // Attribut: archive

        // Cluster de départ (utiliser un cluster libre simple)
        // Pour simplifier, on utilise le cluster 3
        uint32_t new_cluster = 3;
        entry[0x14] = (new_cluster >> 16) & 0xFF;
        entry[0x15] = (new_cluster >> 24) & 0xFF;
        entry[0x1A] = new_cluster & 0xFF;
        entry[0x1B] = (new_cluster >> 8) & 0xFF;

        // Taille = 0
        entry[0x1C] = 0;
        entry[0x1D] = 0;
        entry[0x1E] = 0;
        entry[0x1F] = 0;

        // Écrire le répertoire
        if (!sd_write_sector(root_sector, sector_buffer)) {
            return false;
        }

        csv_next_sector = cluster_to_sector(new_cluster);
        csv_byte_offset = 0;
        header_written = false;

        // Marquer le cluster comme utilisé dans la FAT
        if (!sd_read_sector(fat_start_sector, sector_buffer)) {
            return false;
        }

        // Chaque entrée FAT32 fait 4 octets
        uint32_t fat_offset = new_cluster * 4;
        uint16_t fat_sector_offset = fat_offset % 512;

        // Marquer comme fin de chaîne
        sector_buffer[fat_sector_offset] = 0xFF;
        sector_buffer[fat_sector_offset + 1] = 0xFF;
        sector_buffer[fat_sector_offset + 2] = 0xFF;
        sector_buffer[fat_sector_offset + 3] = 0x0F;

        if (!sd_write_sector(fat_start_sector, sector_buffer)) {
            return false;
        }
    }

    return found || (free_entry < 16);
}

// =============================================================================
// IMPLÉMENTATION API PUBLIQUE
// =============================================================================

bool sd_controller_init(void) {
    spi_init();
    current_spi_freq = SD_SPI_FREQUENCY;
    sd_initialized = false;
    sd_mounted = false;
    card_type = CT_NONE;
    header_written = false;

    return true;
}

sd_error_t sd_mount(uint32_t freq_hz) {
    uint32_t start_time = micros();
    uint8_t response;
    uint16_t retry;

    if (freq_hz > 0) {
        current_spi_freq = freq_hz;
    }

    if (sd_mounted) {
        sd_unmount();
    }

    // Phase 1: Initialisation à basse vitesse
    uint32_t saved_freq = current_spi_freq;
    current_spi_freq = 400000;  // Init toujours à 400kHz

    // 80 cycles d'horloge avec CS high
    spi_deselect();
    for (uint8_t i = 0; i < 10; i++) {
        spi_transfer(0xFF);
    }

    // CMD0 - Reset
    retry = 0;
    do {
        response = sd_send_cmd(CMD0, 0);
    } while (response != R1_IDLE_STATE && ++retry < 100);

    if (response != R1_IDLE_STATE) {
        spi_deselect();
        current_spi_freq = saved_freq;
        last_init_time_us = micros() - start_time;
        return ERR_SD_INIT_FAILED;
    }

    // CMD8 - Check version
    response = sd_send_cmd(CMD8, 0x1AA);

    if (response == R1_IDLE_STATE) {
        // SD v2
        uint8_t ocr[4];
        for (uint8_t i = 0; i < 4; i++) {
            ocr[i] = spi_transfer(0xFF);
        }
        spi_deselect();

        if (ocr[2] != 0x01 || ocr[3] != 0xAA) {
            current_spi_freq = saved_freq;
            last_init_time_us = micros() - start_time;
            return ERR_SD_CARD_TYPE_UNKNOWN;
        }

        // ACMD41 avec HCS
        retry = 0;
        do {
            response = sd_send_cmd(0x80 | ACMD41, 0x40000000);
        } while (response != 0 && ++retry < 1000);

        if (response != 0) {
            spi_deselect();
            current_spi_freq = saved_freq;
            last_init_time_us = micros() - start_time;
            return ERR_SD_INIT_FAILED;
        }

        // CMD58 - Read OCR
        response = sd_send_cmd(CMD58, 0);
        if (response == 0) {
            for (uint8_t i = 0; i < 4; i++) {
                ocr[i] = spi_transfer(0xFF);
            }
            card_type = (ocr[0] & 0x40) ? CT_SDHC : CT_SD2;
        }
        spi_deselect();

    } else if (response & R1_ILLEGAL_CMD) {
        // SD v1
        spi_deselect();

        retry = 0;
        do {
            response = sd_send_cmd(0x80 | ACMD41, 0);
        } while (response != 0 && ++retry < 1000);

        if (response != 0) {
            current_spi_freq = saved_freq;
            last_init_time_us = micros() - start_time;
            return ERR_SD_INIT_FAILED;
        }

        card_type = CT_SD1;
    } else {
        spi_deselect();
        current_spi_freq = saved_freq;
        last_init_time_us = micros() - start_time;
        return ERR_SD_INIT_FAILED;
    }

    // Set block size to 512 for SD1/SD2
    if (card_type != CT_SDHC) {
        response = sd_send_cmd(CMD16, 512);
        spi_deselect();
        if (response != 0) {
            current_spi_freq = saved_freq;
            last_init_time_us = micros() - start_time;
            return ERR_SD_INIT_FAILED;
        }
    }

    // Restaurer la fréquence
    current_spi_freq = saved_freq;
    sd_initialized = true;

    // Phase 2: Monter le système de fichiers FAT32
    if (!fat32_read_bpb()) {
        last_init_time_us = micros() - start_time;
        return ERR_FAT_VOLUME_FAILED;
    }

    // Trouver ou créer le fichier CSV
    if (!fat32_find_or_create_file()) {
        last_init_time_us = micros() - start_time;
        return ERR_FILE_OPEN_FAILED;
    }

    sd_mounted = true;
    last_init_time_us = micros() - start_time;
    return ERR_NONE;
}

sd_error_t sd_unmount(void) {
    sd_mounted = false;
    spi_deselect();
    return ERR_NONE;
}

bool sd_is_mounted(void) {
    return sd_mounted;
}

sd_error_t sd_write_csv_line(uint32_t cycle, const cycle_result_t* result, uint32_t timestamp_ms) {
    if (!sd_mounted) {
        return ERR_SD_MOUNT_FAILED;
    }

    uint32_t start_time = micros();

    // Préparer la ligne
    char line[CSV_LINE_MAX_SIZE];
    int len;

    // Écrire l'en-tête si c'est le premier write
    if (!header_written) {
        len = snprintf(line, sizeof(line), "%s", CSV_HEADER);
        header_written = true;
    } else {
        line[0] = '\0';
        len = 0;
    }

    // Ajouter la ligne de données
    int data_len = snprintf(line + len, sizeof(line) - len,
        "%lu,%lu,%s,%d,%lu,%lu,%lu,%lu,%lu\n",
        timestamp_ms,
        cycle,
        result->success ? "OK" : "FAIL",
        (int)result->error_code,
        result->init_time_us,
        result->write_time_us,
        result->spi_freq_used,
        GET_BATTERY_MV(),
        GET_FREE_HEAP()
    );

    len += data_len;

    if (len < 0 || len >= (int)sizeof(line)) {
        last_write_time_us = micros() - start_time;
        return ERR_BUFFER_OVERFLOW;
    }

    // Lire le secteur actuel si on n'est pas au début
    if (csv_byte_offset > 0) {
        if (!sd_read_sector(csv_next_sector, sector_buffer)) {
            last_write_time_us = micros() - start_time;
            return ERR_FILE_WRITE_FAILED;
        }
    } else {
        memset(sector_buffer, 0, 512);
    }

    // Copier les données
    uint16_t bytes_written = 0;
    while (bytes_written < len) {
        uint16_t space_in_sector = 512 - csv_byte_offset;
        uint16_t to_write = (len - bytes_written < space_in_sector) ?
                            (len - bytes_written) : space_in_sector;

        memcpy(&sector_buffer[csv_byte_offset], &line[bytes_written], to_write);
        csv_byte_offset += to_write;
        bytes_written += to_write;

        // Écrire le secteur si plein ou terminé
        if (csv_byte_offset >= 512 || bytes_written >= len) {
            if (!sd_write_sector(csv_next_sector, sector_buffer)) {
                last_write_time_us = micros() - start_time;
                return ERR_FILE_WRITE_FAILED;
            }

            if (csv_byte_offset >= 512) {
                csv_next_sector++;
                csv_byte_offset = 0;
                memset(sector_buffer, 0, 512);
            }
        }
    }

    last_write_time_us = micros() - start_time;
    return ERR_NONE;
}

sd_error_t sd_health_check(void) {
    if (!sd_mounted) {
        return ERR_SD_MOUNT_FAILED;
    }

    // Simple test: lire le secteur 0
    if (!sd_read_sector(0, sector_buffer)) {
        return ERR_SD_INIT_FAILED;
    }

    return ERR_NONE;
}

uint32_t sd_get_current_frequency(void) {
    return current_spi_freq;
}

bool sd_reduce_frequency(void) {
    for (uint8_t i = 0; i < spi_freq_count; i++) {
        if (spi_freq_table[i] < current_spi_freq) {
            current_spi_freq = spi_freq_table[i];
            return true;
        }
    }
    return false;
}

void sd_reset_frequency(void) {
    current_spi_freq = SD_SPI_FREQUENCY;
}

sd_error_t sd_get_card_info(char* card_type_str, uint32_t* card_size_mb) {
    if (!sd_initialized) {
        return ERR_SD_INIT_FAILED;
    }

    if (card_type_str != nullptr) {
        switch (card_type) {
            case CT_SD1:
                strcpy(card_type_str, "SD1");
                break;
            case CT_SD2:
                strcpy(card_type_str, "SD2");
                break;
            case CT_SDHC:
                strcpy(card_type_str, "SDHC");
                break;
            default:
                strcpy(card_type_str, "Unknown");
                break;
        }
    }

    if (card_size_mb != nullptr) {
        // Estimation basée sur le total des secteurs
        *card_size_mb = (total_sectors / 2048);  // sectors * 512 / 1024 / 1024
    }

    return ERR_NONE;
}

uint32_t sd_get_last_init_time_us(void) {
    return last_init_time_us;
}

uint32_t sd_get_last_write_time_us(void) {
    return last_write_time_us;
}
