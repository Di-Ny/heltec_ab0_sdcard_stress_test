# SD Card Stress Test - HTCC-AB02 (CubeCell Board Plus)

Test intensif de carte SD avec power-cycle hardware pour la carte Heltec HTCC-AB02.

## Fonctionnalités

- **Power-cycle hardware** via Vext - coupe vraiment l'alimentation de la SD
- **Deux modes de test** :
  - Mode Agressif : unmount/power-cycle/mount à chaque cycle
  - Mode Continu : fichier ouvert en permanence avec sync
- **Logging CSV** sur la carte SD avec timestamps et métriques
- **Fallback SPI automatique** : réduit la fréquence en cas d'erreurs
- **Reboot automatique** après N échecs consécutifs
- **Monitoring série** avec statistiques détaillées

## Matériel requis

- Carte Heltec HTCC-AB02 (CubeCell Board Plus avec ASR6501)
- Module carte SD avec interface SPI
- Carte microSD formatée FAT32
- Câbles de connexion

## Câblage

| Signal SD | Pin AB02 | Description |
|-----------|----------|-------------|
| VCC       | Vext     | Alimentation 3.3V contrôlée |
| GND       | GND      | Masse |
| MOSI      | GPIO1    | MOSI1 (P6_0) - Master Out Slave In |
| MISO      | GPIO2    | MISO1 (P6_1) - Master In Slave Out |
| SCK       | GPIO3    | SCK1 (P6_2) - Clock SPI |
| CS        | GPIO4    | Chip Select (P6_4) |

**Important** : Le pin Vext contrôle l'alimentation externe :
- `LOW` = Alimentation ON (3.3V, max 350mA)
- `HIGH` = Alimentation OFF

**Note sur les pins AB02** :
- SDA/SCL (P0_1/P0_0) : I2C0 pour l'OLED intégré
- GPIO13 (P0_6) : LED RGB
- SPI0 : Réservé pour le module LoRa SX1262
- GPIO7/USER_KEY : Bouton utilisateur

## Installation

### 1. Prérequis

