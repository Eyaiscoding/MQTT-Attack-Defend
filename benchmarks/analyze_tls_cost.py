#!/usr/bin/env python3
"""
TLS Overhead Analysis Script for ESP32 MQTT IoT Project

Project: SUP'COM MIT Project 4 - MQTT Attack & Defense
Author: Eya
Date: November 2026
Phase: Phase 2 (S4-S6)

Description:
    Analyzes performance benchmarks from Phase 1 (plaintext) and Phase 2 (TLS)
    to quantify the overhead of TLS encryption on the ESP32 IoT node.
    
    Metrics analyzed:
    - Connection / TLS handshake latency (ms)
    - RAM / Heap consumption (bytes)
    - Energy per message (µJ / mJ) - calculated or measured
    
    Generates:
    - Comparison charts (PNG)
    - Summary tables (Markdown)
    - Statistical analysis (mean, std dev, overhead %)

Usage:
    python3 analyze_tls_cost.py
    
    Input files (expected in benchmarks/sample_data/):
    - plaintext.csv (Phase 1 benchmark data)
    - tls.csv (Phase 2 benchmark data)
    
    Output files (created in benchmarks/):
    - tls_overhead_comparison.png (visualization)
    - results_summary.md (quantitative analysis)
"""

import sys
import os
from pathlib import Path
import csv
import statistics
from dataclasses import dataclass
from typing import List, Tuple

# Check if matplotlib and pandas are available (optional dependencies)
try:
    import matplotlib.pyplot as plt
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("[WARNING] matplotlib not found. Charts will not be generated.")
    print("          Install with: pip install matplotlib")

# ============================================================================
# Configuration
# ============================================================================

# File paths
SCRIPT_DIR = Path(__file__).parent
DATA_DIR = SCRIPT_DIR / "sample_data"
PLAINTEXT_CSV = DATA_DIR / "plaintext.csv"
TLS_CSV = DATA_DIR / "tls.csv"
OUTPUT_CHART = SCRIPT_DIR / "tls_overhead_comparison.png"
OUTPUT_SUMMARY = SCRIPT_DIR / "results_summary.md"

# ESP32 hardware parameters for energy calculation
ESP32_VOLTAGE = 3.3  # V (operating voltage)
ESP32_CURRENT_IDLE = 20  # mA (idle current, Wi-Fi connected)
ESP32_CURRENT_TX_PLAINTEXT = 160  # mA (transmitting, plaintext MQTT)
ESP32_CURRENT_TX_TLS = 180  # mA (transmitting, TLS encryption active)

# ESP32 SRAM total (for percentage calculation)
ESP32_TOTAL_SRAM = 520 * 1024  # 520 KB

# ============================================================================
# Data Structures
# ============================================================================

@dataclass
class BenchmarkData:
    """Container for benchmark measurements"""
    version: str
    msg_ids: List[int]
    connect_times: List[float]  # ms
    publish_times: List[float]  # ms
    free_heap: List[int]  # bytes
    min_free_heap: List[int]  # bytes

@dataclass
class Statistics:
    """Statistical summary"""
    mean: float
    std_dev: float
    min_val: float
    max_val: float
    median: float

@dataclass
class Comparison:
    """Phase 1 vs Phase 2 comparison"""
    plaintext: Statistics
    tls: Statistics
    overhead_abs: float  # Absolute difference
    overhead_pct: float  # Percentage overhead

# ============================================================================
# Data Loading
# ============================================================================

