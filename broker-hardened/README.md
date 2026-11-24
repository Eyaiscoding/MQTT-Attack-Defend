# Configuration Mosquitto Broker - Phase 2 (Durcie)

**Projet:** SUP'COM MIT Project 4 - Attaque et Défense MQTT  
**Phase:** Phase 2 (S4–S5) - Configuration Sécurisée avec TLS  
**Responsable:** Malak (Broker Raspberry Pi)  
**Statut:** 🚧 **EN DÉVELOPPEMENT** (prévu pour S4–S5)

---

## 📋 Aperçu

Ce dossier contiendra la configuration **sécurisée** du broker Mosquitto avec chiffrement TLS, authentification par certificats, et contrôles d'accès stricts.

---

## 🔐 Fonctionnalités Prévues (Phase 2)

### Sécurité Transport

✅ **Port 8883 (TLS/SSL)** au lieu de 1883  
✅ **TLS 1.2/1.3 uniquement** (désactivation de TLS 1.0/1.1)  
✅ **Certificats X.509** (CA + certificat serveur)  
✅ **Cipher suites sécurisées** (AES-GCM, ECDHE)

### Authentification & Autorisation

✅ **Authentification par certificats clients** (obligatoire)  
✅ **ACL (Access Control List)** par topic et client  
✅ **Désactivation de allow_anonymous**  
✅ **Révocation de certificats** (CRL - Certificate Revocation List)

### Défense en Profondeur

