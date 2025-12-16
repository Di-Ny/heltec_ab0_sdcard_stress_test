/**
 * @file power_cycle.h
 * @brief Gestion du power-cycle de la carte SD via Vext sur HTCC-AB01
 *
 * Le pin Vext (GPIO6) contrôle l'alimentation externe 3.3V:
 * - LOW = Alimentation ON (jusqu'à 350mA)
 * - HIGH = Alimentation OFF
 *
 * Ce module permet de faire un vrai power-cycle hardware de la carte SD,
 * ce qui est plus fiable qu'un simple unmount pour réinitialiser la carte.
 */

#ifndef POWER_CYCLE_H
#define POWER_CYCLE_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief Initialise le contrôle d'alimentation Vext
 *
 * Configure le pin GPIO6 en sortie et active l'alimentation.
 */
void power_init(void);

/**
 * @brief Active l'alimentation Vext (carte SD alimentée)
 *
 * Met le pin Vext à LOW pour activer l'alimentation 3.3V.
 * Attend le délai de stabilisation configuré.
 */
void power_on(void);

/**
 * @brief Désactive l'alimentation Vext (carte SD non alimentée)
 *
 * Met le pin Vext à HIGH pour couper l'alimentation.
 * Attend le délai de décharge configuré.
 */
void power_off(void);

/**
 * @brief Effectue un cycle complet power-off/power-on
 *
 * Séquence:
 * 1. Coupe l'alimentation (HIGH)
 * 2. Attend le délai de décharge
 * 3. Réactive l'alimentation (LOW)
 * 4. Attend le délai de stabilisation
 *
 * @return Durée totale du cycle en millisecondes
 */
uint32_t power_cycle(void);

/**
 * @brief Vérifie si l'alimentation Vext est active
 *
 * @return true si l'alimentation est ON (pin LOW)
 */
bool power_is_on(void);

/**
 * @brief Obtient le délai total d'un power-cycle
 *
 * @return Durée en millisecondes (OFF delay + ON delay)
 */
uint32_t power_get_cycle_duration_ms(void);

/**
 * @brief Active/désactive la LED de feedback
 *
 * @param on true pour allumer, false pour éteindre
 */
void led_set(bool on);

/**
 * @brief Fait clignoter la LED
 *
 * @param count Nombre de clignotements
 * @param on_ms Durée ON en ms
 * @param off_ms Durée OFF en ms
 */
void led_blink(uint8_t count, uint16_t on_ms, uint16_t off_ms);

/**
 * @brief Effectue un reboot logiciel du microcontrôleur
 *
 * Utilisé après trop d'échecs consécutifs.
 */
void system_reboot(void);

/**
 * @brief Lit l'état du bouton utilisateur
 *
 * @return true si le bouton est pressé
 */
bool button_is_pressed(void);

/**
 * @brief Initialise le bouton utilisateur avec interruption
 *
 * @param callback Fonction à appeler lors d'un appui (ou NULL)
 */
void button_init(void (*callback)(void));

#endif // POWER_CYCLE_H
