/**
 * @file config.h
 * @brief Configuration principale pour le test de stress SD sur HTCC-AB02
 *
 * Ce fichier contient toutes les configurations matérielles et logicielles
 * pour le test intensif de carte SD sur la carte CubeCell AB02.
 *
 * IMPORTANT: Certains paramètres peuvent être surchargés via platformio.ini
 * en utilisant les build_flags (-D PARAM=VALUE)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// CONFIGURATION MATERIELLE - HTCC-AB02 (CubeCell Board Plus)
// =============================================================================

/**
 * Pin Vext Control - Alimentation externe 3.3V
 * LOW = ON (alimentation active), HIGH = OFF (alimentation coupée)
 * Max 350mA @ 3.3V
 *
 * IMPORTANT: Ce pin permet de faire un vrai power-cycle de la carte SD!
 */
#define PIN_VEXT_CTRL       Vext

/**
 * Pins SPI pour la carte SD (Software SPI utilisant SPI1)
 *
 * HTCC-AB02 dispose de deux ports SPI:
 *   - SPI0: Utilisé par le module LoRa SX1262 intégré (NE PAS UTILISER)
 *   - SPI1: Disponible pour la carte SD (GPIO1/GPIO2/GPIO3)
 *
 * Mapping GPIO AB02 (depuis pins_arduino.h):
 *   - GPIO1 (P6_0) = MOSI1
 *   - GPIO2 (P6_1) = MISO1
 *   - GPIO3 (P6_2) = SCK1/PWM1
 *   - GPIO4 (P6_4) = PWM2 (libre)
 *   - GPIO5 (P3_4) = libre
 *   - GPIO6 (P3_6) = libre
 *   - GPIO7 (P3_7) = USER_KEY/VBAT_ADC_CTL
 *   - SDA/SCL (P0_1/P0_0) = I2C0 pour OLED
 *   - SDA1/SCL1 (GPIO9/GPIO8) = I2C1
 *
 * Configuration SPI1 pour SD:
 *   - GPIO1 = MOSI1
 *   - GPIO2 = MISO1
 *   - GPIO3 = SCK1
 *   - GPIO4 = CS (libre, choisi car proche des autres pins SPI1)
 */
#define PIN_SD_MOSI         GPIO1   // MOSI1 - Master Out Slave In
#define PIN_SD_MISO         GPIO2   // MISO1 - Master In Slave Out
#define PIN_SD_SCK          GPIO3   // SCK1  - Serial Clock
#define PIN_SD_CS           GPIO4   // Chip Select (actif LOW) - GPIO libre

/**
 * Pin LED embarquée (optionnel, pour feedback visuel)
 * Sur AB02, la LED RGB est sur GPIO13 (P0_6), contrôlée via le framework
 * On utilise GPIO5 comme sortie LED externe si nécessaire
 */
#define PIN_LED             GPIO5   // LED externe sur GPIO5 (P3_4)

/**
 * Pin USER Button
 * AB02: GPIO7 (P3_7) est partagé avec VBAT_ADC_CTL
 * USER_KEY est défini comme alias dans pins_arduino.h
 */
#define PIN_USER_BUTTON     USER_KEY

// =============================================================================
// CONFIGURATION SPI
// =============================================================================

/**
 * Fréquence SPI pour la carte SD
 * ATTENTION: Les cartes SD peuvent être instables à haute fréquence
 * Valeurs recommandées:
 *   - 400000  (400 kHz)  : Initialisation, très stable
 *   - 1000000 (1 MHz)    : Stable, recommandé pour debug
 *   - 4000000 (4 MHz)    : Normal, bon compromis
 *   - 10000000 (10 MHz)  : Rapide mais peut être instable
 *   - 25000000 (25 MHz)  : Maximum théorique, souvent instable
 */
#ifndef SD_SPI_FREQUENCY
#define SD_SPI_FREQUENCY    4000000UL
#endif

/**
 * Fréquence SPI d'initialisation (toujours basse)
 */
#define SD_SPI_INIT_FREQ    400000UL

/**
 * Délai après activation Vext avant init SD (ms)
 * La carte SD a besoin de temps pour se stabiliser après power-on
 */
#define VEXT_POWER_ON_DELAY_MS  100

/**
 * Délai après désactivation Vext (ms)
 * Temps pour que les condensateurs se déchargent
 */
#define VEXT_POWER_OFF_DELAY_MS 50

// =============================================================================
// CONFIGURATION DU TEST DE STRESS
// =============================================================================

/**
 * Intervalle entre chaque cycle de test (ms)
 */
#ifndef CYCLE_INTERVAL_MS
#define CYCLE_INTERVAL_MS   1000
#endif

/**
 * Mode agressif: unmount/remount la SD à chaque cycle
 * 0 = Mode continu (fichier reste ouvert, flush à chaque écriture)
 * 1 = Mode agressif (open/write/close à chaque cycle + power-cycle optionnel)
 */
