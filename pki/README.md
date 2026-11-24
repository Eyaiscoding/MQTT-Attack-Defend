# PKI Infrastructure for MQTT over TLS

**Projet:** SUP'COM MIT Project 4 - MQTT Attack & Defense  
**Phase:** Phase 2 (S4–S6) - Configuration Sécurisée  
**Auteur:** Eya  
**Date:** Novembre 2026

---

## 📋 Vue d'Ensemble

Ce dossier contient les scripts de génération d'une **infrastructure à clés publiques (PKI)** complète pour sécuriser les communications MQTT via TLS/SSL avec **authentification mutuelle (mTLS)**.

### Architecture PKI

```
┌─────────────────────────────────────────────────────────────┐
│                   Root Certificate Authority                 │
│                        (ca.crt, ca.key)                      │
│                      Self-signed, 10 years                   │
└──────────────────────┬──────────────────────────────────────┘
                       │
           ┌───────────┴───────────┐
           │                       │
           ▼                       ▼
┌──────────────────────┐  ┌──────────────────────┐
│  Server Certificate  │  │  Client Certificates │
│   (server.crt/.key)  │  │                      │
│                      │  │  - esp32-client      │
│  CN: mqtt.projet4... │  │    (CN: esp32-node-01)│
│  SANs:               │  │  - dashboard-client  │
│   - raspberrypi...   │  │    (CN: dashboard-01)│
│   - 192.168.1.100    │  │                      │
└──────────────────────┘  └──────────────────────┘
```

---

## 🚀 Génération des Certificats

### Prérequis

