// Régulateur d'angle (cap) du PAMI : un PI, sans dérivée (logique pure, testable sur PC).
//
// Contrôle polaire : on pilote le PAMI avec deux grandeurs au lieu de deux roues.
// - la distance d = (distance droite + distance gauche) / 2, gérée par le profil en trapèze ;
// - l'angle θ = (distance droite - distance gauche) / entraxe, géré ici.
// Le régulateur d'angle donne une vitesse de rotation u_θ (en mm/s de roue), et les consignes
// des roues sont : droite = u_d + u_θ, gauche = u_d - u_θ.
// u_θ > 0 : la roue droite va plus vite, le PAMI tourne vers la gauche (cap positif, comme l'odométrie).
//
// Pourquoi un PI et pas seulement un P (l'ancienne correction de cap) : un P laisse un petit écart
// qui dure si une roue frotte plus que l'autre. L'intégrale le rattrape. Pas de dérivée : la mesure
// de l'angle vient des ticks, elle est en marches d'escalier, et la dériver amplifierait ce bruit.
#pragma once

#include "../config.h"

// Angle (en degrés) que donnent les distances des deux roues depuis le début du mouvement.
// Positif = le PAMI a tourné vers la gauche (la roue droite a fait plus de chemin).
inline float headingFromWheelsDeg(float leftMm, float rightMm, float wheelBaseMm) {
  return (rightMm - leftMm) / wheelBaseMm * 57.2957795f;  // radians -> degrés
}

class HeadingController {
public:
  // Renvoie la correction u_θ à ajouter à la roue droite et à retirer à la roue gauche, en mm/s.
  // targetDeg : angle voulu. measuredDeg : angle mesuré. dtS : temps depuis le dernier appel.
  float update(float targetDeg, float measuredDeg, float dtS) {
    const float errorDeg = targetDeg - measuredDeg;

    // Intégrale : corrige un écart d'angle qui dure. Deux protections contre l'emballement
    // (anti-windup), comme dans SpeedController :
    // - on ne l'augmente pas quand la correction est déjà au maximum et que l'erreur pousse encore
    //   dans ce sens (les roues ne peuvent pas faire mieux : intégrer ne servirait à rien, et le
    //   PAMI dépasserait ensuite l'angle voulu) ;
    // - elle reste bornée à HEADING_INTEGRAL_MAX_MM_S dans tous les cas.
    const float correctionBefore = HEADING_KP_MM_S_PER_DEG * errorDeg + integralMmS_;
    const bool saturatedAndPushing =
        (correctionBefore >= HEADING_CORRECTION_MAX_MM_S && errorDeg > 0.0f) ||
        (correctionBefore <= -HEADING_CORRECTION_MAX_MM_S && errorDeg < 0.0f);
    if (!saturatedAndPushing) {
      integralMmS_ += HEADING_KI_MM_S_PER_DEG_S * errorDeg * dtS;
      if (integralMmS_ > HEADING_INTEGRAL_MAX_MM_S) {
        integralMmS_ = HEADING_INTEGRAL_MAX_MM_S;
      }
      if (integralMmS_ < -HEADING_INTEGRAL_MAX_MM_S) {
        integralMmS_ = -HEADING_INTEGRAL_MAX_MM_S;
      }
    }

    float correctionMmS = HEADING_KP_MM_S_PER_DEG * errorDeg + integralMmS_;
    if (correctionMmS > HEADING_CORRECTION_MAX_MM_S) {
      correctionMmS = HEADING_CORRECTION_MAX_MM_S;
    }
    if (correctionMmS < -HEADING_CORRECTION_MAX_MM_S) {
      correctionMmS = -HEADING_CORRECTION_MAX_MM_S;
    }
    return correctionMmS;
  }

  void reset() { integralMmS_ = 0.0f; }

private:
  float integralMmS_ = 0.0f;
};
