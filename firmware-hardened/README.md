# Firmware Durci ESP32 - Phase 2 (À Venir)

**Projet:** SUP'COM MIT Project 4 - Attaque et Défense MQTT  
**Phase:** Phase 2 (S4–S5) - Configuration Sécurisée avec TLS  
**Auteur:** Eya  
**Statut:** 🚧 **EN DÉVELOPPEMENT** (prévu pour S4–S5)

---

## 📋 Aperçu

Ce dossier contiendra le firmware ESP32 **sécurisé** avec chiffrement TLS/SSL, authentification par certificats, et validation stricte des payloads.

---

## 🔐 Fonctionnalités Prévues (Phase 2)

### Sécurité Réseau

✅ **Chiffrement TLS 1.2/1.3** (port 8883 au lieu de 1883)  
✅ **Authentification par certificats X.509** (CA + client cert + client key)  
✅ **Validation du certificat serveur** (vérification de la chaîne CA)  
✅ **Perfect Forward Secrecy (PFS)** via ECDHE

### Sécurité Applicative

✅ **Validation des payloads JSON** (schéma strict)  
✅ **Timestamps anti-replay** (rejet des commandes anciennes)  
✅ **Rate limiting côté client** (prévention du flooding)  
✅ **Logs d'audit** (traçabilité des commandes)

### Hardware Security

✅ **Stockage sécurisé des clés** (SPIFFS chiffré ou NVS)  
✅ **Secure Boot** (optionnel, dépend du temps disponible)  
✅ **Watchdog timer** renforcé

---

## 🗂️ Structure Prévue

```
firmware-hardened/
├── src/
│   ├── main.cpp                # Firmware principal avec TLS
│   ├── mqtt_secure.cpp         # Wrapper MQTT sécurisé
│   └── cert_manager.cpp        # Gestion des certificats
├── include/
│   ├── config.h                # Configuration (sans secrets)
│   ├── certs.h                 # Headers de certificats (généré)
│   └── mqtt_secure.h
├── certs/                      # Certificats (non commités dans Git)
│   ├── ca_cert.pem            # Certificat de l'autorité de certification
│   ├── client_cert.pem        # Certificat client ESP32
│   └── client_key.pem         # Clé privée client
├── platformio.ini              # Configuration avec TLS build flags
└── README.md                   # Ce fichier
```

---

## 🔑 Génération des Certificats

Les certificats seront générés avec **OpenSSL** côté Raspberry Pi, puis convertis en format C header pour l'ESP32.

### Étapes Prévues

```bash
# 1. Générer la CA (Certificate Authority)
openssl req -new -x509 -days 365 -extensions v3_ca \
  -keyout ca_key.pem -out ca_cert.pem

# 2. Générer la clé privée client
openssl genrsa -out client_key.pem 2048

# 3. Générer la CSR (Certificate Signing Request) client
openssl req -new -key client_key.pem -out client.csr

# 4. Signer le certificat client avec la CA
openssl x509 -req -in client.csr -CA ca_cert.pem \
  -CAkey ca_key.pem -CAcreateserial -out client_cert.pem -days 365

# 5. Convertir les certificats en headers C (script Python)
python3 cert_to_header.py ca_cert.pem > include/certs.h
```

---

## 📊 Comparaison Phase 1 vs Phase 2

| Métrique | Phase 1 (Vulnérable) | Phase 2 (Durci) | Impact |
|----------|----------------------|------------------|--------|
| **Port MQTT** | 1883 (non chiffré) | 8883 (TLS) | - |
| **Temps connexion MQTT** | 200–500 ms | 700–1500 ms | +300–1000 ms |
| **Latence publication** | 10–30 ms | 15–45 ms | +5–15 ms |
| **Mémoire heap** | 230–250 KB | 190–210 KB | -40 KB |
| **Consommation CPU** | Faible | Moyenne | +20–30% |
| **Sécurité** | ❌ Aucune | ✅ Haute | 🔐 |

---

## 🛠️ Technologies Utilisées

- **WiFiClientSecure** (ESP32 built-in) : Client TLS/SSL avec support mbedTLS
- **mbedTLS** : Librairie crypto hardware-accelerated (AES, SHA, RSA)
- **PubSubClient modifié** : Support de WiFiClientSecure
- **ArduinoJson** : Validation de schéma JSON

---

## 🧪 Plan de Tests Phase 2

### Test 1 : Validation TLS

- Wireshark : Vérifier que le trafic sur port 8883 est chiffré (Application Data)
- OpenSSL client : Vérifier le handshake TLS

```bash
openssl s_client -connect 192.168.1.100:8883 -CAfile ca_cert.pem
```

### Test 2 : Rejet de Certificat Invalide

- Modifier le certificat client → connexion MQTT doit échouer
- Modifier la CA côté broker → ESP32 doit rejeter la connexion

### Test 3 : Protection Anti-Replay

- Capturer une commande légitime avec Wireshark
- Rejouer la commande → doit être rejetée (timestamp expiré)

### Test 4 : Performance Benchmark

- Comparer temps de connexion, latence, mémoire avec Phase 1
- Documenter les résultats dans `docs/measurements/`

---

## 📚 Ressources Utiles

- [ESP32 TLS/SSL Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_tls.html)
- [Mosquitto TLS Configuration](https://mosquitto.org/man/mosquitto-tls-7.html)
- [mbedTLS Documentation](https://www.trustedfirmware.org/projects/mbed-tls/)
- [OASIS MQTT Security](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718028)

---

## 🚀 Prochaines Étapes (S4–S5)

1. **S4 Semaine 1** : Générer les certificats CA et clients, configurer Mosquitto avec TLS
2. **S4 Semaine 2** : Développer le firmware durci avec WiFiClientSecure
3. **S5 Semaine 1** : Implémenter validation JSON et anti-replay
4. **S5 Semaine 2** : Tests de performance et benchmarks comparatifs

---

## ⚠️ Note Importante

Les certificats et clés privées **ne doivent JAMAIS** être commités dans Git. Utiliser `.gitignore` pour exclure :

```
*.pem
*.key
*.crt
certs/
include/certs.h
```

Pour le partage avec l'équipe, utiliser un canal sécurisé (ex: keybase, encrypted zip).

---

**Ce firmware sera développé et documenté lors de la Phase 2 du projet (S4–S5).**

