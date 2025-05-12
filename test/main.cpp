#include "bell/Logger.h"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
  bell::registerDefaultLogger();

  int result = Catch::Session().run(argc, argv);
  return result;
}