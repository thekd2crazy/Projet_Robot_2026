// Tests du filtre de vitesse des roues (logic/speed_filter.h).
// Sur PC : pio test -e native. Sur l'ESP32 branché en USB : pio test -e logic_esp32.
#include <unity.h>

#include "config.h"
#include "logic/speed_filter.h"

void setUp() {}
void tearDown() {}

// Vitesse constante : le filtre finit par donner exactement cette vitesse.
void test_converges_to_constant_speed() {
  SpeedFilter filter;
  float out = 0.0f;
  for (int i = 0; i < 50; ++i) {
    out = filter.update(200.0f);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f, out);
}

// Une mesure aberrante isolée (parasite, cycle mal mesuré) est jetée par la médiane.
void test_single_spike_is_rejected() {
  SpeedFilter filter;
  for (int i = 0; i < 50; ++i) {
    filter.update(200.0f);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f, filter.update(900.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f, filter.update(200.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f, filter.update(0.0f));  // chute isolée : jetée aussi
}

// Un vrai changement de vitesse passe, avec un petit retard (quelques cycles, pas plus).
void test_real_step_passes_quickly() {
  SpeedFilter filter;
  float out = 0.0f;
  int cycles = 0;
  while (out < 0.9f * 300.0f && cycles < 20) {
    out = filter.update(300.0f);
    ++cycles;
  }
  TEST_ASSERT_TRUE(cycles <= 6);  // alpha = 0,6 : 90 % en 5 cycles (100 ms)
}

// Vitesses négatives (rotation sur place) : même comportement.
void test_negative_speed() {
  SpeedFilter filter;
  float out = 0.0f;
  for (int i = 0; i < 50; ++i) {
    out = filter.update(-90.0f);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -90.0f, out);
}

void test_reset() {
  SpeedFilter filter;
  filter.update(200.0f);
  filter.update(200.0f);
  filter.reset();
  TEST_ASSERT_EQUAL_FLOAT(0.0f, filter.value());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, filter.update(0.0f));
}

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_converges_to_constant_speed);
  RUN_TEST(test_single_spike_is_rejected);
  RUN_TEST(test_real_step_passes_quickly);
  RUN_TEST(test_negative_speed);
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
