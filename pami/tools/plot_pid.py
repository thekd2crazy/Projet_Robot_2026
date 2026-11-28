"""Trace les courbes du banc test_pid à partir d'un fichier de log du moniteur série.

1. Enregistrer le moniteur dans un fichier (depuis le dossier pami/) :
       pio device monitor -e test_pid -f log2file          (USB)
       pio device monitor -e test_pid_wifi -f log2file     (Wi-Fi)
   PlatformIO crée un fichier platformio-device-monitor-<date>.log dans le dossier courant.
   Dans le banc : v (courbes CSV), puis e, s ou c. On peut faire plusieurs essais à la suite.
2. Tracer :
       python tools/plot_pid.py platformio-device-monitor-xxx.log            (dernier essai)
       python tools/plot_pid.py fichier.log --essai 1                         (1er essai du fichier)
       python tools/plot_pid.py fichier.log --png essai.png                   (enregistrer l'image)
   Sans nom de fichier : le log le plus récent du dossier courant.

Le script garde seulement les lignes qui contiennent « CSV; ». Chaque ligne d'en-tête
(CSV;t_ms;...) commence un nouvel essai. Il marche avec l'ancien format (sans brutG / brutD)
et le nouveau.
"""

import argparse
import glob
import os
import statistics
import sys

import matplotlib.pyplot as plt

# Comme TEST_BENCH_PID_SETTLE_MS dans config.h : début du régime établi.
SETTLE_MS = 1500
# Comme TEST_BENCH_PID_RISE_FRACTION : ligne « 90 % de la consigne ».
RISE_FRACTION = 0.9


def readTests(path):
    """Renvoie la liste des essais. Un essai = dict {nom de colonne: liste de valeurs}."""
    tests = []
    columns = None
    with open(path, encoding="utf-8", errors="replace") as file:
        for line in file:
            start = line.find("CSV;")
            if start < 0:
                continue
            fields = line[start:].strip().split(";")[1:]  # on enlève « CSV »
            if fields and fields[0] == "t_ms":
                columns = fields  # en-tête : nouvel essai
                tests.append({name: [] for name in columns})
                continue
            if columns is None or len(fields) != len(columns):
                continue  # ligne coupée ou mélangée à un autre message : ignorée
            try:
                values = [int(v) for v in fields]
            except ValueError:
                continue
            for name, value in zip(columns, values):
                tests[-1][name].append(value)
    return [t for t in tests if t["t_ms"]]  # essais sans aucune ligne : ignorés


def pwmRipple(test, wheel):
    """Écart-type de la PWM en régime établi : plus il est petit, moins la PWM tremble."""
    pwm = [p for t, p in zip(test["t_ms"], test["pwm" + wheel]) if t >= SETTLE_MS]
    return statistics.pstdev(pwm) if len(pwm) >= 2 else None


def plotTest(test, title):
    figure, axes = plt.subplots(2, 2, sharex=True, figsize=(12, 7))
    figure.suptitle(title)
    timeS = [t / 1000 for t in test["t_ms"]]

    for column, (wheel, name) in enumerate([("G", "gauche"), ("D", "droite")]):
        speedAxis = axes[0][column]
        pwmAxis = axes[1][column]

        if "brut" + wheel in test:
            speedAxis.plot(timeS, test["brut" + wheel], color="lightgray", label="mesure brute")
        speedAxis.plot(timeS, test["mesure" + wheel], color="tab:blue", label="mesure (régulateur)")
        speedAxis.plot(timeS, test["cible" + wheel], color="tab:red", linestyle="--", label="consigne")
        target = max(test["cible" + wheel], key=abs)
        if target != 0:
            speedAxis.axhline(RISE_FRACTION * target, color="tab:red", linewidth=0.8, linestyle=":",
                              label="90 % de la consigne")
        speedAxis.set_title(f"Roue {name}")
        speedAxis.set_ylabel("vitesse (mm/s)")
        speedAxis.grid(True, alpha=0.3)
        speedAxis.legend(loc="lower right", fontsize=8)

        pwmAxis.plot(timeS, test["pwm" + wheel], color="tab:green")
        pwmAxis.axvline(SETTLE_MS / 1000, color="gray", linewidth=0.8, linestyle=":")
        ripple = pwmRipple(test, wheel)
        if ripple is not None:
            pwmAxis.set_title(f"PWM : écart-type après {SETTLE_MS / 1000:.1f} s = {ripple:.1f} %",
                              fontsize=9)
        pwmAxis.set_ylabel("PWM (%)")
        pwmAxis.set_xlabel("temps (s)")
        pwmAxis.grid(True, alpha=0.3)

    figure.tight_layout()
    return figure


def main():
    parser = argparse.ArgumentParser(description="Courbes du banc test_pid")
    parser.add_argument("log", nargs="?", help="fichier de log du moniteur (défaut : le plus récent)")
    parser.add_argument("--essai", type=int, help="numéro de l'essai, 1 = le premier (défaut : le dernier)")
    parser.add_argument("--png", help="enregistrer l'image dans ce fichier au lieu de l'afficher")
    args = parser.parse_args()

    path = args.log
    if path is None:
        logs = glob.glob("platformio-device-monitor-*.log")
        if not logs:
            sys.exit("Aucun fichier platformio-device-monitor-*.log ici : donnez le nom du fichier.")
        path = max(logs, key=os.path.getmtime)

    tests = readTests(path)
    if not tests:
        sys.exit(f"Aucune ligne CSV dans {path}. Avez-vous appuyé sur v avant l'essai ?")
    number = args.essai if args.essai is not None else len(tests)
    if not 1 <= number <= len(tests):
        sys.exit(f"Essai {number} introuvable : le fichier en contient {len(tests)}.")

    test = tests[number - 1]
    print(f"{path} : {len(tests)} essai(s), affichage de l'essai {number}"
          f" ({len(test['t_ms'])} lignes, {test['t_ms'][-1] / 1000:.1f} s)")
    for wheel in ("G", "D"):
        ripple = pwmRipple(test, wheel)
        if ripple is not None:
            print(f"  roue {wheel} : écart-type de la PWM en régime établi = {ripple:.1f} %")

    figure = plotTest(test, f"{os.path.basename(path)} — essai {number}")
    if args.png:
        figure.savefig(args.png, dpi=120)
        print(f"Image enregistrée : {args.png}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
