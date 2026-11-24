# Firmware Vulnérable ESP32 - Phase 1

**Projet:** SUP'COM MIT Project 4 - Attaque et Défense MQTT  
**Phase:** Phase 1 (S1–S3) - Configuration Intentionnellement Vulnérable  
**Auteur:** Eya

---

## 📋 Description

Ce firmware implémente un nœud IoT ESP32 avec capteur DHT11 et contrôle de relais via MQTT **sans aucune mesure de sécurité** (intentionnel pour démonstration).

### Fonctionnalités

✅ Connexion Wi-Fi automatique avec reconnexion non-bloquante  
✅ Client MQTT publiant des données de capteurs (température, humidité)  
✅ Souscription aux commandes de relais/LED  
✅ Sérialisation JSON des payloads  
✅ Diagnostics de performance (temps de connexion, mémoire heap)  
✅ Sortie série détaillée pour monitoring

### Vulnérabilités Intentionnelles ⚠️

❌ Pas de chiffrement TLS (port 1883)  
❌ Pas d'authentification MQTT  
❌ Pas de validation des commandes entrantes  
❌ Trafic en clair (capturable avec Wireshark)  
❌ Sensible aux attaques MitM, replay, DoS

---

## 🔧 Matériel Requis

- ESP32 DevKit (ESP32-WROOM-32)
- DHT11 (température et humidité)
- Module LED ou Relais (5V ou 3.3V compatible)
- Résistance pull-up 10kΩ (DHT11 DATA vers 3.3V)
- Breadboard et câbles jumper
- Câble USB pour programmation

---

## 🔌 Connexions

| Composant | Pin ESP32 | Notes |
|-----------|-----------|-------|
| DHT11 VCC | 3.3V | Ne pas utiliser 5V |
| DHT11 GND | GND | Masse commune |
| DHT11 DATA | GPIO 4 | Pull-up 10kΩ obligatoire |
| Relais IN | GPIO 2 | Active HIGH |
| Relais VCC | 5V (VIN) | Selon datasheet du relais |
| Relais GND | GND | Masse commune |

---

## 🚀 Installation

### 1. Prérequis

