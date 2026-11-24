################################################################################
# PKI Certificate Generation Script for MQTT IoT Security Project (PowerShell)
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
# Requirements:
#   - OpenSSL must be installed and in PATH
#   - Tested with: OpenSSL 3.x on Windows 10/11
#
# Usage:
#   .\generate_certs.ps1
#
# Output:
#   All certificates and keys are created in pki\certs\
################################################################################

# Ensure script stops on errors
$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Green
Write-Host "MQTT PKI Certificate Generation" -ForegroundColor Green
Write-Host "SUP'COM Project 4 - Phase 2" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Green

# Check if OpenSSL is available
try {
    $null = & openssl version 2>&1
} catch {
    Write-Host "[ERROR] OpenSSL not found in PATH!" -ForegroundColor Red
    Write-Host "Please install OpenSSL from: https://slproweb.com/products/Win32OpenSSL.html" -ForegroundColor Yellow
    exit 1
}

# Configuration
$CERT_DIR = ".\certs"
$DAYS_CA = 3650          # 10 years for CA
$DAYS_CERTS = 365        # 1 year for server/client certs
$COUNTRY = "TN"
$STATE = "Tunis"
$CITY = "Ariana"
$ORG = "SUPCOM"
$OU = "IoT-Security"

# Create output directory
New-Item -ItemType Directory -Force -Path $CERT_DIR | Out-Null
Push-Location $CERT_DIR

Write-Host "[INFO] All certificates will be created in: $(Get-Location)`n" -ForegroundColor Yellow

################################################################################
# Step 1: Generate Root Certificate Authority (CA)
################################################################################

Write-Host "[STEP 1/4] Generating Root Certificate Authority (CA)..." -ForegroundColor Yellow

# Generate CA private key (4096-bit RSA for strong security)
& openssl genrsa -out ca.key 4096 2>$null

# Generate self-signed CA certificate
& openssl req -new -x509 -days $DAYS_CA -key ca.key -out ca.crt `
  -subj "/C=$COUNTRY/ST=$STATE/L=$CITY/O=$ORG/OU=$OU/CN=MQTT-CA-Root" `
  -extensions v3_ca 2>$null

Write-Host "✓ CA certificate generated: ca.crt (valid for $DAYS_CA days)" -ForegroundColor Green
Write-Host "✓ CA private key: ca.key`n" -ForegroundColor Green

################################################################################
# Step 2: Generate Mosquitto Broker Server Certificate with SANs
################################################################################

Write-Host "[STEP 2/4] Generating Mosquitto Broker Server Certificate..." -ForegroundColor Yellow

# Generate server private key
& openssl genrsa -out server.key 2048 2>$null

# Create OpenSSL configuration for SANs (Subject Alternative Names)
$serverSanConfig = @"
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
"@

$serverSanConfig | Out-File -FilePath server_san.cnf -Encoding ASCII

# Generate Certificate Signing Request (CSR) with SANs
& openssl req -new -key server.key -out server.csr -config server_san.cnf 2>$null

# Sign server certificate with CA (include SANs)
& openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key `
  -CAcreateserial -out server.crt -days $DAYS_CERTS `
  -extensions v3_req -extfile server_san.cnf 2>$null

# Clean up temporary files
Remove-Item server.csr, server_san.cnf -Force

Write-Host "✓ Server certificate generated: server.crt (valid for $DAYS_CERTS days)" -ForegroundColor Green
Write-Host "✓ Server private key: server.key" -ForegroundColor Green
Write-Host "  SANs: DNS:raspberrypi.local, DNS:mqtt.projet4.local, DNS:localhost,"
Write-Host "        IP:192.168.1.100, IP:127.0.0.1`n"

################################################################################
# Step 3: Generate ESP32 Client Certificate
################################################################################

Write-Host "[STEP 3/4] Generating ESP32 Client Certificate..." -ForegroundColor Yellow

$CLIENT_CN = "esp32-node-01"

# Generate client private key
& openssl genrsa -out esp32-client.key 2048 2>$null

# Generate client CSR
& openssl req -new -key esp32-client.key -out esp32-client.csr `
  -subj "/C=$COUNTRY/ST=$STATE/O=$ORG/OU=$OU/CN=$CLIENT_CN" 2>$null

# Sign client certificate with CA
& openssl x509 -req -in esp32-client.csr -CA ca.crt -CAkey ca.key `
  -CAcreateserial -out esp32-client.crt -days $DAYS_CERTS 2>$null

# Clean up
Remove-Item esp32-client.csr -Force

Write-Host "✓ ESP32 client certificate generated: esp32-client.crt" -ForegroundColor Green
Write-Host "✓ ESP32 client private key: esp32-client.key" -ForegroundColor Green
Write-Host "  Common Name: $CLIENT_CN (used for Mosquitto ACL mapping)`n"

