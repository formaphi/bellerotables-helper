
#include <cstdio>
#include <libpq-fe.h>

int main() {
  std::printf("libpq version: %d\n", PQlibVersion());
  return 0;
}