#ifndef AGGRESSIVE_MODE
#define AGGRESSIVE_MODE     1
#endif

/**
 * Activer le power-cycle hardware (via Vext) à chaque cycle
 * Seulement actif si AGGRESSIVE_MODE=1
 */
#define POWER_CYCLE_ENABLED 1

/**
 * Nombre maximum d'échecs consécutifs avant reboot automatique
 */
#ifndef MAX_CONSECUTIVE_FAILURES
#define MAX_CONSECUTIVE_FAILURES 10
#endif

/**
 * Nombre de retries pour une opération SD avant de déclarer un échec
 */
#define SD_OPERATION_RETRIES    3

/**
 * Délai entre les retries (ms)
 */
#define SD_RETRY_DELAY_MS       100

/**
 * Activer le fallback automatique de fréquence SPI
 * Si l'init échoue, réduit la fréquence et réessaye
 */
#define SPI_FREQUENCY_FALLBACK  1

// =============================================================================
// CONFIGURATION FICHIER CSV
// =============================================================================

/**
 * Nom du fichier CSV sur la carte SD
 */
#ifndef CSV_FILENAME
#define CSV_FILENAME        "/sd_test.csv"
#endif

/**
 * Taille maximale de la ligne CSV (bytes)
 * Format: timestamp,cycle,status,error_code,init_time_ms,write_time_ms,spi_freq
 */
#define CSV_LINE_MAX_SIZE   128

/**
 * Écrire l'en-tête CSV si le fichier est nouveau
 */
#define CSV_WRITE_HEADER    1

/**
 * En-tête du fichier CSV
 */
#define CSV_HEADER          "timestamp_ms,cycle,status,error_code,init_time_us,write_time_us,spi_freq_hz,vbat_mv,free_heap\n"

// =============================================================================
// CONFIGURATION LOGGING
// =============================================================================

/**
 * Niveau de log (0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)
 * Note: On utilise APP_LOG_LEVEL car LOG_LEVEL est déjà utilisé par le framework CubeCell
 */
#ifndef APP_LOG_LEVEL
#define APP_LOG_LEVEL       3
#endif

/**
 * Vitesse du port série
 */
#define SERIAL_BAUD_RATE    115200

/**
 * Activer les logs série
 */
#ifndef SERIAL_DEBUG
#define SERIAL_DEBUG        1
#endif

// =============================================================================
// CODES D'ERREUR
// =============================================================================

typedef enum {
    ERR_NONE = 0,
    ERR_SD_INIT_FAILED = 1,
    ERR_SD_MOUNT_FAILED = 2,
    ERR_FILE_OPEN_FAILED = 3,
    ERR_FILE_WRITE_FAILED = 4,
    ERR_FILE_CLOSE_FAILED = 5,
    ERR_SD_UNMOUNT_FAILED = 6,
    ERR_VEXT_TIMEOUT = 7,
    ERR_SPI_INIT_FAILED = 8,
    ERR_SD_NOT_PRESENT = 9,
    ERR_SD_CARD_TYPE_UNKNOWN = 10,
    ERR_FAT_VOLUME_FAILED = 11,
    ERR_BUFFER_OVERFLOW = 12,
    ERR_UNKNOWN = 255
} sd_error_t;

/**
 * Structure pour les statistiques de test
 */
typedef struct {
    uint32_t total_cycles;
    uint32_t successful_cycles;
    uint32_t failed_cycles;
    uint32_t consecutive_failures;
    uint32_t total_init_time_us;
    uint32_t total_write_time_us;
    uint32_t min_init_time_us;
    uint32_t max_init_time_us;
    uint32_t min_write_time_us;
    uint32_t max_write_time_us;
    uint32_t spi_fallback_count;
    sd_error_t last_error;
    uint32_t current_spi_freq;
} test_stats_t;

/**
 * Structure pour le résultat d'un cycle
 */
typedef struct {
    bool success;
    sd_error_t error_code;
    uint32_t init_time_us;
    uint32_t write_time_us;
    uint32_t spi_freq_used;
} cycle_result_t;

// =============================================================================
// MACROS UTILITAIRES
// =============================================================================

/**
 * Obtenir le niveau de batterie (mV)
 * ADC sur GPIO1 avec diviseur de tension 100k/(100k+390k)
 */
#define GET_BATTERY_MV()    ((uint32_t)(analogRead(ADC) * 4.9 * 1000 / 4096))

/**
 * Obtenir la mémoire libre (si disponible)
 */
#ifdef ESP_PLATFORM
#define GET_FREE_HEAP()     ESP.getFreeHeap()
#else
// CubeCell ASR6501 - pas de fonction standard, utiliser une estimation
extern "C" char* sbrk(int incr);
inline uint32_t getFreeHeap() {
    char top;
    return &top - reinterpret_cast<char*>(sbrk(0));
}
#define GET_FREE_HEAP()     getFreeHeap()
#endif

#endif // CONFIG_H