- **OpenSSL** installé et accessible dans le PATH
  - **Linux/macOS:** Préinstallé sur la plupart des distributions
  - **Windows:** [OpenSSL Win64](https://slproweb.com/products/Win32OpenSSL.html)

Vérifier l'installation :
```bash
openssl version
# Output attendu: OpenSSL 3.x.x ou 1.1.1+
```

### Exécution

#### Linux / macOS / Git Bash

```bash
cd pki
chmod +x generate_certs.sh
./generate_certs.sh
```

#### Windows PowerShell

```powershell
cd pki
.\generate_certs.ps1
```

### Sortie

Tous les certificats et clés sont créés dans `pki/certs/` :

```
pki/certs/
├── ca.crt                      # Certificat de l'autorité racine (distribuer à tous)
├── ca.key                      # ⚠️  Clé privée CA (NE PAS DÉPLOYER)
├── server.crt                  # Certificat du broker Mosquitto
├── server.key                  # ⚠️  Clé privée du serveur
├── esp32-client.crt            # Certificat client ESP32
├── esp32-client.key            # ⚠️  Clé privée client ESP32
├── dashboard-client.crt        # Certificat client dashboard
└── dashboard-client.key        # ⚠️  Clé privée dashboard
```

---

## 🔐 Certificats Générés

### 1. Certificate Authority (CA)

| Paramètre | Valeur |
|-----------|--------|
| **Fichiers** | `ca.crt`, `ca.key` |
| **Type** | Self-signed Root CA |
| **Algorithme** | RSA 4096-bit |
| **Validité** | 10 ans (3650 jours) |
| **Common Name** | `MQTT-CA-Root` |
| **Usage** | Signer les certificats serveur et clients |

**⚠️ Sécurité:** La clé privée `ca.key` doit être **stockée de manière sécurisée** et **jamais déployée** sur les appareils IoT ou le serveur.

### 2. Server Certificate (Mosquitto Broker)

| Paramètre | Valeur |
|-----------|--------|
| **Fichiers** | `server.crt`, `server.key` |
| **Algorithme** | RSA 2048-bit |
| **Validité** | 1 an (365 jours) |
| **Common Name** | `mqtt.projet4.local` |
| **Subject Alternative Names (SANs)** | `DNS:raspberrypi.local`<br>`DNS:mqtt.projet4.local`<br>`DNS:localhost`<br>`IP:192.168.1.100`<br>`IP:127.0.0.1` |
| **Extended Key Usage** | Server Authentication (`serverAuth`) |

**SANs (Subject Alternative Names)** permettent au certificat d'être valide pour plusieurs noms de domaine et adresses IP, évitant les erreurs de vérification TLS.

### 3. ESP32 Client Certificate

| Paramètre | Valeur |
|-----------|--------|
| **Fichiers** | `esp32-client.crt`, `esp32-client.key` |
| **Algorithme** | RSA 2048-bit |
| **Validité** | 1 an (365 jours) |
| **Common Name** | `esp32-node-01` |
| **Usage** | Authentification mutuelle (mTLS) |

**Note:** Le **Common Name (CN)** est utilisé par Mosquitto pour mapper l'identité du client dans les **ACLs** (Access Control Lists). Avec `use_identity_as_username true`, le CN devient le username MQTT.

### 4. Dashboard Client Certificate

| Paramètre | Valeur |
|-----------|--------|
| **Fichiers** | `dashboard-client.crt`, `dashboard-client.key` |
| **Algorithme** | RSA 2048-bit |
| **Validité** | 1 an (365 jours) |
| **Common Name** | `dashboard-01` |
| **Usage** | Connexion MQTT depuis MQTT Explorer, Node-RED, etc. |

---

## 📦 Déploiement

### Étape 1 : Déployer les Certificats sur le Raspberry Pi (Broker)

```bash
# Créer le répertoire de certificats
ssh pi@raspberrypi.local
sudo mkdir -p /etc/mosquitto/certs
exit

# Copier les fichiers (depuis votre machine)
cd pki/certs
scp ca.crt server.crt server.key pi@raspberrypi.local:/tmp/

# Sur le Raspberry Pi, déplacer et configurer les permissions
ssh pi@raspberrypi.local
sudo mv /tmp/{ca.crt,server.crt,server.key} /etc/mosquitto/certs/
sudo chown mosquitto:mosquitto /etc/mosquitto/certs/*
sudo chmod 644 /etc/mosquitto/certs/ca.crt
sudo chmod 644 /etc/mosquitto/certs/server.crt
sudo chmod 600 /etc/mosquitto/certs/server.key  # Lecture seule par mosquitto
```

### Étape 2 : Configurer Mosquitto pour TLS

Éditer `/etc/mosquitto/mosquitto.conf` sur le Raspberry Pi :

```conf
# Désactiver le port non sécurisé (optionnel, pour forcer TLS uniquement)
# listener 1883

# Activer le port TLS
listener 8883
protocol mqtt

# Certificats TLS
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key

# Exiger un certificat client (mTLS)
require_certificate true
use_identity_as_username true

# Version TLS minimum (désactiver TLS 1.0/1.1 obsolètes)
tls_version tlsv1.2

# Cipher suites sécurisées uniquement
# (optionnel, Mosquitto utilise des valeurs sécurisées par défaut)
# ciphers ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256
```

Redémarrer Mosquitto :

```bash
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
```

Vérifier l'écoute sur le port 8883 :

```bash
sudo netstat -tuln | grep 8883
# Output attendu: tcp  0  0.0.0.0:8883  0.0.0.0:*  LISTEN
```

### Étape 3 : Embarquer les Certificats dans le Firmware ESP32

Les certificats doivent être **convertis en format C header** pour être compilés dans le firmware ESP32.

#### Méthode Automatique (Script Python)

Créer un script `pki/cert_to_header.py` :

```python
#!/usr/bin/env python3
import sys

if len(sys.argv) < 2:
    print("Usage: python3 cert_to_header.py <cert_file.crt>")
    sys.exit(1)

cert_file = sys.argv[1]
var_name = cert_file.replace('.', '_').replace('-', '_').upper()

with open(cert_file, 'r') as f:
    cert_data = f.read()

print(f'const char {var_name}[] PROGMEM = R"EOF(')
print(cert_data, end='')
print(')EOF";')
```

Utilisation :

```bash
cd pki/certs
python3 ../cert_to_header.py ca.crt > ../../firmware-hardened/src/ca_cert.h
python3 ../cert_to_header.py esp32-client.crt >> ../../firmware-hardened/src/client_cert.h
python3 ../cert_to_header.py esp32-client.key >> ../../firmware-hardened/src/client_key.h
```

#### Méthode Manuelle

Copier le contenu de `ca.crt`, `esp32-client.crt`, et `esp32-client.key` dans `firmware-hardened/src/certs.h` :

```cpp
// firmware-hardened/src/certs.h
#ifndef CERTS_H
#define CERTS_H

// Root CA Certificate
const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
[Copier le contenu de ca.crt ici]
-----END CERTIFICATE-----
)EOF";

// ESP32 Client Certificate
const char CLIENT_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
[Copier le contenu de esp32-client.crt ici]
-----END CERTIFICATE-----
)EOF";

// ESP32 Client Private Key
const char CLIENT_KEY[] PROGMEM = R"EOF(
-----BEGIN PRIVATE KEY-----
[Copier le contenu de esp32-client.key ici]
-----END PRIVATE KEY-----
)EOF";

#endif // CERTS_H
```

**⚠️ Important:** 
- **Ne JAMAIS commiter `certs.h` avec les vraies clés privées dans Git!**
- Utiliser `.gitignore` pour exclure `firmware-hardened/src/certs.h`
- Fournir uniquement `certs.h.example` avec des placeholders

---

## 🧪 Tests de Connexion TLS

### Test 1 : Connexion avec OpenSSL (depuis PC)

```bash
openssl s_client -connect 192.168.1.100:8883 \
  -CAfile pki/certs/ca.crt \
  -cert pki/certs/esp32-client.crt \
  -key pki/certs/esp32-client.key
```

**Sortie attendue :**
```
Verify return code: 0 (ok)
```

### Test 2 : Connexion MQTT avec Certificat Client

```bash
mosquitto_pub -h 192.168.1.100 -p 8883 \
  --cafile pki/certs/ca.crt \
  --cert pki/certs/esp32-client.crt \
  --key pki/certs/esp32-client.key \
  -t "iot/projet4/test" -m "Hello TLS!"
```

**Sans certificat client (doit échouer) :**

```bash
mosquitto_pub -h 192.168.1.100 -p 8883 \
  --cafile pki/certs/ca.crt \
  -t "iot/projet4/test" -m "Fail"
# Error: Connection refused (certificat client requis)
```

---

## 🔄 Renouvellement des Certificats

Les certificats serveur et clients expirent après **1 an**. Pour les renouveler :

### Option 1 : Regénérer Tous les Certificats

```bash
# Sauvegarder l'ancienne CA si nécessaire
cp pki/certs/ca.crt pki/certs/ca.crt.old

# Regénérer
./pki/generate_certs.sh

# Redéployer sur tous les appareils
```

### Option 2 : Renouveler Uniquement les Certificats Clients/Serveur

Garder la CA existante et regénérer seulement les certificats expirés :

```bash
cd pki/certs

# Renouveler le certificat serveur
openssl genrsa -out server_new.key 2048
openssl req -new -key server_new.key -out server_new.csr \
  -subj "/C=TN/ST=Tunis/L=Ariana/O=SUPCOM/OU=IoT-Security/CN=mqtt.projet4.local"
openssl x509 -req -in server_new.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server_new.crt -days 365

# Remplacer les anciens certificats
mv server.crt server.crt.old
mv server.key server.key.old
mv server_new.crt server.crt
mv server_new.key server.key
rm server_new.csr
```

---

## 📚 Références

- [Mosquitto TLS Configuration](https://mosquitto.org/man/mosquitto-tls-7.html)
- [OpenSSL Certificate Authority](https://jamielinux.com/docs/openssl-certificate-authority/)
- [ESP32 WiFiClientSecure Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure)
- [X.509 Certificate Standards (RFC 5280)](https://tools.ietf.org/html/rfc5280)

---

## ⚠️ Sécurité et Bonnes Pratiques

### ✅ À Faire

1. **Protéger les clés privées** : Permissions restrictives (`chmod 600`), stockage sécurisé
2. **Ne jamais commiter les clés** : Utiliser `.gitignore` pour exclure `*.key`, `*.pem`
3. **Renouveler régulièrement** : Certificats avec validité courte (1 an max)
4. **Vérifier la chaîne CA** : `openssl verify -CAfile ca.crt client.crt`
5. **Utiliser des SANs** : Pour éviter les erreurs de hostname mismatch
6. **Désactiver TLS 1.0/1.1** : Utiliser uniquement TLS 1.2+ dans Mosquitto

### ❌ À Éviter

1. **Partager `ca.key`** : Ne jamais distribuer la clé privée de la CA
2. **Utiliser la même clé** : Générer des clés uniques par appareil/client
3. **Certificats auto-signés en production** : Utiliser Let's Encrypt ou une CA publique pour production
4. **Ignorer les avertissements TLS** : Ne jamais désactiver la vérification du certificat
5. **Clés faibles** : Utiliser RSA 2048-bit minimum (4096-bit pour CA)

---

**Certificats générés et prêts pour le déploiement Phase 2!** 🔐

