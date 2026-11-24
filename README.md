# Projet 4: Attaque et Défense d'une Chaîne MQTT

## 📋 Vue d'Ensemble du Projet

Ce projet explore la sécurité des systèmes IoT basés sur MQTT à travers deux phases :

- **Phase 1 (S1–S3) : Configuration Vulnérable** – Système intentionnellement non sécurisé (sans chiffrement, accès anonyme)
- **Phase 2 (S4–S5) : Configuration Durcie** – Implémentation de TLS/SSL, authentification, et durcissement de la sécurité

### Architecture Système

```
┌─────────────────┐          MQTT (TCP/1883 ou TLS/8883)          ┌──────────────────┐
│   ESP32 Node    │ ───────────────────────────────────────────► │  Mosquitto Broker│
│  (DHT11 + LED)  │ ◄─────────────────────────────────────────── │  (Raspberry Pi)  │
└─────────────────┘                                                └──────────────────┘
     Sensor Data                                                           │
     + Actuator Control                                                    │
                                                                           ▼
                                                                   ┌──────────────────┐
                                                                   │  MQTT Dashboard  │
                                                                   │   (Monitoring)   │
                                                                   └──────────────────┘
```

---

## 🗂️ Structure du Dépôt

```
MQTT-Attack&Defend/
│
├── firmware-vulnerable/        # Code ESP32 Phase 1 (non sécurisé)
│   ├── src/
│   │   └── main.cpp           # Firmware principal ESP32
│   ├── include/
│   ├── platformio.ini         # Configuration PlatformIO
│   └── README.md
│
├── firmware-hardened/          # Code ESP32 Phase 2 (TLS + Auth)
│   └── README.md              # Placeholder pour S4–S5
│
├── broker-vulnerable/          # Config Mosquitto Phase 1
│   └── README.md              # Placeholder (géré par Malak)
│
├── broker-hardened/            # Config Mosquitto Phase 2
│   └── README.md              # Placeholder (géré par Malak)
│
├── docs/                       # Documentation technique
│   ├── architecture_eya.md    # Architecture détaillée
│   ├── wiring_diagram.png     # (À ajouter) Schéma de câblage
│   └── measurements/          # Logs de performance et benchmarks
│
├── .gitignore
└── README.md                   # Ce fichier
```

---

## 📡 Hiérarchie des Topics MQTT

Le système utilise une structure de topics claire et hiérarchique :

### Topics de Capteurs (Publication par l'ESP32)

| Topic | Payload | Fréquence | Description |
|-------|---------|-----------|-------------|
| `iot/projet4/capteurs/temperature` | `{"temp": 24.5, "hum": 55.0, "uptime": 120}` | 5 sec | Données DHT11 (température + humidité) |
| `iot/projet4/status` | `{"state": "online", "ip": "192.168.1.50", "heap": 234567}` | À la connexion | État du nœud ESP32 |

### Topics d'Actionneurs (Souscription par l'ESP32)

| Topic | Payload | Description |
|-------|---------|-------------|
| `iot/projet4/actionneurs/relais` | `ON` / `OFF` / `1` / `0` | Commande du relais/LED |

### Topics de Réponse (Publication par l'ESP32)

| Topic | Payload | Description |
|-------|---------|-------------|
| `iot/projet4/actionneurs/relais/state` | `{"relay": "ON", "timestamp": 1234567890}` | Confirmation d'état du relais |

---

## 🔧 Matériel Utilisé

### Nœud ESP32 (Eya)

- **Microcontrôleur:** ESP32 DevKit (Dual-core, Wi-Fi + Bluetooth, support TLS hardware)
- **Capteur:** DHT11 (Température et humidité)
- **Actionneur:** Module LED ou Relais (contrôle distant)
- **Accessoires:** Breadboard, jumper wires, câble USB

### Configuration du Broker (Malak)

- **Plateforme:** Raspberry Pi 3/4
- **Broker:** Mosquitto MQTT
  - **Phase 1:** Port 1883 (TCP non chiffré), accès anonyme
  - **Phase 2:** Port 8883 (TLS/SSL), authentification par certificats

---

## 🔌 Schéma de Câblage ESP32

| Composant | Pin ESP32 | Notes |
|-----------|-----------|-------|
| DHT11 VCC | 3.3V | Alimentation capteur |
| DHT11 GND | GND | Masse |
| DHT11 Data | GPIO 4 | Pin de données (avec pull-up 10kΩ) |
| LED/Relais Signal | GPIO 2 | Sortie digitale (active HIGH) |
| LED/Relais VCC | 5V / VIN | Selon le module (utiliser transistor si nécessaire) |
| LED/Relais GND | GND | Masse commune |