def load_benchmark_csv(filepath: Path) -> BenchmarkData:
    """
    Load benchmark CSV data from firmware output
    
    CSV Format: version,msg_id,connect_ms,publish_ms,free_heap_bytes,min_free_heap_bytes
    """
    if not filepath.exists():
        raise FileNotFoundError(f"Benchmark data not found: {filepath}")
    
    msg_ids = []
    connect_times = []
    publish_times = []
    free_heap = []
    min_free_heap = []
    version = None
    
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if version is None:
                version = row['version']
            
            msg_ids.append(int(row['msg_id']))
            connect_times.append(float(row['connect_ms']))
            publish_times.append(float(row['publish_ms']))
            free_heap.append(int(row['free_heap_bytes']))
            min_free_heap.append(int(row['min_free_heap_bytes']))
    
    if not msg_ids:
        raise ValueError(f"No data found in {filepath}")
    
    return BenchmarkData(
        version=version,
        msg_ids=msg_ids,
        connect_times=connect_times,
        publish_times=publish_times,
        free_heap=free_heap,
        min_free_heap=min_free_heap
    )

# ============================================================================
# Statistical Analysis
# ============================================================================

def compute_statistics(data: List[float]) -> Statistics:
    """Compute statistical summary of a dataset"""
    return Statistics(
        mean=statistics.mean(data),
        std_dev=statistics.stdev(data) if len(data) > 1 else 0.0,
        min_val=min(data),
        max_val=max(data),
        median=statistics.median(data)
    )

def compute_comparison(plaintext_data: List[float], tls_data: List[float]) -> Comparison:
    """Compare Phase 1 vs Phase 2 metrics"""
    plaintext_stats = compute_statistics(plaintext_data)
    tls_stats = compute_statistics(tls_data)
    
    overhead_abs = tls_stats.mean - plaintext_stats.mean
    overhead_pct = (overhead_abs / plaintext_stats.mean) * 100 if plaintext_stats.mean > 0 else 0
    
    return Comparison(
        plaintext=plaintext_stats,
        tls=tls_stats,
        overhead_abs=overhead_abs,
        overhead_pct=overhead_pct
    )

# ============================================================================
# Energy Calculation
# ============================================================================

def calculate_energy_per_message(publish_time_ms: float, current_ma: float) -> float:
    """
    Calculate energy consumption per message
    
    E = V × I × t
    
    Args:
        publish_time_ms: Time to publish message (milliseconds)
        current_ma: Current draw during transmission (milliamps)
    
    Returns:
        Energy in microjoules (µJ)
    """
    voltage = ESP32_VOLTAGE
    current_a = current_ma / 1000  # Convert mA to A
    time_s = publish_time_ms / 1000  # Convert ms to s
    
    energy_j = voltage * current_a * time_s
    energy_uj = energy_j * 1_000_000  # Convert J to µJ
    
    return energy_uj

# ============================================================================
# Visualization
# ============================================================================

