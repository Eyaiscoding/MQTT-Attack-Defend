# Architecture Technique du Projet 4 : Attaque et Défense MQTT

**Auteur:** Eya  
**Formation:** 3ème année MIT Engineering, SUP'COM  
**Date:** Novembre 2026  
**Phase:** Phase 1 (S1–S3) – Configuration Vulnérable

---

## Table des Matières

1. [Vue d'Ensemble de l'Architecture](#1-vue-densemble-de-larchitecture)
2. [Conception des Topics MQTT](#2-conception-des-topics-mqtt)
3. [Justifications Technologiques](#3-justifications-technologiques)
4. [Schéma de Câblage Matériel](#4-schéma-de-câblage-matériel)
5. [Diagramme d'Architecture](#5-diagramme-darchitecture)
6. [Métriques de Performance Baseline](#6-métriques-de-performance-baseline)

---

## 1. Vue d'Ensemble de l'Architecture

### 1.1 Architecture End-to-End

Le système IoT MQTT se compose de trois composants principaux :

```
[Nœud ESP32] ──► [Broker Mosquitto] ──► [Dashboard/Clients]
```

#### Composant 1 : Nœud ESP32 (Edge Device)

- **Rôle:** Nœud capteur-actionneur en périphérie (edge)
- **Microcontrôleur:** ESP32 DevKit (Espressif ESP32-WROOM-32)
- **Capteur:** DHT11 (température et humidité)
- **Actionneur:** Module LED ou Relais (contrôle GPIO)
- **Connectivité:** Wi-Fi 802.11 b/g/n (2.4 GHz)
- **Protocole:** MQTT v3.1.1 sur TCP (port 1883, non chiffré en Phase 1)

**Flux de données:**

- **Publication:** Données de capteurs (température, humidité, uptime) → `iot/projet4/capteurs/temperature` toutes les 5 secondes
- **Souscription:** Commandes d'actionneur depuis `iot/projet4/actionneurs/relais`
- **Confirmation:** État du relais publié vers `iot/projet4/actionneurs/relais/state`

#### Composant 2 : Broker MQTT (Raspberry Pi - géré par Malak)

- **Plateforme:** Raspberry Pi 3B+ ou 4 (Raspbian/Raspberry Pi OS)
- **Broker:** Eclipse Mosquitto (open-source MQTT broker)
- **Configuration Phase 1:**
  - Port d'écoute: 1883 (TCP non chiffré)
  - Authentification: Désactivée (accès anonyme)
  - Persistence: Activée (messages QoS 1/2)
  - Log level: Debug (pour analyse des attaques)

**Rôle:**

- Routage des messages entre publishers et subscribers
- Gestion des souscriptions par wildcard (`iot/projet4/#`)
- Stockage des messages retained (état du système)

#### Composant 3 : Dashboard/Clients

- **Outils de monitoring:**
  - MQTT Explorer (GUI pour visualisation en temps réel)
  - `mosquitto_sub` (ligne de commande)
  - Node-RED (dashboard personnalisé, optionnel)
- **Outils de contrôle:**
  - `mosquitto_pub` (envoi de commandes)
  - Interface web custom (Phase 2)

---

## 2. Conception des Topics MQTT

### 2.1 Hiérarchie et Conventions

La structure des topics suit les meilleures pratiques MQTT pour la scalabilité et la clarté :

```
iot/projet4/{catégorie}/{type}/{identifiant}
```

**Rationale:**

- `iot/` : Namespace global pour séparer du trafic non-IoT
- `projet4/` : Identifiant du projet (permet plusieurs projets sur le même broker)
- `{catégorie}` : `capteurs`, `actionneurs`, `status`, `config`
- `{type}` : Type de capteur ou actionneur spécifique
- `{identifiant}` : (Optionnel) Pour multi-devices (ex: `node01`, `node02`)

### 2.2 Topics Définis

#### Topics de Capteurs (ESP32 → Broker)

| Topic | QoS | Retained | Payload Example | Fréquence |
|-------|-----|----------|-----------------|-----------|
| `iot/projet4/capteurs/temperature` | 0 | Non | `{"temp":24.5,"hum":55.0,"uptime":120}` | 5 sec |
| `iot/projet4/status` | 1 | Oui | `{"state":"online","ip":"192.168.1.50","heap":234567}` | À la connexion |

**Justification QoS:**

- **QoS 0** pour données de capteurs : Les lectures DHT11 sont périodiques (5s), donc une perte occasionnelle est acceptable (fire-and-forget)
- **QoS 1** pour status : Garantit que l'état de connexion est reçu au moins une fois
- **Retained flag** sur status : Le dernier état connu reste disponible pour les nouveaux subscribers

#### Topics d'Actionneurs (Broker → ESP32)

| Topic | QoS | Payload Example | Action |
|-------|-----|-----------------|--------|
| `iot/projet4/actionneurs/relais` | 1 | `ON`, `OFF`, `1`, `0` | Active/désactive le relais |

**Justification QoS 1:**

- Les commandes d'actionneur doivent être reçues au moins une fois (at-least-once delivery)
- Évite la perte de commandes critiques (ex: désactiver un chauffage)

#### Topics de Confirmation (ESP32 → Broker)

| Topic | QoS | Retained | Payload Example |
|-------|-----|----------|-----------------|
| `iot/projet4/actionneurs/relais/state` | 1 | Oui | `{"relay":"ON","timestamp":1700000000}` |

**Justification Retained:**

- Le dernier état confirmé du relais reste accessible aux clients qui se connectent après le changement d'état

### 2.3 Extensibilité Future

Pour la Phase 2 et l'ajout de nœuds supplémentaires, la structure sera étendue :

```
iot/projet4/capteurs/{node_id}/temperature
iot/projet4/actionneurs/{node_id}/relais
```

Exemple avec plusieurs ESP32 :

```
iot/projet4/capteurs/esp32-01/temperature
iot/projet4/capteurs/esp32-02/temperature
iot/projet4/actionneurs/esp32-01/relais
```

---

## 3. Justifications Technologiques

### 3.1 Choix de l'ESP32

**Alternatives considérées:** Arduino Uno + ESP8266, Raspberry Pi Pico W, STM32 + Wi-Fi module

**Justification du choix ESP32:**

| Critère | ESP32 | Alternatives |
|---------|-------|--------------|
| **Wi-Fi intégré** | ✅ 802.11 b/g/n natif | ⚠️ ESP8266 : Wi-Fi mais MCU 32-bit limité<br>❌ Arduino Uno : Module externe requis |
| **Puissance de calcul** | ✅ Dual-core Xtensa 240 MHz | ⚠️ Pico W : Single-core ARM 133 MHz<br>❌ Uno : 8-bit 16 MHz |
| **Support TLS/SSL** | ✅ **Accélération matérielle** (AES, SHA, RSA) | ⚠️ ESP8266 : TLS software (lent, RAM limitée)<br>⚠️ Pico W : TLS software uniquement |
| **Mémoire** | ✅ 520 KB SRAM, 4 MB Flash | ❌ Uno : 2 KB SRAM (insuffisant pour TLS)<br>⚠️ ESP8266 : 80 KB SRAM (juste suffisant) |
| **GPIOs** | ✅ 34 GPIOs (ADC, DAC, PWM, I2C, SPI) | ✅ Pico W : 26 GPIOs<br>⚠️ Uno : 14 GPIOs limités |
| **Coût** | ✅ ~5–8 EUR | ✅ Similaire pour toutes les options |
| **Écosystème** | ✅ Arduino IDE + PlatformIO + ESP-IDF | ✅ Bon support pour toutes les options |
| **Consommation** | ⚠️ ~240 mA en Wi-Fi actif | ✅ Pico W : ~150 mA (meilleur pour batterie) |

**Conclusion:** L'ESP32 est le choix optimal pour ce projet car :

1. **Crypto hardware acceleration** essentielle pour Phase 2 (TLS 1.2/1.3 avec latence acceptable)
2. Dual-core permet de gérer MQTT et lecture de capteurs sans blocking
3. Mémoire suffisante pour gérer les certificats X.509 en Phase 2
4. Large adoption dans l'industrie IoT (documentation, exemples, bibliothèques matures)

### 3.2 Choix du Capteur DHT11

**Alternatives considérées:** DHT22, BME280, DS18B20

**Justification:**

| Critère | DHT11 | DHT22 | BME280 |
|---------|-------|-------|--------|
| **Précision** | ⚠️ ±2°C, ±5% HR | ✅ ±0.5°C, ±2% HR | ✅ ±1°C, ±3% HR |
| **Fréquence d'échantillonnage** | ⚠️ 1 Hz (1 lecture/sec max) | ✅ 0.5 Hz | ✅ Configurable jusqu'à 1000 Hz |
| **Coût** | ✅ ~2 EUR | ⚠️ ~5 EUR | ⚠️ ~8 EUR |
| **Interface** | ✅ 1-wire simple | ✅ 1-wire | ⚠️ I2C/SPI (plus complexe) |
| **Disponibilité** | ✅ Très courant | ✅ Courant | ⚠️ Moins courant localement |

**Conclusion:** Le DHT11 est suffisant pour ce projet éducatif car :

1. Précision acceptable pour une démonstration MQTT (non critique)
2. Interface simple (1-wire) réduit la complexité du câblage
3. Coût minimal (budget étudiant)
4. Fréquence d'échantillonnage (1 Hz) adaptée à notre publication toutes les 5 secondes

**Note:** Pour une application de production (monitoring de salle serveur, serre agricole), le BME280 serait préférable.

### 3.3 Choix de MQTT vs Autres Protocoles IoT

**Alternatives considérées:** HTTP REST, CoAP, WebSocket, LoRaWAN

**Comparaison:**

| Critère | MQTT | HTTP REST | CoAP | WebSocket |
|---------|------|-----------|------|-----------|
| **Overhead** | ✅ 2 bytes header min | ❌ ~200 bytes headers | ✅ 4 bytes header | ⚠️ ~20 bytes frame |
| **Pub/Sub natif** | ✅ Oui | ❌ Polling requis | ⚠️ Observe (limité) | ⚠️ Implémentation custom |
| **QoS multi-niveaux** | ✅ 0, 1, 2 | ❌ Non (retries app-level) | ✅ CON/NON | ❌ Non |
| **Retained messages** | ✅ Oui | ❌ Non | ❌ Non | ❌ Non |
| **Batterie** | ✅ Optimisé (keep-alive) | ❌ Connection overhead | ✅ Optimisé UDP | ⚠️ Connection TCP persistante |
| **Firewall-friendly** | ✅ Port 1883/8883 standard | ✅ Port 80/443 | ⚠️ UDP 5683 (bloqué parfois) | ✅ Port 80/443 |
| **TLS/SSL** | ✅ Natif (port 8883) | ✅ HTTPS | ✅ DTLS | ✅ WSS |

**Justification MQTT:**

1. **Publish/Subscribe décentralisé** : Le broker gère le routage, les clients ne se connaissent pas directement (loose coupling)
2. **Efficacité réseau** : Header minimal (crucial pour IoT limité en bande passante)
3. **QoS flexible** : Permet d'adapter la garantie de livraison selon le type de données
4. **Last Will & Testament** : Le broker peut notifier automatiquement la déconnexion d'un nœud
5. **Maturité** : Standard OASIS, implémentations robustes (Mosquitto, HiveMQ, EMQX)

**Cas d'usage HTTP REST refusés:**

- Polling nécessaire côté client (gaspillage de bande passante)
- Overhead significatif pour chaque requête (headers HTTP volumineux)
- Pas de notification push native (Server-Sent Events = workaround)

**Cas d'usage CoAP refusés:**

- Moins mature que MQTT dans l'écosystème IoT
- Pas de broker standardisé (implémentation plus complexe)
- UDP peut être problématique sur certains réseaux (firewall, NAT)

---

## 4. Schéma de Câblage Matériel

### 4.1 Table de Connexions

| Composant | Pin Composant | Pin ESP32 | Type Signal | Notes |
|-----------|---------------|-----------|-------------|-------|
| **DHT11** | | | | |
| | VCC | 3.3V | Alimentation | ⚠️ Ne pas utiliser 5V (risque d'endommager le capteur) |
| | GND | GND | Masse | |
| | DATA | GPIO 4 | Digital I/O | Résistance pull-up 10kΩ vers 3.3V **obligatoire** |
| **LED/Relais** | | | | |
| | Signal/IN | GPIO 2 | Digital Output | Active HIGH (3.3V = ON) |
| | VCC | 5V (VIN) | Alimentation | Selon le module (vérifier datasheet) |
| | GND | GND | Masse | Masse commune avec ESP32 |

### 4.2 Notes de Câblage

#### DHT11 : Résistance Pull-up Obligatoire

Le DHT11 utilise un protocole 1-wire qui nécessite une résistance pull-up :

```
3.3V ──┬─── [10kΩ] ──┬─── DHT11 DATA ─── GPIO 4 (ESP32)
       │               │
       └─── DHT11 VCC  └─── (Pull-up vers 3.3V)
```

**Raison:** Le DHT11 communique via un bus bidirectionnel open-drain. Sans pull-up, le signal reste flottant et la communication échoue.

**Solution:** La plupart des modules DHT11 breakout incluent déjà cette résistance. Vérifier avec un multimètre avant d'en ajouter une seconde.

#### Module Relais : Isolation et Alimentation

**Type de relais:** Relais optoisolé 5V (recommandé pour sécurité)

```
ESP32 GPIO 2 (3.3V) ──► [Transistor NPN + Optocoupler] ──► Bobine Relais (5V)
```

**Alimentation:**

- **Option 1:** Relais module 5V ⇒ Alimenter depuis VIN de l'ESP32 (via USB = 5V)
- **Option 2:** Relais 3.3V ⇒ Alimenter depuis 3.3V de l'ESP32
- **⚠️ Attention:** Ne **jamais** alimenter un relais 5V depuis la pin 3.3V (risque de brownout de l'ESP32)

**LED simple (alternative au relais):**

Pour les tests sans charge AC :

```
GPIO 2 ──► [220Ω] ──► LED (anode) ──► (cathode) ──► GND
```

### 4.3 Schéma Fritzing (Description Textuelle)

```
                    ┌─────────────────┐
                    │   ESP32 DevKit  │
                    │                 │
   DHT11            │  GPIO 4  ◄──────┼──── DATA (avec pull-up 10kΩ → 3.3V)
   ┌─────┐          │  3.3V    ───────┼──── VCC
   │  ○  │          │  GND     ───────┼──── GND
   │ ○○○ │          │                 │
   └─────┘          │  GPIO 2  ───────┼──── → [Relais Module]
                    │  5V/VIN  ───────┼──── → VCC Relais
                    │  GND     ───────┼──── → GND Relais
                    │                 │
                    │  USB     ◄──────┼──── Programmation + Alimentation
                    └─────────────────┘
```

**⚠️ Checklist avant le premier branchement:**

1. ✅ Vérifier la polarité VCC/GND (risque de court-circuit)
2. ✅ Confirmer que le DHT11 a sa résistance pull-up (multimètre : ~10kΩ entre DATA et VCC)
3. ✅ Tester la LED/Relais séparément avant de connecter à l'ESP32
4. ✅ Ne pas connecter de charge AC au relais pendant les tests initiaux

---

## 5. Diagramme d'Architecture

### 5.1 Architecture en Couches

```
┌──────────────────────────────────────────────────────────────────┐
│                        COUCHE APPLICATION                         │
│  (Dashboard, Node-RED, Logs, Wireshark pour capture de trafic)  │
└────────────────────────────┬─────────────────────────────────────┘
                             │ MQTT Subscribe/Publish
┌────────────────────────────▼─────────────────────────────────────┐
│                       COUCHE MIDDLEWARE                           │
│            Eclipse Mosquitto MQTT Broker (Raspberry Pi)           │
│  - Routage de messages (pub/sub)                                 │
│  - Gestion QoS (0, 1, 2)                                          │
│  - Persistence (retained messages)                                │
│  - [Phase 1] Port 1883 TCP non chiffré, accès anonyme            │
└────────────────────────────┬─────────────────────────────────────┘
                             │ TCP/IP (Wi-Fi 802.11n)
┌────────────────────────────▼─────────────────────────────────────┐
│                        COUCHE PÉRIPHÉRIQUE                        │
│                      ESP32 Node (Edge Device)                     │
│  ┌────────────────────────────────────────────────────────┐      │
│  │  Firmware (PlatformIO + Arduino Framework)             │      │
│  │  - Connexion Wi-Fi (non-blocking reconnect)            │      │
│  │  - Client MQTT (PubSubClient library)                  │      │
│  │  - Lecture DHT11 (5s interval)                         │      │
│  │  - Contrôle GPIO relais (callback)                     │      │
│  └────────────────────────────────────────────────────────┘      │
│  ┌──────────────┐  ┌──────────────┐                              │
│  │  DHT11       │  │  LED/Relais  │                              │
│  │  (Capteur)   │  │  (Actionneur)│                              │
│  └──────────────┘  └──────────────┘                              │
└──────────────────────────────────────────────────────────────────┘
```

### 5.2 Flux de Données Détaillé

#### Flux 1 : Publication de Données Capteur

```
┌─────────┐                                    ┌──────────┐                     ┌───────────┐
│  DHT11  │                                    │  ESP32   │                     │ Mosquitto │
└────┬────┘                                    └────┬─────┘                     └─────┬─────┘
     │                                              │                                 │
     │  1. Lecture température/humidité             │                                 │
     │     (protocole 1-wire, durée ~18ms)          │                                 │
     ├─────────────────────────────────────────────►│                                 │
     │                                              │                                 │
     │                                              │  2. Sérialisation JSON           │
     │                                              │     {"temp":24.5,"hum":55.0}    │
     │                                              │                                 │
     │                                              │  3. MQTT PUBLISH                │
     │                                              │     Topic: iot/projet4/capteurs/temperature
     │                                              │     QoS: 0, Retain: false       │
     │                                              ├────────────────────────────────►│
     │                                              │                                 │
     │                                              │                                 │  4. Routage vers
     │                                              │                                 │     subscribers
     │                                              │                                 │     (Dashboard, etc.)
     │                                              │                                 ├─────────────►
     │  5. Attendre 5 secondes                      │                                 │
     │  (loop delay)                                │                                 │
     │                                              │                                 │
```

**Timing:**

- Lecture DHT11 : ~18 ms (protocole)
- Sérialisation JSON : ~2 ms
- Publication MQTT : ~5–15 ms (dépend de la latence Wi-Fi)
- **Total par cycle : ~25–35 ms** (bien inférieur au cycle de 5 secondes)

#### Flux 2 : Réception de Commande Actionneur

```
┌───────────┐                     ┌──────────┐                     ┌──────────┐
│ Dashboard │                     │ Mosquitto│                     │  ESP32   │
└─────┬─────┘                     └────┬─────┘                     └────┬─────┘
      │                                │                                │
      │  1. Utilisateur envoie "ON"    │                                │
      │     mosquitto_pub -t ...       │                                │
      ├───────────────────────────────►│                                │
      │                                │                                │
      │                                │  2. MQTT PUBLISH Forward       │
      │                                │     Topic: iot/projet4/actionneurs/relais
      │                                │     Payload: "ON"              │
      │                                │     QoS: 1                     │
      │                                ├───────────────────────────────►│
      │                                │                                │
      │                                │                                │  3. Callback déclenché
      │                                │                                │     messageReceived()
      │                                │                                │
      │                                │                                │  4. Parsing payload
      │                                │                                │     if (payload == "ON")
      │                                │                                │
      │                                │                                │  5. digitalWrite(GPIO_RELAY, HIGH)
      │                                │                                │     ───► Relais activé
      │                                │                                │
      │                                │  6. Confirmation PUBLISH       │
      │                                │     Topic: .../relais/state    │
      │                                │     Payload: {"relay":"ON"}    │
      │                                ◄───────────────────────────────┤
      │                                │                                │
      │  7. Réception confirmation     │                                │
      ◄───────────────────────────────┤                                │
      │                                │                                │
```

**Latence totale** (dashboard → activation physique) : **~50–100 ms** (Phase 1, réseau local)

---

## 6. Métriques de Performance Baseline

### 6.1 Objectif des Mesures

Établir une **baseline de performance Phase 1** (sans TLS) pour comparaison avec Phase 2 (TLS activé). Cela permettra de quantifier l'impact du chiffrement.

### 6.2 Métriques Capturées par le Firmware

Le code ESP32 inclut des mesures automatiques via le port série :

```cpp
// Exemple de sortie série attendue :
[BOOT] ESP32 IoT Node Starting...
[WIFI] Connecting to SSID: MyNetwork
[WIFI] Connected in 2847 ms | IP: 192.168.1.50
[MQTT] Connecting to broker: 192.168.1.100:1883
[MQTT] Connected in 324 ms
[HEAP] Free heap: 234567 bytes
[SENSOR] DHT11 read: temp=24.5°C, hum=55.0%
[PUBLISH] Topic: iot/projet4/capteurs/temperature | Payload: {"temp":24.5,"hum":55.0,"uptime":5}
```

**Métriques extraites:**

| Métrique | Valeur Typique Phase 1 | Méthode de Mesure |
|----------|------------------------|-------------------|
| Temps de connexion Wi-Fi | 2000–4000 ms | `millis()` avant/après `WiFi.begin()` |
| Temps de connexion MQTT | 200–500 ms | `millis()` avant/après `mqttClient.connect()` |
| Free Heap (RAM disponible) | 230–250 KB | `ESP.getFreeHeap()` |
| Latence publication (locale) | 10–30 ms | Timestamp ESP32 vs timestamp broker (logs Mosquitto) |
| Période d'échantillonnage | 5000 ms | Valide par logs |

### 6.3 Métriques Côté Broker (Mosquitto)

Activer les logs détaillés dans `/etc/mosquitto/mosquitto.conf` :

```conf
log_type all
log_timestamp true
```

Analyser avec :

```bash
sudo tail -f /var/log/mosquitto/mosquitto.log | grep "iot/projet4"
```

**Métriques broker:**

- Nombre de messages reçus/routés par seconde
- Nombre de clients connectés simultanément
- Taille moyenne des payloads MQTT

### 6.4 Benchmarks Attendus Phase 2 (TLS)

**Hypothèse:** L'ajout de TLS augmentera :

- **Temps de connexion MQTT** : +500–1500 ms (handshake TLS, échange de certificats)
- **Latence de publication** : +5–15 ms (chiffrement/déchiffrement AES-128-GCM)
- **Consommation RAM** : -20–40 KB (buffers TLS, stack de certificats)

Ces valeurs seront validées en S4–S5 lors de l'implémentation TLS.

### 6.5 Outils de Mesure Externe

#### Wireshark : Capture de Trafic MQTT

**Phase 1 (démonstration de vulnérabilité) :**

1. Capturer sur l'interface Wi-Fi du Raspberry Pi :
   ```bash
   sudo tcpdump -i wlan0 -w mqtt_capture.pcap port 1883
   ```

2. Analyser avec Wireshark :
   - Filtre : `mqtt`
   - Vérifier que les payloads JSON sont **en clair** (preuve de non-chiffrement)

**Phase 2 :**

- Capturer sur port 8883 (TLS)
- Vérifier que les payloads sont chiffrés (Application Data opaque)

#### MQTT Explorer : Monitoring Temps Réel

- Visualisation graphique des topics
- Mesure de la fréquence de réception (doit être ~0.2 Hz, soit 1 message/5s)
- Validation du format JSON

---

## 7. Vulnérabilités Phase 1 (Intentionnelles)

### 7.1 Liste des Faiblesses de Sécurité

| Vulnérabilité | Impact | Démonstration Prévue (S6) |
|---------------|--------|---------------------------|
| **Pas de chiffrement** | Données en clair sur le réseau | Wireshark capture de payloads JSON |
| **Pas d'authentification** | N'importe qui peut publier/souscrire | `mosquitto_pub` depuis machine externe |
| **Pas de validation** | Commandes malformées acceptées | Envoyer `"relais": "MALICIOUS"` |
| **Pas de rate limiting** | Vulnérable au flooding | Script Python envoyant 1000 msg/sec |
| **Pas d'audit logs** | Impossible de tracer les attaquants | Pas de logs d'authentification |

### 7.2 Scénarios d'Attaque Possibles

1. **Man-in-the-Middle (MitM) :**
   - Attaquant en écoute passive capture les données de température
   - Exploitation : Espionnage industriel, vol de données IoT

2. **Command Injection :**
   - Attaquant publie sur `iot/projet4/actionneurs/relais` sans autorisation
   - Exploitation : Activation non autorisée d'équipements (portes, alarmes)

3. **Replay Attack :**
   - Capturer une commande légitime et la rejouer plus tard
   - Exploitation : Réactivation d'un relais désactivé

4. **Denial of Service (DoS) :**
   - Flooding du broker avec des milliers de publications
   - Exploitation : Rendre le système IoT indisponible

---

## 8. Conclusion et Prochaines Étapes

### Phase 1 (S1–S3) : Objectifs Atteints

✅ Architecture système documentée et justifiée  
✅ Topics MQTT structurés et extensibles  
✅ Firmware ESP32 fonctionnel avec diagnostics  
✅ Baseline de performance établie  

### Phase 2 (S4–S5) : Travail à Venir

- Génération de certificats CA + certificats clients (OpenSSL)
- Configuration Mosquitto avec TLS et authentification par certificats
- Firmware durci avec `WiFiClientSecure` et validation de certificats
- Benchmarks comparatifs Phase 1 vs Phase 2
- Tests d'attaque et validation de la résilience

---

**Document maintenu par:** Eya  
**Dernière mise à jour:** Novembre 2026 (Phase 1)  
**Version:** 1.0