**⚠️ Important:** Le DHT11 nécessite une résistance pull-up de 10kΩ entre VCC et Data (souvent intégrée sur les modules breakout).

---

## 🚀 Installation et Compilation

### Prérequis

1. **PlatformIO** (VS Code Extension ou CLI)
2. **Driver USB-to-Serial** pour ESP32 (CP210x ou CH340)
3. **Python 3.x** (pour PlatformIO)

### Étapes de Build

```bash
# 1. Cloner le dépôt
git clone <VOTRE_REPO_GITHUB>
cd MQTT-Attack&Defend

# 2. Naviguer vers le firmware vulnérable
cd firmware-vulnerable

# 3. Installer les dépendances (automatique via PlatformIO)
pio lib install

# 4. Configurer les identifiants Wi-Fi et MQTT
# Éditer src/main.cpp et modifier :
#   - WIFI_SSID
#   - WIFI_PASSWORD
#   - MQTT_BROKER (adresse IP du Raspberry Pi)

# 5. Compiler le projet
pio run

# 6. Flasher l'ESP32 (connecté via USB)
pio run --target upload

# 7. Monitorer la sortie série
pio device monitor --baud 115200
```

### Configuration Wi-Fi & MQTT

Avant de flasher, modifier les constantes dans `firmware-vulnerable/src/main.cpp` :

```cpp
const char* WIFI_SSID = "VotreSSID";
const char* WIFI_PASSWORD = "VotreMotDePasse";
const char* MQTT_BROKER = "192.168.1.100"; // IP du Raspberry Pi
const int MQTT_PORT = 1883;
```

---

## 📊 Métriques de Performance (Baseline Phase 1)

Le firmware vulnérable inclut des diagnostics pour établir une baseline avant l'ajout de TLS en Phase 2 :

- **Temps de connexion Wi-Fi** (ms)
- **Temps de connexion MQTT** (ms)
- **Mémoire Heap disponible** (bytes)
- **Latence de publication** (mesurée côté broker)

Ces métriques seront comparées avec Phase 2 pour évaluer l'impact du chiffrement TLS.

---

## 🧪 Tests de Phase 1

### Test 1: Connexion et Publication

```bash
# Sur le Raspberry Pi, surveiller les topics
mosquitto_sub -h localhost -t "iot/projet4/#" -v

# Vérifier la réception des données toutes les 5 secondes :
# iot/projet4/capteurs/temperature {"temp":24.5,"hum":55.0,"uptime":120}
```

### Test 2: Contrôle de l'Actionneur

```bash
# Depuis n'importe quelle machine du réseau
mosquitto_pub -h <IP_RASPBERRY> -t "iot/projet4/actionneurs/relais" -m "ON"
mosquitto_pub -h <IP_RASPBERRY> -t "iot/projet4/actionneurs/relais" -m "OFF"

# Vérifier la LED/Relais physique et la confirmation :
# iot/projet4/actionneurs/relais/state {"relay":"ON","timestamp":1234567890}
```

### Test 3: Vulnérabilités (Phase 1)

**⚠️ Ces vulnérabilités sont intentionnelles pour la démonstration :**

- Trafic MQTT en clair (Wireshark peut capturer les messages)
- Pas d'authentification (n'importe qui peut publier/souscrire)
- Pas de validation des commandes
- Sujet à Man-in-the-Middle, replay attacks, etc.

---

## 🔐 Phase 2: Durcissement (S4–S5)

La Phase 2 implémentera :

1. **Chiffrement TLS 1.2/1.3** (port 8883)
2. **Authentification par certificats X.509** (CA, client cert)
3. **Validation stricte des payloads** (JSON schema)
4. **Rate limiting** côté broker
5. **Logs d'audit** et détection d'anomalies

Voir `firmware-hardened/README.md` pour les détails (à venir).

---

## 📚 Références Techniques

- **MQTT Specification:** [OASIS MQTT v3.1.1](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)
- **ESP32 Datasheet:** [Espressif ESP32](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
- **PubSubClient Library:** [knolleary/pubsubclient](https://github.com/knolleary/pubsubclient)
- **DHT Sensor Library:** [Adafruit DHT](https://github.com/adafruit/DHT-sensor-library)
- **Mosquitto Documentation:** [Eclipse Mosquitto](https://mosquitto.org/documentation/)

---

## 👥 Contributeurs

- **Eya** – Nœud ESP32 (capteur + actionneur), firmware, documentation
- **Malak** – Configuration broker Mosquitto (Raspberry Pi), tests réseau
