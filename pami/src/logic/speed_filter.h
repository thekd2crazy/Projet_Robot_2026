// Filtre de la vitesse mesurée d'une roue (logique pure, testable sur PC).
//
// La vitesse mesurée = ticks comptés pendant un cycle / durée du cycle. Elle est bruitée :
// - millis() compte en ms entières : un cycle de 20 ms peut valoir 19 ou 21 ms, soit ±5 % de vitesse
//   (±17 mm/s à 350 mm/s) alors que la roue n'a pas changé de vitesse ;
// - un tick de plus ou de moins = ±4,9 mm/s (0,097 mm par tick, cycle de 20 ms) ;
// - aimants de l'encodeur pas tous égaux, parasite isolé.
// Le régulateur réagit à chaque écart : sans filtre, il « corrige » ce bruit et la PWM tremble.
//
// Deux étages :
// 1. Médiane des 3 dernières mesures : une mesure aberrante isolée est jetée (pas mélangée,
//    comme le ferait une moyenne). Retard : 1 cycle (20 ms) sur une rampe.
// 2. Lissage exponentiel : out += (médiane - out) × alpha. Atténue le bruit restant.
//    alpha = 1 : pas de lissage ; plus petit = plus lisse mais plus en retard
//    (retard ≈ (1 - alpha) / alpha cycles, soit 13 ms pour alpha = 0,6).
#pragma once

#include <math.h>

#include "../config.h"

class SpeedFilter {
public:
  // Ajoute la mesure brute de ce cycle (mm/s) et renvoie la vitesse filtrée (mm/s).
  float update(float measuredMmS) {
    buf_[index_] = measuredMmS;
    index_ = (index_ + 1) % 3;
    const float a = buf_[0];
    const float b = buf_[1];
    const float c = buf_[2];
    // Médiane de 3 : le plus grand des deux plus petits.
    const float medianMmS = fmaxf(fminf(a, b), fminf(fmaxf(a, b), c));
    outMmS_ += (medianMmS - outMmS_) * SPEED_FILTER_ALPHA;
    return outMmS_;
  }

  // Dernière vitesse filtrée, sans ajouter de mesure.
  float value() const { return outMmS_; }

  // Retour à « roue arrêtée ». Pas à appeler entre deux mouvements : le filtre doit toujours suivre
  // la vraie roue (une glissade après le freinage, par exemple). Seulement au démarrage.
  void reset() {
    buf_[0] = buf_[1] = buf_[2] = 0.0f;
    outMmS_ = 0.0f;
    index_ = 0;
  }

private:
  float buf_[3] = {0.0f, 0.0f, 0.0f};
  float outMmS_ = 0.0f;
  int index_ = 0;
};