- **PlatformIO IDE** (extension VS Code) ou **PlatformIO Core CLI**
- **Driver USB-to-Serial** pour ESP32 :
  - [CP210x Driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
  - [CH340 Driver](https://sparks.gogo.co.nz/ch340.html)

### 2. Cloner le Dépôt

```bash
git clone <URL_DU_DEPOT>
cd MQTT-Attack&Defend/firmware-vulnerable
```

### 3. Configurer les Identifiants

Ouvrir `src/main.cpp` et modifier les lignes suivantes :

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";           // Votre SSID Wi-Fi
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   // Votre mot de passe Wi-Fi
const char* MQTT_BROKER = "192.168.1.100";          // IP du Raspberry Pi (broker Mosquitto)
```

### 4. Compiler et Flasher

```bash
# Installer les dépendances (automatique)
pio lib install

# Compiler le projet
pio run

# Flasher l'ESP32 (connecté via USB)
pio run --target upload

# Monitorer la sortie série
pio device monitor --baud 115200
```

Ou depuis VS Code avec PlatformIO :

1. Ouvrir le dossier `firmware-vulnerable`
2. Cliquer sur l'icône PlatformIO (barre latérale gauche)
3. Project Tasks → esp32dev → General → Upload
4. Project Tasks → esp32dev → General → Monitor

---

## 📊 Sortie Série Exemple

```
========================================
ESP32 MQTT IoT Node - Phase 1 (Vulnerable)
SUP'COM Project 4: Attack & Defense
========================================

[GPIO] Relay pin initialized (GPIO 2)
[DHT11] Sensor initialized (GPIO 4)
[WIFI] Connecting to SSID: MyNetwork
..........
[WIFI] Connected in 2847 ms
[WIFI] IP Address: 192.168.1.50
[WIFI] Signal Strength (RSSI): -45 dBm
[MQTT] Broker configured: 192.168.1.100:1883
[MQTT] Attempting connection to 192.168.1.100:1883...
[MQTT] Connected in 324 ms
[MQTT] Client ID: ESP32_Node_01
[MQTT] Subscribed to: iot/projet4/actionneurs/relais (QoS 1)
[HEAP] Free heap memory: 234567 bytes

[SYSTEM] Initialization complete. Entering main loop...

--- Sensor Reading Cycle ---
[DHT11] Temperature: 24.5°C
[DHT11] Humidity: 55.0%
[PUBLISH] Topic: iot/projet4/capteurs/temperature
[PUBLISH] Payload: {"temp":24.5,"hum":55.0,"uptime":5}
[PUBLISH] ✓ Success
[HEAP] Free heap: 234123 bytes
--- End Sensor Cycle ---
```

---

## 🧪 Tests

### Test 1 : Vérifier la Publication des Données

Sur le Raspberry Pi (ou toute machine du réseau) :

```bash
mosquitto_sub -h 192.168.1.100 -t "iot/projet4/#" -v
```

Vous devriez voir des messages toutes les 5 secondes :

```
iot/projet4/capteurs/temperature {"temp":24.5,"hum":55.0,"uptime":5}
iot/projet4/capteurs/temperature {"temp":24.6,"hum":54.8,"uptime":10}
```

### Test 2 : Contrôler le Relais

Envoyer une commande ON :

```bash
mosquitto_pub -h 192.168.1.100 -t "iot/projet4/actionneurs/relais" -m "ON"
```

Vérifier la confirmation :

```bash
mosquitto_sub -h 192.168.1.100 -t "iot/projet4/actionneurs/relais/state"
# Output: {"relay":"ON","timestamp":15}
```

Envoyer une commande OFF :

```bash
mosquitto_pub -h 192.168.1.100 -t "iot/projet4/actionneurs/relais" -m "OFF"
```

---

## 📈 Métriques de Performance (Baseline)

Le firmware mesure automatiquement :

- **Temps de connexion Wi-Fi** : 2000–4000 ms (typique)
- **Temps de connexion MQTT** : 200–500 ms (sans TLS)
- **Mémoire heap disponible** : ~230–250 KB
- **Fréquence de publication** : 5 secondes (configurable via `SENSOR_INTERVAL`)

Ces métriques seront comparées avec la Phase 2 (TLS) pour évaluer l'impact du chiffrement.

---

## 🔐 Migration vers Phase 2 (Hardened)

Voir le dossier `firmware-hardened/` pour l'implémentation sécurisée avec :

- TLS 1.2/1.3 (port 8883)
- Authentification par certificats X.509
- Validation stricte des payloads
- Logs d'audit

---

## 🐛 Dépannage

### Problème : ESP32 ne se connecte pas au Wi-Fi

- Vérifier le SSID et mot de passe dans `main.cpp`
- Confirmer que le réseau est en 2.4 GHz (ESP32 ne supporte pas 5 GHz)
- Vérifier la force du signal (trop faible = timeout)

### Problème : DHT11 retourne NaN

- Vérifier le câblage (VCC, GND, DATA)
- Confirmer la présence de la résistance pull-up 10kΩ
- Tester le capteur avec un multimètre (DATA doit être ~3.3V au repos)
- Remplacer le capteur si défectueux

### Problème : MQTT connexion échoue (rc=-2)

- Vérifier que Mosquitto est actif sur le Raspberry Pi :
  ```bash
  sudo systemctl status mosquitto
  ```
- Vérifier que le port 1883 est ouvert :
  ```bash
  sudo netstat -tuln | grep 1883
  ```
- Ping le broker depuis l'ESP32 (via le réseau)

### Problème : Relais ne s'active pas

- Vérifier l'alimentation du relais (5V ou 3.3V selon modèle)
- Tester le GPIO 2 manuellement (LED de test)
- Certains modules relais sont "active LOW" → inverser la logique dans le code

---

## 📚 Références

- [PubSubClient Library](https://github.com/knolleary/pubsubclient)
- [Adafruit DHT Library](https://github.com/adafruit/DHT-sensor-library)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)

---

## 📄 Licence

Projet éducatif - SUP'COM MIT Engineering (2026)

