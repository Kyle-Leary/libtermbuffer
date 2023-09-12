#include "termbuffer.h"
#include <unistd.h>

Termbuffer tb;

const char *lines[] = {
    "hello", "world", "how", "are", "things",
};

void more_stuff() {
  tb_clear(&tb);

  tb_pprintf(&tb, 0, 0, "hello world");

  tb_change_positional_color(&tb, TC_BLUE, 0, 2);
  tb_change_positional_color(&tb, TC_RESET, 0, 7);
  tb_change_positional_color(&tb, TC_RED, 0, 4);
  tb_change_positional_color(&tb, TC_RESET, 0, 6);

  tb_pprintf(&tb, 2, 2, "hello world");
  tb_pprintf(&tb, tb.row - 2, 50, "hello world");
  tb_change_positional_color(&tb, TC_RED, 2, 4);
  tb_change_positional_color(&tb, TC_RESET, 2, 9);

  for (int i = 0; i < 5; i++) {
    tb_pprintf(&tb, 4 + i, 15, "%9s", lines[i]);
  }

  int x = 5;

  tb_pprintf(&tb, 10, 10, "%d", x);

  tb_pprintf(&tb, 11, 10, "%p", &x);

  tb_draw(&tb);
}

int main(int argc, char *argv[]) {
  tb_init(&tb);

  tb.handler = more_stuff;

  for (;;) {
    more_stuff();
    usleep(100000);
  }

  return 0;
}