################################################################################
# Step 4: Generate Dashboard Client Certificate
################################################################################

Write-Host "[STEP 4/4] Generating Dashboard Client Certificate..." -ForegroundColor Yellow

$DASHBOARD_CN = "dashboard-01"

# Generate dashboard client private key
& openssl genrsa -out dashboard-client.key 2048 2>$null

# Generate dashboard client CSR
& openssl req -new -key dashboard-client.key -out dashboard-client.csr `
  -subj "/C=$COUNTRY/ST=$STATE/O=$ORG/OU=$OU/CN=$DASHBOARD_CN" 2>$null

# Sign dashboard client certificate with CA
& openssl x509 -req -in dashboard-client.csr -CA ca.crt -CAkey ca.key `
  -CAcreateserial -out dashboard-client.crt -days $DAYS_CERTS 2>$null

# Clean up
Remove-Item dashboard-client.csr, ca.srl -Force -ErrorAction SilentlyContinue

Write-Host "✓ Dashboard client certificate generated: dashboard-client.crt" -ForegroundColor Green
Write-Host "✓ Dashboard client private key: dashboard-client.key" -ForegroundColor Green
Write-Host "  Common Name: $DASHBOARD_CN`n"

################################################################################
# Verification & Summary
################################################################################

Write-Host "[VERIFICATION] Verifying certificate chain...`n" -ForegroundColor Yellow

# Verify server certificate against CA
$serverVerify = & openssl verify -CAfile ca.crt server.crt 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Server certificate: VALID" -ForegroundColor Green
} else {
    Write-Host "✗ Server certificate: INVALID" -ForegroundColor Red
}

# Verify ESP32 client certificate against CA
$esp32Verify = & openssl verify -CAfile ca.crt esp32-client.crt 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ ESP32 client certificate: VALID" -ForegroundColor Green
} else {
    Write-Host "✗ ESP32 client certificate: INVALID" -ForegroundColor Red
}

# Verify dashboard client certificate against CA
$dashboardVerify = & openssl verify -CAfile ca.crt dashboard-client.crt 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Dashboard client certificate: VALID`n" -ForegroundColor Green
} else {
    Write-Host "✗ Dashboard client certificate: INVALID`n" -ForegroundColor Red
}

################################################################################
# Display certificate details
################################################################################

Write-Host "========================================" -ForegroundColor Green
Write-Host "Certificate Generation Complete!" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Green

Write-Host "Generated files in $(Get-Location):"
Write-Host ""
Write-Host "  Certificate Authority:"
Write-Host "    - ca.crt              (Root CA certificate - distribute to all devices)"
Write-Host "    - ca.key              (⚠️  CA private key - KEEP SECURE, DO NOT DEPLOY)"
Write-Host ""
Write-Host "  Mosquitto Broker (Raspberry Pi):"
Write-Host "    - server.crt          (Server certificate)"
Write-Host "    - server.key          (⚠️  Server private key - KEEP SECURE)"
Write-Host ""
Write-Host "  ESP32 Client (CN: esp32-node-01):"
Write-Host "    - esp32-client.crt    (Client certificate)"
Write-Host "    - esp32-client.key    (⚠️  Client private key - EMBED IN FIRMWARE)"
Write-Host ""
Write-Host "  Dashboard Client (CN: dashboard-01):"
Write-Host "    - dashboard-client.crt"
Write-Host "    - dashboard-client.key"
Write-Host ""
Write-Host "[NEXT STEPS]" -ForegroundColor Yellow
Write-Host ""
Write-Host "1. Copy broker certificates to Raspberry Pi:"
Write-Host "   scp ca.crt server.crt server.key pi@raspberrypi.local:/etc/mosquitto/certs/"
Write-Host ""
Write-Host "2. Update Mosquitto configuration (/etc/mosquitto/mosquitto.conf):"
Write-Host "   listener 8883"
Write-Host "   cafile /etc/mosquitto/certs/ca.crt"
Write-Host "   certfile /etc/mosquitto/certs/server.crt"
Write-Host "   keyfile /etc/mosquitto/certs/server.key"
Write-Host "   require_certificate true"
Write-Host "   use_identity_as_username true"
Write-Host ""
Write-Host "3. Embed ESP32 certificates in firmware:"
Write-Host "   - Use the included conversion script or manually convert to C header format"
Write-Host "   - See: ..\firmware-hardened\src\certs.h.example"
Write-Host ""
Write-Host "4. Set proper permissions on Raspberry Pi:"
Write-Host "   sudo chown mosquitto:mosquitto /etc/mosquitto/certs/*"
Write-Host "   sudo chmod 640 /etc/mosquitto/certs/*.key"
Write-Host ""
Write-Host "✓ PKI generation complete!`n" -ForegroundColor Green

Pop-Location
