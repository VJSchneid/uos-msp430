#include <catch2/catch.hpp>

#include <uos/detail/autoselect.hpp>

using namespace uos;

struct test_selector {
  static void select() {
    selected++;
  }
  static void deselect() {
    deselected++;
  }
  static inline int selected = 0;
  static inline int deselected = 0;
};

TEST_CASE("automatic select and deselect using autoselect", "[autoselect]") {
  REQUIRE(test_selector::selected == 0);
  REQUIRE(test_selector::deselected == 0);

  {
    autoselect<test_selector> as;
    CHECK(test_selector::selected == 1);
    REQUIRE(test_selector::deselected == 0);
  }

  CHECK(test_selector::deselected == 1);
}