- [PlatformIO](https://platformio.org/) installé (extension VSCode ou CLI)
- Driver USB pour CP2102 (Windows) ou équivalent

### 2. Cloner et compiler

```bash
# Cloner le projet
git clone <repo-url>
cd AB01_SDCARD

# Compiler avec PlatformIO
pio run

# Ou compiler et uploader
pio run -t upload
```

### 3. Configurations disponibles

| Environnement | Description |
|---------------|-------------|
| `cubecell_board_plus` | Configuration par défaut (4 MHz SPI, mode agressif) |
| `cubecell_board_plus_debug` | Debug avec 1 MHz SPI et LOG_LEVEL=4 |
| `cubecell_board_plus_slow_spi` | SPI lent (1 MHz) pour cartes problématiques |
| `cubecell_board_plus_continuous` | Mode continu (fichier reste ouvert) |

Compiler un environnement spécifique :
```bash
pio run -e cubecell_board_plus_debug -t upload
```

## Configuration

Les paramètres peuvent être modifiés dans `platformio.ini` ou `include/config.h` :

| Paramètre | Défaut | Description |
|-----------|--------|-------------|
| `SD_SPI_FREQUENCY` | 4000000 | Fréquence SPI en Hz |
| `CYCLE_INTERVAL_MS` | 1000 | Intervalle entre cycles en ms |
| `AGGRESSIVE_MODE` | 1 | Mode agressif (1) ou continu (0) |
| `MAX_CONSECUTIVE_FAILURES` | 10 | Échecs avant reboot auto |
| `LOG_LEVEL` | 3 | Niveau de log (0-4) |
| `POWER_CYCLE_ENABLED` | 1 | Activer le power-cycle hardware |

## Format du fichier CSV

Le fichier `/sd_test.csv` contient :

```csv
timestamp_ms,cycle,status,error_code,init_time_us,write_time_us,spi_freq_hz,vbat_mv,free_heap
1234,1,OK,0,45000,12000,4000000,4200,8192
2234,2,FAIL,1,0,0,4000000,4180,8192
```

| Colonne | Description |
|---------|-------------|
| timestamp_ms | Temps depuis le boot (ms) |
| cycle | Numéro du cycle |
| status | OK ou FAIL |
| error_code | Code d'erreur (voir config.h) |
| init_time_us | Temps d'initialisation SD (µs) |
| write_time_us | Temps d'écriture (µs) |
| spi_freq_hz | Fréquence SPI utilisée |
| vbat_mv | Tension batterie (mV) |
| free_heap | Mémoire libre (bytes) |

## Monitoring série

Connectez-vous au port série (115200 baud) pour voir :

```
================================
  SD CARD STRESS TEST
  HTCC-AB02 (CubeCell)
================================

Configuration:
  Mode: AGGRESSIVE (unmount each cycle)
  Power cycle: ENABLED
  Cycle interval: 1000 ms
  SPI frequency: 4000 kHz
  Max failures: 10
  CSV file: /sd_test.csv
  Log level: 3

[1234] Cycle 1: OK | Init: 45000us | Write: 12000us | SPI: 4000kHz
[2234] Cycle 2: OK | Init: 44500us | Write: 11800us | SPI: 4000kHz
```

## Contrôle utilisateur

- **Bouton USER (GPIO7)** : Appuyer pour mettre en pause le test
- **LED (GPIO5)** :
  - Flash court = cycle réussi
  - Double flash = erreur
  - LED fixe = test en pause

---

# Plan de validation

## Checklist pré-test

### 1. Préparation matérielle

- [ ] Carte SD formatée en **FAT32** (pas exFAT!)
- [ ] Carte SD de bonne qualité (Class 10 minimum recommandé)
- [ ] Vérifier le câblage selon le tableau ci-dessus
- [ ] Vext connecté au VCC du module SD
- [ ] Alimentation stable de la carte AB02

### 2. Vérification du câblage

```
Multimètre en mode continuité :
- [ ] GND SD ↔ GND AB02 : OK
- [ ] VCC SD ↔ Vext AB02 : OK
- [ ] MOSI SD ↔ GPIO1 : OK
- [ ] MISO SD ↔ GPIO2 : OK
- [ ] SCK SD ↔ GPIO3 : OK
- [ ] CS SD ↔ GPIO4 : OK
```

### 3. Test d'alimentation Vext

```cpp
// Test manuel dans le Serial Monitor :
// Observer le multimètre sur Vext pendant ces commandes
pinMode(Vext, OUTPUT);
digitalWrite(Vext, LOW);   // Vext doit être à ~3.3V
digitalWrite(Vext, HIGH);  // Vext doit être à ~0V
```

## Tests de validation

### Test 1 : Boot et détection SD

**Objectif** : Vérifier que la carte SD est détectée au démarrage

**Procédure** :
1. Flasher le firmware
2. Ouvrir le moniteur série (115200 baud)
3. Appuyer sur RESET

**Critères de succès** :
- [ ] Message "SD controller initialized"
- [ ] Type de carte affiché (SD1/SD2/SDHC)
- [ ] Taille de la carte correcte
- [ ] Premier mount réussi

**En cas d'échec** :
- Vérifier câblage
- Réduire fréquence SPI (`-D SD_SPI_FREQUENCY=1000000`)
- Tester avec une autre carte SD
- Vérifier format FAT32

### Test 2 : Power-cycle Vext

**Objectif** : Vérifier que le power-cycle hardware fonctionne

**Procédure** :
1. Connecter un multimètre sur Vext
2. Observer les variations pendant les cycles

**Critères de succès** :
- [ ] Vext oscille entre 0V et 3.3V
- [ ] Délai visible entre OFF et ON
- [ ] La SD se réinitialise à chaque cycle

### Test 3 : Écriture CSV

**Objectif** : Vérifier l'écriture sur la carte SD

**Procédure** :
1. Laisser tourner 10 cycles
2. Appuyer sur le bouton USER pour pause
3. Retirer la carte SD
4. Lire le fichier sur PC

**Critères de succès** :
- [ ] Fichier `/sd_test.csv` présent
- [ ] En-tête correct
- [ ] 10+ lignes de données
- [ ] Pas de corruption

### Test 4 : Stress test court (1 heure)

**Objectif** : Valider la stabilité sur une durée moyenne

**Configuration** :
```ini
-D CYCLE_INTERVAL_MS=1000
-D AGGRESSIVE_MODE=1
```

**Critères de succès** :
- [ ] > 95% de cycles réussis
- [ ] Pas de reboot automatique
- [ ] Temps d'init et write stables
- [ ] Fichier CSV lisible et complet

### Test 5 : Stress test long (24 heures)

**Objectif** : Valider la stabilité sur une longue durée

**Critères de succès** :
- [ ] > 99% de cycles réussis
- [ ] < 3 fallbacks SPI
- [ ] Pas de corruption de fichier
- [ ] Statistiques cohérentes

## Diagnostic des problèmes courants

### La carte SD n'est pas détectée

1. **Vérifier le câblage** - erreur la plus fréquente
2. **Vérifier le format** - doit être FAT32 (pas exFAT)
3. **Réduire la fréquence SPI** à 1 MHz ou 400 kHz
4. **Tester avec une autre carte SD**
5. **Vérifier que Vext est bien à 3.3V**

### Erreurs d'écriture fréquentes

1. **Carte SD de mauvaise qualité** - essayer une Class 10
2. **Fréquence SPI trop haute** - réduire à 1 MHz
3. **Alimentation instable** - vérifier la source USB
4. **Carte SD pleine ou en fin de vie**

### Reboot automatique en boucle

1. Le test a atteint `MAX_CONSECUTIVE_FAILURES`
2. **Vérifier le câblage**
3. **Réduire `SD_SPI_FREQUENCY`**
4. **Augmenter `MAX_CONSECUTIVE_FAILURES`** temporairement
5. **Désactiver `POWER_CYCLE_ENABLED`** pour debug

### Fichier CSV corrompu

1. **Ne jamais retirer la SD pendant l'écriture**
2. Mettre en pause avec le bouton USER d'abord
3. Vérifier que `sync()` fonctionne
4. Tester en mode continu (`AGGRESSIVE_MODE=0`)

## Résultats attendus

| Métrique | Valeur acceptable |
|----------|-------------------|
| Taux de succès | > 99% |
| Temps init SD | < 100 ms |
| Temps écriture | < 50 ms |
| Fallbacks SPI | < 5 sur 24h |
| RAM libre | > 4 KB |

## Liens utiles

- [Heltec CubeCell Documentation](https://docs.heltec.org/en/node/asr650x/)
- [PlatformIO CubeCell Platform](https://docs.platformio.org/en/latest/platforms/heltec-cubecell.html)
- [HTCC-AB02 Product Page](https://heltec.org/project/htcc-ab02/)

## Licence

MIT License - Voir LICENSE pour plus de détails.