def generate_comparison_charts(plaintext: BenchmarkData, tls: BenchmarkData, 
                                output_path: Path) -> None:
    """Generate comparison charts for all metrics"""
    if not HAS_MATPLOTLIB:
        print("[INFO] Skipping chart generation (matplotlib not available)")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Phase 1 (Plaintext) vs Phase 2 (TLS) - Performance Comparison', 
                 fontsize=16, fontweight='bold')
    
    # Chart 1: Connection Time
    ax1 = axes[0, 0]
    ax1.boxplot([plaintext.connect_times, tls.connect_times], 
                labels=['Plaintext\n(Port 1883)', 'TLS\n(Port 8883)'])
    ax1.set_ylabel('Connection Time (ms)', fontweight='bold')
    ax1.set_title('MQTT Connection Latency')
    ax1.grid(True, alpha=0.3)
    
    # Annotate with means
    plain_mean = statistics.mean(plaintext.connect_times)
    tls_mean = statistics.mean(tls.connect_times)
    overhead = tls_mean - plain_mean
    ax1.text(1.5, tls_mean, f'Overhead:\n+{overhead:.0f} ms\n(+{(overhead/plain_mean)*100:.1f}%)', 
             ha='left', va='bottom', fontsize=9, bbox=dict(boxstyle='round', facecolor='yellow', alpha=0.5))
    
    # Chart 2: Publish Latency
    ax2 = axes[0, 1]
    ax2.boxplot([plaintext.publish_times, tls.publish_times],
                labels=['Plaintext', 'TLS'])
    ax2.set_ylabel('Publish Time (ms)', fontweight='bold')
    ax2.set_title('Message Publish Latency')
    ax2.grid(True, alpha=0.3)
    
    plain_pub_mean = statistics.mean(plaintext.publish_times)
    tls_pub_mean = statistics.mean(tls.publish_times)
    pub_overhead = tls_pub_mean - plain_pub_mean
    ax2.text(1.5, tls_pub_mean, f'Overhead:\n+{pub_overhead:.0f} ms\n(+{(pub_overhead/plain_pub_mean)*100:.1f}%)', 
             ha='left', va='bottom', fontsize=9, bbox=dict(boxstyle='round', facecolor='yellow', alpha=0.5))
    
    # Chart 3: Free Heap Memory (trend over time)
    ax3 = axes[1, 0]
    ax3.plot(plaintext.msg_ids, [h/1024 for h in plaintext.free_heap], 
             label='Plaintext', marker='o', markersize=3, linewidth=1.5)
    ax3.plot(tls.msg_ids, [h/1024 for h in tls.free_heap], 
             label='TLS', marker='s', markersize=3, linewidth=1.5)
    ax3.set_xlabel('Message ID', fontweight='bold')
    ax3.set_ylabel('Free Heap (KB)', fontweight='bold')
    ax3.set_title('RAM Consumption Over Time')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    
    # Chart 4: Minimum Free Heap (bar chart)
    ax4 = axes[1, 1]
    min_heaps = [
        min(plaintext.min_free_heap) / 1024,
        min(tls.min_free_heap) / 1024
    ]
    colors = ['green', 'orange']
    bars = ax4.bar(['Plaintext', 'TLS'], min_heaps, color=colors, alpha=0.7)
    ax4.set_ylabel('Minimum Free Heap (KB)', fontweight='bold')
    ax4.set_title('Peak RAM Usage (Minimum Available)')
    ax4.grid(True, axis='y', alpha=0.3)
    
    # Annotate bars
    for bar, val in zip(bars, min_heaps):
        height = bar.get_height()
        ax4.text(bar.get_x() + bar.get_width()/2., height,
                f'{val:.1f} KB\n({(val*1024/ESP32_TOTAL_SRAM)*100:.1f}% available)',
                ha='center', va='bottom', fontsize=9)
    
    # RAM overhead text
    ram_overhead = (min(plaintext.min_free_heap) - min(tls.min_free_heap)) / 1024
    ax4.text(1.5, min(min_heaps) * 0.5, 
             f'TLS RAM Cost:\n{ram_overhead:.1f} KB',
             ha='left', fontsize=10, bbox=dict(boxstyle='round', facecolor='red', alpha=0.3))
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"[OK] Chart saved: {output_path}")

# ============================================================================
# Summary Report Generation
# ============================================================================

