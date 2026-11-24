#!/bin/bash
################################################################################
# PKI Certificate Generation Script for MQTT IoT Security Project
# 
# Project: SUP'COM MIT Project 4 - MQTT Attack & Defense
# Author: Eya
# Date: November 2026
# Phase: Phase 2 (S4-S6)
#
# Description:
#   Generates a complete self-signed PKI infrastructure for MQTT over TLS:
#   - Root Certificate Authority (CA)
#   - Mosquitto Broker Server Certificate with SANs
#   - ESP32 Client Certificate (CN: esp32-node-01)
#   - Dashboard Client Certificate (CN: dashboard-01)
#
# Usage:
#   chmod +x generate_certs.sh
#   ./generate_certs.sh
#
# Output:
#   All certificates and keys are created in pki/certs/
################################################################################

set -e  # Exit on error

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}MQTT PKI Certificate Generation${NC}"
echo -e "${GREEN}SUP'COM Project 4 - Phase 2${NC}"
echo -e "${GREEN}========================================${NC}\n"

# Configuration
CERT_DIR="./certs"
DAYS_CA=3650          # 10 years for CA
DAYS_CERTS=365        # 1 year for server/client certs
COUNTRY="TN"
STATE="Tunis"
CITY="Ariana"
ORG="SUPCOM"
OU="IoT-Security"

# Create output directory
mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

echo -e "${YELLOW}[INFO]${NC} All certificates will be created in: $(pwd)\n"

################################################################################
# Step 1: Generate Root Certificate Authority (CA)
################################################################################

echo -e "${YELLOW}[STEP 1/4]${NC} Generating Root Certificate Authority (CA)..."

# Generate CA private key (4096-bit RSA for strong security)
openssl genrsa -out ca.key 4096 2>/dev/null

# Generate self-signed CA certificate
openssl req -new -x509 -days $DAYS_CA -key ca.key -out ca.crt \
  -subj "/C=$COUNTRY/ST=$STATE/L=$CITY/O=$ORG/OU=$OU/CN=MQTT-CA-Root" \
  -extensions v3_ca 2>/dev/null

echo -e "${GREEN}✓${NC} CA certificate generated: ca.crt (valid for $DAYS_CA days)"
echo -e "${GREEN}✓${NC} CA private key: ca.key\n"

################################################################################
# Step 2: Generate Mosquitto Broker Server Certificate with SANs
################################################################################

echo -e "${YELLOW}[STEP 2/4]${NC} Generating Mosquitto Broker Server Certificate..."

# Generate server private key
openssl genrsa -out server.key 2048 2>/dev/null

# Create OpenSSL configuration for SANs (Subject Alternative Names)
cat > server_san.cnf <<EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
C = $COUNTRY
ST = $STATE
L = $CITY
O = $ORG
OU = $OU
CN = mqtt.projet4.local

[v3_req]
keyUsage = keyEncipherment, dataEncipherment, digitalSignature
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = raspberrypi.local
DNS.2 = mqtt.projet4.local
DNS.3 = localhost
IP.1 = 192.168.1.100
IP.2 = 127.0.0.1
EOF

# Generate Certificate Signing Request (CSR) with SANs
openssl req -new -key server.key -out server.csr -config server_san.cnf 2>/dev/null

# Sign server certificate with CA (include SANs)
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days $DAYS_CERTS \
  -extensions v3_req -extfile server_san.cnf 2>/dev/null

# Clean up temporary files
rm server.csr server_san.cnf

echo -e "${GREEN}✓${NC} Server certificate generated: server.crt (valid for $DAYS_CERTS days)"
echo -e "${GREEN}✓${NC} Server private key: server.key"
echo -e "  SANs: DNS:raspberrypi.local, DNS:mqtt.projet4.local, DNS:localhost,"
echo -e "        IP:192.168.1.100, IP:127.0.0.1\n"

################################################################################
# Step 3: Generate ESP32 Client Certificate
################################################################################

echo -e "${YELLOW}[STEP 3/4]${NC} Generating ESP32 Client Certificate..."

CLIENT_CN="esp32-node-01"

# Generate client private key
openssl genrsa -out esp32-client.key 2048 2>/dev/null

# Generate client CSR
openssl req -new -key esp32-client.key -out esp32-client.csr \
  -subj "/C=$COUNTRY/ST=$STATE/O=$ORG/OU=$OU/CN=$CLIENT_CN" 2>/dev/null

# Sign client certificate with CA
openssl x509 -req -in esp32-client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out esp32-client.crt -days $DAYS_CERTS 2>/dev/null

# Clean up
rm esp32-client.csr

