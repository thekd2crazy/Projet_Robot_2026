// Tests du régulateur d'angle PI (logic/heading_controller.h).
// Sur PC : pio test -e native. Sur l'ESP32 branché en USB : pio test -e logic_esp32.
#include <unity.h>

#include "config.h"
#include "logic/heading_controller.h"

void setUp() {}
void tearDown() {}

// La roue droite a fait 1 mm de plus sur un entraxe de 101,6 mm : 0,564° vers la gauche.
void test_heading_from_wheels() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.564f, headingFromWheelsDeg(0.0f, 1.0f, 101.6f));
  TEST_ASSERT_TRUE(headingFromWheelsDeg(1.0f, 0.0f, 101.6f) < 0.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, headingFromWheelsDeg(50.0f, 50.0f, 101.6f));
}

// Angle juste : aucune correction.
void test_no_error_no_correction() {
  HeadingController controller;
  TEST_ASSERT_EQUAL_FLOAT(0.0f, controller.update(0.0f, 0.0f, 0.02f));
}

// Le PAMI a tourné à gauche (angle > 0) : correction négative, la roue droite ralentit.
void test_sign_of_correction() {
  HeadingController controller;
  TEST_ASSERT_TRUE(controller.update(0.0f, 2.0f, 0.02f) < 0.0f);
  controller.reset();
  TEST_ASSERT_TRUE(controller.update(0.0f, -2.0f, 0.02f) > 0.0f);
}

// Première mise à jour : P + un pas d'intégrale.
void test_first_update_value() {
  HeadingController controller;
  const float expected = HEADING_KP_MM_S_PER_DEG * 1.0f + HEADING_KI_MM_S_PER_DEG_S * 1.0f * 0.02f;
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected, controller.update(1.0f, 0.0f, 0.02f));
}

// Même erreur qui dure : la correction grandit (c'est l'intégrale).
void test_integral_grows_with_lasting_error() {
  HeadingController controller;
  const float first = controller.update(1.0f, 0.0f, 0.02f);
  float later = first;
  for (int i = 0; i < 50; i++) {
    later = controller.update(1.0f, 0.0f, 0.02f);
  }
  TEST_ASSERT_TRUE(later > first);
}

// La correction ne dépasse jamais HEADING_CORRECTION_MAX_MM_S.
void test_output_is_limited() {
  HeadingController controller;
  TEST_ASSERT_EQUAL_FLOAT(HEADING_CORRECTION_MAX_MM_S, controller.update(90.0f, 0.0f, 0.02f));
  TEST_ASSERT_EQUAL_FLOAT(-HEADING_CORRECTION_MAX_MM_S, controller.update(-90.0f, 0.0f, 0.02f));
}

// Anti-windup : après une longue saturation, l'erreur disparaît et la correction retombe tout de
// suite sous HEADING_INTEGRAL_MAX_MM_S (l'intégrale ne s'est pas emballée).
void test_anti_windup() {
  HeadingController controller;
  for (int i = 0; i < 1000; i++) {  // 20 s saturé
    controller.update(90.0f, 0.0f, 0.02f);
  }
  const float afterSaturation = controller.update(0.0f, 0.0f, 0.02f);
  TEST_ASSERT_TRUE(afterSaturation <= HEADING_INTEGRAL_MAX_MM_S);
  TEST_ASSERT_TRUE(afterSaturation >= 0.0f);
}

// reset() efface l'intégrale.
void test_reset() {
  HeadingController controller;
  for (int i = 0; i < 100; i++) {
    controller.update(1.0f, 0.0f, 0.02f);
  }
  controller.reset();
  TEST_ASSERT_EQUAL_FLOAT(0.0f, controller.update(0.0f, 0.0f, 0.02f));
}

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_heading_from_wheels);
  RUN_TEST(test_no_error_no_correction);
  RUN_TEST(test_sign_of_correction);
  RUN_TEST(test_first_update_value);
  RUN_TEST(test_integral_grows_with_lasting_error);
  RUN_TEST(test_output_is_limited);
  RUN_TEST(test_anti_windup);
  RUN_TEST(test_reset);
  return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup() {
  delay(2000);  // programme de test uniquement : laisse le temps au moniteur série de se connecter
  runAllTests();
}
void loop() {}
#else
int main() {
  return runAllTests();
}
#endif