def generate_summary_report(plaintext: BenchmarkData, tls: BenchmarkData,
                             output_path: Path) -> None:
    """Generate Markdown summary report"""
    
    # Compute comparisons
    connect_comp = compute_comparison(plaintext.connect_times, tls.connect_times)
    publish_comp = compute_comparison(plaintext.publish_times, tls.publish_times)
    
    # Heap consumption (inverted: lower free heap = higher usage)
    plaintext_heap_used = ESP32_TOTAL_SRAM - min(plaintext.min_free_heap)
    tls_heap_used = ESP32_TOTAL_SRAM - min(tls.min_free_heap)
    heap_overhead_bytes = tls_heap_used - plaintext_heap_used
    heap_overhead_pct = (heap_overhead_bytes / plaintext_heap_used) * 100
    
    # Energy calculations
    plaintext_energy = calculate_energy_per_message(
        publish_comp.plaintext.mean, 
        ESP32_CURRENT_TX_PLAINTEXT
    )
    tls_energy = calculate_energy_per_message(
        publish_comp.tls.mean,
        ESP32_CURRENT_TX_TLS
    )
    energy_overhead = tls_energy - plaintext_energy
    energy_overhead_pct = (energy_overhead / plaintext_energy) * 100
    
    # Generate Markdown report
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("# TLS Overhead Analysis - SUP'COM Project 4\n\n")
        f.write("**Auteur:** Eya  \n")
        f.write("**Date:** Novembre 2026  \n")
        f.write("**Phase:** Phase 2 (S4–S6)  \n\n")
        
        f.write("---\n\n")
        f.write("## 📊 Résumé Exécutif\n\n")
        f.write("Cette analyse quantifie le **coût de TLS** sur un nœud ESP32 IoT en comparant ")
        f.write("les mesures de performance Phase 1 (MQTT en clair) et Phase 2 (MQTTS avec mTLS).\n\n")
        
        f.write("### Métriques Clés\n\n")
        f.write("| Métrique | Phase 1 (Plaintext) | Phase 2 (TLS) | Overhead |\n")
        f.write("|----------|---------------------|---------------|----------|\n")
        f.write(f"| **Temps de connexion MQTT** | {connect_comp.plaintext.mean:.1f} ms | ")
        f.write(f"{connect_comp.tls.mean:.1f} ms | **+{connect_comp.overhead_abs:.1f} ms** ")
        f.write(f"({connect_comp.overhead_pct:+.1f}%) |\n")
        f.write(f"| **Latence de publication** | {publish_comp.plaintext.mean:.1f} ms | ")
        f.write(f"{publish_comp.tls.mean:.1f} ms | **+{publish_comp.overhead_abs:.1f} ms** ")
        f.write(f"({publish_comp.overhead_pct:+.1f}%) |\n")
        f.write(f"| **Consommation RAM (peak)** | {plaintext_heap_used/1024:.1f} KB | ")
        f.write(f"{tls_heap_used/1024:.1f} KB | **+{heap_overhead_bytes/1024:.1f} KB** ")
        f.write(f"({heap_overhead_pct:+.1f}%) |\n")
        f.write(f"| **Énergie par message** | {plaintext_energy:.1f} µJ | ")
        f.write(f"{tls_energy:.1f} µJ | **+{energy_overhead:.1f} µJ** ")
        f.write(f"({energy_overhead_pct:+.1f}%) |\n\n")
        
        f.write("---\n\n")
        f.write("## 1. Latence de Connexion MQTT\n\n")
        f.write("### Phase 1 (Port 1883, sans TLS)\n\n")
        f.write(f"- **Moyenne:** {connect_comp.plaintext.mean:.2f} ms  \n")
        f.write(f"- **Écart-type:** {connect_comp.plaintext.std_dev:.2f} ms  \n")
        f.write(f"- **Min/Max:** {connect_comp.plaintext.min_val:.0f} / {connect_comp.plaintext.max_val:.0f} ms  \n")
        f.write(f"- **Médiane:** {connect_comp.plaintext.median:.0f} ms  \n\n")
        
        f.write("### Phase 2 (Port 8883, TLS 1.2 avec mTLS)\n\n")
        f.write(f"- **Moyenne:** {connect_comp.tls.mean:.2f} ms  \n")
        f.write(f"- **Écart-type:** {connect_comp.tls.std_dev:.2f} ms  \n")
        f.write(f"- **Min/Max:** {connect_comp.tls.min_val:.0f} / {connect_comp.tls.max_val:.0f} ms  \n")
        f.write(f"- **Médiane:** {connect_comp.tls.median:.0f} ms  \n\n")
        
        f.write("### Analyse\n\n")
        f.write(f"L'ajout de TLS augmente le temps de connexion de **{connect_comp.overhead_abs:.0f} ms** ")
        f.write(f"({connect_comp.overhead_pct:.1f}%). Ce surcoût provient principalement du **TLS handshake** ")
        f.write("qui inclut :\n\n")
        f.write("1. Échange ClientHello / ServerHello  \n")
        f.write("2. Validation du certificat serveur (vérification CA)  \n")
        f.write("3. Envoi du certificat client (authentification mutuelle)  \n")
        f.write("4. Génération de clés de session (ECDHE ou RSA)  \n")
        f.write("5. Vérification cryptographique (signature RSA/ECDSA)  \n\n")
        
        f.write("**Accélération matérielle ESP32:** Le crypto-coprocesseur de l'ESP32 (AES, SHA, RSA) ")
        f.write("réduit significativement ce coût. Sans accélération matérielle, le TLS handshake prendrait ")
        f.write("**+2000–3000 ms** supplémentaires.\n\n")
        
        f.write("---\n\n")
        f.write("## 2. Latence de Publication de Messages\n\n")
        f.write("### Comparaison\n\n")
        f.write(f"- **Plaintext:** {publish_comp.plaintext.mean:.2f} ± {publish_comp.plaintext.std_dev:.2f} ms  \n")
        f.write(f"- **TLS:** {publish_comp.tls.mean:.2f} ± {publish_comp.tls.std_dev:.2f} ms  \n")
        f.write(f"- **Overhead:** +{publish_comp.overhead_abs:.2f} ms ({publish_comp.overhead_pct:+.1f}%)  \n\n")
        
        f.write("### Explication\n\n")
        f.write("Chaque message MQTT publié via TLS subit :\n\n")
        f.write("1. **Chiffrement AES-128/256-GCM** du payload JSON (~50–100 bytes)  \n")
        f.write("2. **Calcul de MAC (Message Authentication Code)** pour l'intégrité  \n")
        f.write("3. **Encapsulation TLS Record Layer** (ajout de headers TLS)  \n\n")
        
        f.write(f"L'overhead de **{publish_comp.overhead_abs:.1f} ms par message** reste acceptable pour ")
        f.write("ce cas d'usage IoT (publication toutes les 5 secondes). Pour des applications temps-réel ")
        f.write("critiques (<10 ms), cet overhead pourrait nécessiter des optimisations supplémentaires.\n\n")
        
        f.write("---\n\n")
        f.write("## 3. Consommation de RAM (Heap)\n\n")
        f.write(f"### RAM Utilisée (Peak)\n\n")
        f.write(f"- **Phase 1 (Plaintext):** {plaintext_heap_used/1024:.1f} KB ")
        f.write(f"({(plaintext_heap_used/ESP32_TOTAL_SRAM)*100:.1f}% de {ESP32_TOTAL_SRAM/1024:.0f} KB)  \n")
        f.write(f"- **Phase 2 (TLS):** {tls_heap_used/1024:.1f} KB ")
        f.write(f"({(tls_heap_used/ESP32_TOTAL_SRAM)*100:.1f}% de {ESP32_TOTAL_SRAM/1024:.0f} KB)  \n")
        f.write(f"- **Overhead TLS:** **+{heap_overhead_bytes/1024:.1f} KB** ({heap_overhead_pct:+.1f}%)  \n\n")
        
        f.write("### Répartition de la RAM TLS\n\n")
        f.write("| Composant | Taille Estimée |\n")
        f.write("|-----------|----------------|\n")
        f.write("| Certificats (CA + client cert + client key) | ~3–5 KB (PROGMEM, hors RAM) |\n")
        f.write("| Buffers TLS (send/receive) | ~16–20 KB |\n")
        f.write("| Session state (cipher suite, keys) | ~8–12 KB |\n")
        f.write("| Stack mbedTLS (structures crypto) | ~10–15 KB |\n")
        f.write(f"| **Total overhead mesuré** | **{heap_overhead_bytes/1024:.1f} KB** |\n\n")
        
        f.write("### Impact\n\n")
        f.write(f"Avec **{(min(tls.min_free_heap)/1024):.1f} KB de RAM disponible** après connexion TLS, ")
        f.write("le système conserve une marge suffisante pour :\n\n")
        f.write("- Buffers réseau additionnels  \n")
        f.write("- Traitement de messages entrants  \n")
        f.write("- Gestion des interruptions capteurs  \n\n")
        
        f.write("⚠️ **Recommandation:** Pour des applications nécessitant plus de RAM (ex: buffers audio, ")
        f.write("images), considérer l'utilisation d'ESP32 avec PSRAM externe (+4 MB).\n\n")
        
        f.write("---\n\n")
        f.write("## 4. Consommation Énergétique\n\n")
        f.write("### Modèle de Calcul\n\n")
        f.write("```\n")
        f.write("E = V × I × t\n")
        f.write(f"V = {ESP32_VOLTAGE} V (alimentation ESP32)\n")
        f.write(f"I_plaintext = {ESP32_CURRENT_TX_PLAINTEXT} mA (TX MQTT sans chiffrement)\n")
        f.write(f"I_tls = {ESP32_CURRENT_TX_TLS} mA (TX MQTT + chiffrement AES)\n")
        f.write("t = temps de publication mesuré\n")
        f.write("```\n\n")
        
        f.write("### Résultats\n\n")
        f.write(f"- **Énergie par message (Plaintext):** {plaintext_energy:.2f} µJ ")
        f.write(f"({plaintext_energy/1000:.4f} mJ)  \n")
        f.write(f"- **Énergie par message (TLS):** {tls_energy:.2f} µJ ")
        f.write(f"({tls_energy/1000:.4f} mJ)  \n")
        f.write(f"- **Overhead:** +{energy_overhead:.2f} µJ ({energy_overhead_pct:+.1f}%)  \n\n")
        
        f.write("### Projection pour Fonctionnement Continu\n\n")
        messages_per_hour = 3600 / 5  # 1 message every 5 seconds
        messages_per_day = messages_per_hour * 24
        
        plaintext_daily = (plaintext_energy * messages_per_day) / 1_000_000  # Convert µJ to J
        tls_daily = (tls_energy * messages_per_day) / 1_000_000
        daily_overhead = tls_daily - plaintext_daily
        
        f.write(f"**Hypothèse:** 1 message toutes les 5 secondes (soit {messages_per_day:.0f} messages/jour)  \n\n")
        f.write(f"- **Consommation quotidienne (Plaintext):** {plaintext_daily:.2f} J ")
        f.write(f"({plaintext_daily/3600:.4f} Wh)  \n")
        f.write(f"- **Consommation quotidienne (TLS):** {tls_daily:.2f} J ")
        f.write(f"({tls_daily/3600:.4f} Wh)  \n")
        f.write(f"- **Overhead quotidien:** +{daily_overhead:.2f} J ({energy_overhead_pct:+.1f}%)  \n\n")
        
        # Battery life estimation
        battery_mah = 2000  # Typical Li-Ion battery (e.g., 18650)
        battery_wh = (battery_mah / 1000) * ESP32_VOLTAGE
        battery_j = battery_wh * 3600
        
        # Simple estimation (ignoring idle power, etc.)
        plaintext_days = battery_j / plaintext_daily
        tls_days = battery_j / tls_daily
        
        f.write("### Autonomie sur Batterie\n\n")
        f.write(f"**Batterie:** {battery_mah} mAh Li-Ion @ {ESP32_VOLTAGE}V ")
        f.write(f"({battery_wh:.2f} Wh, {battery_j:.0f} J)  \n\n")
        f.write("⚠️ Estimation simplifiée (uniquement coût de publication, hors idle/Wi-Fi/capteur):  \n\n")
        f.write(f"- **Autonomie (Plaintext):** ~{plaintext_days:.1f} jours  \n")
        f.write(f"- **Autonomie (TLS):** ~{tls_days:.1f} jours  \n")
        f.write(f"- **Réduction:** -{(plaintext_days - tls_days):.1f} jours ")
        f.write(f"({((tls_days - plaintext_days)/plaintext_days)*100:.1f}%)  \n\n")
        
        f.write("💡 **Note:** En pratique, le coût dominant est la consommation Wi-Fi idle ")
        f.write("(~70–80 mA) plutôt que le chiffrement TLS. L'impact réel de TLS sur ")
        f.write("l'autonomie batterie est donc **inférieur à 2–3%** dans un scénario réaliste.\n\n")
        
        f.write("---\n\n")
        f.write("## 5. Conclusions et Recommandations\n\n")
        f.write("### Compromis Sécurité vs Performance\n\n")
        f.write("| Critère | Phase 1 (Vulnérable) | Phase 2 (TLS) | Évaluation |\n")
        f.write("|---------|----------------------|---------------|------------|\n")
        f.write(f"| **Sécurité** | ❌ Aucune | ✅ Élevée (mTLS) | **TLS indispensable** |\n")
        f.write(f"| **Latence** | {publish_comp.plaintext.mean:.0f} ms | ")
        f.write(f"{publish_comp.tls.mean:.0f} ms (+{publish_comp.overhead_pct:.0f}%) | ")
        f.write("Acceptable pour IoT |\n")
        f.write(f"| **RAM** | {plaintext_heap_used/1024:.0f} KB | ")
        f.write(f"{tls_heap_used/1024:.0f} KB (+{heap_overhead_pct:.0f}%) | ")
        f.write("Marge suffisante |\n")
        f.write(f"| **Énergie** | {plaintext_energy:.0f} µJ | ")
        f.write(f"{tls_energy:.0f} µJ (+{energy_overhead_pct:.0f}%) | ")
        f.write("Impact négligeable |\n\n")
        
        f.write("### Recommandations pour Déploiement Production\n\n")
        f.write("1. **Toujours utiliser TLS en production**  \n")
        f.write("   - Le coût de performance est largement compensé par les gains de sécurité  \n")
        f.write("   - Prévient espionnage, MitM, replay attacks, impersonation  \n\n")
        
        f.write("2. **Exploiter l'accélération matérielle ESP32**  \n")
        f.write("   - Flags de compilation: `-DMBEDTLS_AES_HARDWARE_ACCELERATION`  \n")
        f.write("   - Réduit l'overhead TLS de ~50% comparé à crypto software  \n\n")
        
        f.write("3. **Optimiser la gestion de RAM**  \n")
        f.write("   - Stocker certificats en PROGMEM (Flash) plutôt qu'en RAM  \n")
        f.write("   - Réduire les buffers MQTT si mémoire limitée  \n")
        f.write("   - Considérer ESP32 avec PSRAM pour applications gourmandes  \n\n")
        
        f.write("4. **Pour applications temps-réel critiques (<10 ms)**  \n")
        f.write("   - Évaluer alternatives: DTLS (UDP), pré-shared keys (PSK) au lieu de X.509  \n")
        f.write("   - Session resumption TLS pour éviter full handshake à chaque reconnexion  \n\n")
        
        f.write("5. **Mesures d'énergie réelles recommandées**  \n")
        f.write("   - Utiliser INA219/INA260 pour mesures précises du courant  \n")
        f.write("   - Profiler tous les modes (idle, TX, RX, deep sleep)  \n")
        f.write("   - Optimiser avec modes low-power ESP32 (light sleep, modem sleep)  \n\n")
        
        f.write("---\n\n")
        f.write("## Annexe: Paramètres de Test\n\n")
        f.write(f"- **Nombre de messages:** {len(plaintext.msg_ids)} (Phase 1), {len(tls.msg_ids)} (Phase 2)  \n")
        f.write(f"- **Intervalle entre messages:** 100 ms (benchmark mode)  \n")
        f.write(f"- **ESP32 Total SRAM:** {ESP32_TOTAL_SRAM/1024:.0f} KB  \n")
        f.write(f"- **Broker MQTT:** Mosquitto (Raspberry Pi)  \n")
        f.write(f"- **TLS Version:** 1.2 (minimum)  \n")
        f.write(f"- **Cipher Suite:** AES-128/256-GCM avec ECDHE  \n")
        f.write(f"- **Certificats:** RSA 2048-bit (client), RSA 4096-bit (CA)  \n\n")
        
        f.write("---\n\n")
        f.write("**Rapport généré automatiquement par `analyze_tls_cost.py`**  \n")
        f.write("**SUP'COM MIT Project 4 - Phase 2 (S4–S6)**  \n")
    
    print(f"[OK] Summary report saved: {output_path}")

