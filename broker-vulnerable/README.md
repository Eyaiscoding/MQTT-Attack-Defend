# Configuration Mosquitto Broker - Phase 1 (Vulnérable)

**Projet:** SUP'COM MIT Project 4 - Attaque et Défense MQTT  
**Phase:** Phase 1 (S1–S3) - Configuration Intentionnellement Vulnérable  
**Responsable:** Malak (Broker Raspberry Pi)  
**Statut:** 📝 Placeholder - Configuration gérée par Malak

---

## 📋 Description

Ce dossier contient la configuration du broker **Eclipse Mosquitto** hébergé sur le Raspberry Pi pour la Phase 1 du projet. La configuration est intentionnellement vulnérable pour permettre les démonstrations d'attaque en S6.

---

## 🔧 Configuration Phase 1

### Fichier de Configuration (`mosquitto.conf`)

Exemple de configuration utilisée pour Phase 1 :

```conf
# ============================================================================
# Mosquitto Configuration - Phase 1 (Vulnerable)
# SUP'COM Project 4: MQTT Attack & Defense
# ============================================================================

# Listener non sécurisé (port 1883)
listener 1883
protocol mqtt

# Accès anonyme activé (pas d'authentification)
allow_anonymous true

# Pas de TLS (Phase 1)
# cafile /etc/mosquitto/certs/ca.crt
# certfile /etc/mosquitto/certs/server.crt
# keyfile /etc/mosquitto/certs/server.key

# Persistence des messages (QoS 1 et 2)
persistence true
persistence_location /var/lib/mosquitto/

# Logs détaillés pour analyse
log_dest file /var/log/mosquitto/mosquitto.log
log_type all
log_timestamp true

# Connexions
max_connections -1
```

---

## 🚀 Installation et Démarrage

### 1. Installer Mosquitto sur Raspberry Pi

```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients -y
```

### 2. Appliquer la Configuration

```bash
sudo nano /etc/mosquitto/mosquitto.conf
# (Copier la configuration ci-dessus)

# Redémarrer le service
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto  # Activer au démarrage
```

### 3. Vérifier le Statut

```bash
# Vérifier que le service est actif
sudo systemctl status mosquitto

# Vérifier l'écoute sur le port 1883
sudo netstat -tuln | grep 1883
# Output attendu: tcp  0  0.0.0.0:1883  0.0.0.0:*  LISTEN
```

---

## 🧪 Tests de Fonctionnement

### Test 1 : Publication Locale

Dans un terminal du Raspberry Pi :

```bash
# Terminal 1 : Subscriber
mosquitto_sub -h localhost -t "test/demo" -v

# Terminal 2 : Publisher
mosquitto_pub -h localhost -t "test/demo" -m "Hello MQTT!"
```

Résultat attendu dans Terminal 1 :

```
test/demo Hello MQTT!
```

### Test 2 : Connexion Externe (depuis ESP32 ou PC)

Sur une autre machine du réseau :

```bash
# Remplacer 192.168.1.100 par l'IP du Raspberry Pi
mosquitto_sub -h 192.168.1.100 -t "iot/projet4/#" -v
```

---

## 📊 Monitoring et Logs

### Visualiser les Logs en Temps Réel

```bash
sudo tail -f /var/log/mosquitto/mosquitto.log
```

### Filtrer les Messages du Projet

```bash
sudo grep "iot/projet4" /var/log/mosquitto/mosquitto.log
```

---

## ⚠️ Vulnérabilités Phase 1 (Intentionnelles)

| Vulnérabilité | Impact |
|---------------|--------|
| **Port 1883 non chiffré** | Trafic en clair (Wireshark) |
| **allow_anonymous true** | N'importe qui peut se connecter |
| **Pas de ACL (Access Control List)** | Tous les topics accessibles |
| **Pas de rate limiting** | Vulnérable au flooding DoS |
| **Logs non protégés** | Fuite d'informations |

---

## 🔐 Migration vers Phase 2 (Hardened)

Voir `broker-hardened/README.md` pour la configuration sécurisée avec :

- Port 8883 (TLS/SSL)
- Authentification par certificats X.509
- ACL restreignant l'accès aux topics
- Rate limiting et connexion limits

---

## 🐛 Dépannage

### Problème : Mosquitto ne démarre pas

```bash
# Vérifier les erreurs de configuration
sudo mosquitto -c /etc/mosquitto/mosquitto.conf -v

# Vérifier les permissions
sudo chown mosquitto:mosquitto /var/log/mosquitto/mosquitto.log
```

### Problème : Connexions externes refusées

```bash
# Vérifier le firewall
sudo ufw status
sudo ufw allow 1883/tcp

# Vérifier l'IP d'écoute dans mosquitto.conf
# S'assurer que listener n'est pas limité à 127.0.0.1
```

---

## 📚 Ressources

- [Mosquitto Documentation](https://mosquitto.org/documentation/)
- [Mosquitto Configuration Man Page](https://mosquitto.org/man/mosquitto-conf-5.html)
- [MQTT Explorer](http://mqtt-explorer.com/) - GUI pour monitoring

---

**Configuration maintenue par:** Malak  
**Contact:** (Ajouter coordonnées si nécessaire)