echo -e "${GREEN}✓${NC} ESP32 client certificate generated: esp32-client.crt"
echo -e "${GREEN}✓${NC} ESP32 client private key: esp32-client.key"
echo -e "  Common Name: $CLIENT_CN (used for Mosquitto ACL mapping)\n"

################################################################################
# Step 4: Generate Dashboard Client Certificate
################################################################################

echo -e "${YELLOW}[STEP 4/4]${NC} Generating Dashboard Client Certificate..."

DASHBOARD_CN="dashboard-01"

# Generate dashboard client private key
openssl genrsa -out dashboard-client.key 2048 2>/dev/null

# Generate dashboard client CSR
openssl req -new -key dashboard-client.key -out dashboard-client.csr \
  -subj "/C=$COUNTRY/ST=$STATE/O=$ORG/OU=$OU/CN=$DASHBOARD_CN" 2>/dev/null

# Sign dashboard client certificate with CA
openssl x509 -req -in dashboard-client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out dashboard-client.crt -days $DAYS_CERTS 2>/dev/null

# Clean up
rm dashboard-client.csr ca.srl

echo -e "${GREEN}✓${NC} Dashboard client certificate generated: dashboard-client.crt"
echo -e "${GREEN}✓${NC} Dashboard client private key: dashboard-client.key"
echo -e "  Common Name: $DASHBOARD_CN\n"

################################################################################
# Verification & Summary
################################################################################

echo -e "${YELLOW}[VERIFICATION]${NC} Verifying certificate chain...\n"

# Verify server certificate against CA
if openssl verify -CAfile ca.crt server.crt >/dev/null 2>&1; then
    echo -e "${GREEN}✓${NC} Server certificate: VALID"
else
    echo -e "${RED}✗${NC} Server certificate: INVALID"
fi

# Verify ESP32 client certificate against CA
if openssl verify -CAfile ca.crt esp32-client.crt >/dev/null 2>&1; then
    echo -e "${GREEN}✓${NC} ESP32 client certificate: VALID"
else
    echo -e "${RED}✗${NC} ESP32 client certificate: INVALID"
fi

# Verify dashboard client certificate against CA
if openssl verify -CAfile ca.crt dashboard-client.crt >/dev/null 2>&1; then
    echo -e "${GREEN}✓${NC} Dashboard client certificate: VALID\n"
else
    echo -e "${RED}✗${NC} Dashboard client certificate: INVALID\n"
fi

################################################################################
# Display certificate details
################################################################################

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Certificate Generation Complete!${NC}"
echo -e "${GREEN}========================================${NC}\n"

echo "Generated files in $(pwd):"
echo ""
echo "  Certificate Authority:"
echo "    - ca.crt              (Root CA certificate - distribute to all devices)"
echo "    - ca.key              (⚠️  CA private key - KEEP SECURE, DO NOT DEPLOY)"
echo ""
echo "  Mosquitto Broker (Raspberry Pi):"
echo "    - server.crt          (Server certificate)"
echo "    - server.key          (⚠️  Server private key - KEEP SECURE)"
echo ""
echo "  ESP32 Client (CN: esp32-node-01):"
echo "    - esp32-client.crt    (Client certificate)"
echo "    - esp32-client.key    (⚠️  Client private key - EMBED IN FIRMWARE)"
echo ""
echo "  Dashboard Client (CN: dashboard-01):"
echo "    - dashboard-client.crt"
echo "    - dashboard-client.key"
echo ""
echo -e "${YELLOW}[NEXT STEPS]${NC}"
echo ""
echo "1. Copy broker certificates to Raspberry Pi:"
echo "   scp ca.crt server.crt server.key pi@raspberrypi.local:/etc/mosquitto/certs/"
echo ""
echo "2. Update Mosquitto configuration (/etc/mosquitto/mosquitto.conf):"
echo "   listener 8883"
echo "   cafile /etc/mosquitto/certs/ca.crt"
echo "   certfile /etc/mosquitto/certs/server.crt"
echo "   keyfile /etc/mosquitto/certs/server.key"
echo "   require_certificate true"
echo "   use_identity_as_username true"
echo ""
echo "3. Embed ESP32 certificates in firmware:"
echo "   - Convert ca.crt, esp32-client.crt, esp32-client.key to C header format"
echo "   - See: ../firmware-hardened/src/certs.h.example"
echo ""
echo "4. Set proper permissions on Raspberry Pi:"
echo "   sudo chown mosquitto:mosquitto /etc/mosquitto/certs/*"
echo "   sudo chmod 640 /etc/mosquitto/certs/*.key"
echo ""
echo -e "${GREEN}✓${NC} PKI generation complete!\n"