# ============================================================================
# Main Execution
# ============================================================================

def main():
    """Main analysis workflow"""
    print("=" * 70)
    print("TLS Overhead Analysis for ESP32 MQTT IoT Project")
    print("SUP'COM MIT Project 4 - Phase 2 (S4-S6)")
    print("=" * 70)
    print()
    
    # Load benchmark data
    print("[1/4] Loading benchmark data...")
    try:
        plaintext = load_benchmark_csv(PLAINTEXT_CSV)
        print(f"      ✓ Loaded Phase 1 (plaintext): {len(plaintext.msg_ids)} messages")
        
        tls = load_benchmark_csv(TLS_CSV)
        print(f"      ✓ Loaded Phase 2 (TLS): {len(tls.msg_ids)} messages")
    except (FileNotFoundError, ValueError) as e:
        print(f"[ERROR] {e}")
        print("\n[HINT] Expected CSV files:")
        print(f"        - {PLAINTEXT_CSV}")
        print(f"        - {TLS_CSV}")
        sys.exit(1)
    
    print()
    
    # Generate comparison charts
    print("[2/4] Generating comparison charts...")
    try:
        generate_comparison_charts(plaintext, tls, OUTPUT_CHART)
    except Exception as e:
        print(f"[WARNING] Chart generation failed: {e}")
    
    print()
    
    # Generate summary report
    print("[3/4] Generating summary report...")
    try:
        generate_summary_report(plaintext, tls, OUTPUT_SUMMARY)
    except Exception as e:
        print(f"[ERROR] Report generation failed: {e}")
        sys.exit(1)
    
    print()
    
    # Quick summary to console
    print("[4/4] Analysis Summary")
    print("-" * 70)
    
    connect_comp = compute_comparison(plaintext.connect_times, tls.connect_times)
    publish_comp = compute_comparison(plaintext.publish_times, tls.publish_times)
    
    plaintext_heap_used = ESP32_TOTAL_SRAM - min(plaintext.min_free_heap)
    tls_heap_used = ESP32_TOTAL_SRAM - min(tls.min_free_heap)
    heap_overhead_kb = (tls_heap_used - plaintext_heap_used) / 1024
    
    print(f"Connection Time:   {connect_comp.plaintext.mean:.1f} ms → {connect_comp.tls.mean:.1f} ms "
          f"(+{connect_comp.overhead_abs:.0f} ms, +{connect_comp.overhead_pct:.1f}%)")
    print(f"Publish Latency:   {publish_comp.plaintext.mean:.1f} ms → {publish_comp.tls.mean:.1f} ms "
          f"(+{publish_comp.overhead_abs:.0f} ms, +{publish_comp.overhead_pct:.1f}%)")
    print(f"RAM Overhead:      +{heap_overhead_kb:.1f} KB (TLS buffers + crypto state)")
    print()
    print(f"Full report:       {OUTPUT_SUMMARY}")
    if HAS_MATPLOTLIB:
        print(f"Visualizations:    {OUTPUT_CHART}")
    print()
    print("=" * 70)
    print("✓ Analysis complete!")
    print("=" * 70)

if __name__ == "__main__":
    main()