✅ **Rate limiting** (max messages par seconde)  
✅ **Connection limits** (max clients simultanés)  
✅ **Logs d'audit** chiffrés et horodatés  
✅ **Monitoring actif** (détection d'anomalies)

---

## 🗂️ Structure Prévue

```
broker-hardened/
├── mosquitto.conf              # Configuration Mosquitto sécurisée
├── acl.conf                    # Access Control List (permissions par topic)
├── certs/                      # Certificats (non commités dans Git)
│   ├── ca.crt                 # Certificat de la CA
│   ├── server.crt             # Certificat du serveur Mosquitto
│   ├── server.key             # Clé privée du serveur
│   └── crl.pem                # Certificate Revocation List
├── scripts/
│   ├── generate_certs.sh      # Script de génération de certificats
│   └── monitor_broker.sh      # Script de monitoring
└── README.md                   # Ce fichier
```

---

## 🔑 Génération des Certificats

### Certificats Prévus

```bash
# 1. Créer l'Autorité de Certification (CA)
openssl req -new -x509 -days 365 -extensions v3_ca \
  -keyout ca.key -out ca.crt \
  -subj "/C=TN/ST=Tunis/L=Ariana/O=SUPCOM/OU=IoT/CN=MQTT-CA"

# 2. Générer la clé privée du serveur
openssl genrsa -out server.key 2048

# 3. Générer la CSR (Certificate Signing Request) du serveur
openssl req -new -key server.key -out server.csr \
  -subj "/C=TN/ST=Tunis/O=SUPCOM/CN=mqtt.projet4.local"

# 4. Signer le certificat serveur avec la CA
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days 365

# 5. Générer les certificats clients (un par ESP32)
# Répéter pour chaque nœud : ESP32_Node_01, ESP32_Node_02, etc.
openssl genrsa -out esp32_node_01.key 2048
openssl req -new -key esp32_node_01.key -out esp32_node_01.csr \
  -subj "/C=TN/O=SUPCOM/CN=ESP32_Node_01"
openssl x509 -req -in esp32_node_01.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out esp32_node_01.crt -days 365

# 6. Vérifier les certificats
openssl verify -CAfile ca.crt server.crt
openssl verify -CAfile ca.crt esp32_node_01.crt
```

---

## 🔧 Configuration Prévue (`mosquitto.conf`)

```conf
# ============================================================================
# Mosquitto Configuration - Phase 2 (Hardened)
# SUP'COM Project 4: MQTT Attack & Defense
# ============================================================================

# Désactiver le port non sécurisé 1883
# listener 1883
# protocol mqtt

# Listener sécurisé TLS (port 8883)
listener 8883
protocol mqtt

# Certificats TLS
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key

# Exiger l'authentification par certificat client
require_certificate true
use_identity_as_username true

# Version TLS minimum (désactiver TLS 1.0/1.1)
tls_version tlsv1.2

# Cipher suites sécurisées uniquement
ciphers ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256

# Révocation de certificats (CRL)
# crlfile /etc/mosquitto/certs/crl.pem

# Désactiver l'accès anonyme
allow_anonymous false

# Access Control List (ACL)
acl_file /etc/mosquitto/acl.conf

# Persistence
persistence true
persistence_location /var/lib/mosquitto/

# Logs d'audit détaillés
log_dest file /var/log/mosquitto/mosquitto_secure.log
log_type all
log_timestamp true
log_timestamp_format %Y-%m-%dT%H:%M:%S

# Limites de connexion (DoS protection)
max_connections 50
max_inflight_messages 20
max_queued_messages 100

# Rate limiting (si supporté par la version)
# max_publish_rate 10
```

---

## 🔐 Access Control List (`acl.conf`)

```conf
# ============================================================================
# ACL Configuration - Phase 2
# Permissions par client et par topic
# ============================================================================

# Client ESP32_Node_01
user ESP32_Node_01
topic write iot/projet4/capteurs/#
topic write iot/projet4/status
topic read iot/projet4/actionneurs/relais
topic write iot/projet4/actionneurs/relais/state

# Client ESP32_Node_02 (si ajouté plus tard)
user ESP32_Node_02
topic write iot/projet4/capteurs/#
topic write iot/projet4/status
topic read iot/projet4/actionneurs/relais
topic write iot/projet4/actionneurs/relais/state

# Dashboard (lecture seule sur les capteurs)
user Dashboard_01
topic read iot/projet4/capteurs/#
topic read iot/projet4/status
topic write iot/projet4/actionneurs/relais
topic read iot/projet4/actionneurs/relais/state

# Admin (accès complet pour debug)
user Admin
topic readwrite iot/projet4/#

# Bloquer tous les autres accès
pattern read iot/projet4/#
deny
```

---

## 🧪 Tests de Sécurité Prévus

### Test 1 : Connexion TLS Validée

```bash
# Connexion avec certificat valide (doit réussir)
mosquitto_pub -h 192.168.1.100 -p 8883 \
  --cafile ca.crt \
  --cert esp32_node_01.crt \
  --key esp32_node_01.key \
  -t "iot/projet4/test" -m "Secure Message"
```

### Test 2 : Rejet de Certificat Invalide

```bash
# Connexion sans certificat (doit échouer)
mosquitto_pub -h 192.168.1.100 -p 8883 -t "test" -m "Fail"
# Erreur attendue: Connection Refused (certificat manquant)
```

### Test 3 : ACL - Rejet de Topic Non Autorisé

```bash
# ESP32_Node_01 tente d'écrire sur un topic non autorisé (doit échouer)
mosquitto_pub -h 192.168.1.100 -p 8883 \
  --cafile ca.crt \
  --cert esp32_node_01.crt \
  --key esp32_node_01.key \
  -t "iot/projet4/admin/config" -m "Unauthorized"
# Erreur attendue: Publish denied (ACL)
```

### Test 4 : Wireshark - Vérification Chiffrement

```bash
# Capturer le trafic sur port 8883
sudo tcpdump -i wlan0 -w mqtt_tls.pcap port 8883

# Analyser avec Wireshark :
# - Protocole : TLSv1.2 ou TLSv1.3
# - Application Data : chiffré (opaque)
# - Pas de payloads MQTT visibles en clair
```

---

## 📊 Comparaison Phase 1 vs Phase 2

| Aspect | Phase 1 | Phase 2 |
|--------|---------|---------|
| **Port** | 1883 | 8883 |
| **Chiffrement** | ❌ Aucun | ✅ TLS 1.2/1.3 |
| **Authentification** | ❌ Anonyme | ✅ Certificats X.509 |
| **Autorisation** | ❌ Aucune | ✅ ACL par topic |
| **Wireshark** | Payloads visibles | Application Data chiffrée |
| **Attaques MitM** | ✅ Possible | ❌ Bloquée |
| **DoS Protection** | ❌ Aucune | ✅ Rate limiting |

---

## 🚀 Plan de Déploiement (S4)

1. **Semaine 1 :** Générer tous les certificats (CA, serveur, clients)
2. **Semaine 2 :** Configurer Mosquitto avec TLS et tester la connexion
3. **Semaine 3 :** Implémenter l'ACL et tester les restrictions
4. **Semaine 4 :** Tests de pénétration et validation de sécurité

---

## 🐛 Dépannage Prévu

### Problème : Handshake TLS échoue

```bash
# Vérifier les certificats avec OpenSSL
openssl s_client -connect 192.168.1.100:8883 -CAfile ca.crt
# Doit afficher "Verify return code: 0 (ok)"
```

### Problème : ACL rejette une connexion légitime

```bash
# Vérifier les logs Mosquitto
sudo tail -f /var/log/mosquitto/mosquitto_secure.log | grep "DENIED"
```

---

## 📚 Ressources

- [Mosquitto TLS Configuration](https://mosquitto.org/man/mosquitto-tls-7.html)
- [OpenSSL Certificate Guide](https://www.openssl.org/docs/manmaster/man1/openssl-req.html)
- [MQTT Security Best Practices](https://www.hivemq.com/blog/mqtt-security-fundamentals/)

---

**Configuration à développer en S4–S5 par Malak**

