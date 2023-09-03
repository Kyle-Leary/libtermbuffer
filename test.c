#include "termbuffer.h"
#include <unistd.h>

Termbuffer tb;

const char *lines[] = {
    "hello", "world", "how", "are", "things",
};

void more_stuff() {
  tb_clear(&tb);

  tb_change_color(&tb, TC_RED);
  tb_pprintf(&tb, 0, 0, "hello world");

  tb_change_color(&tb, TC_GREEN);
  tb_change_color(&tb, TC_BG_BLUE);
  tb_change_color(&tb, TC_GREEN);
  tb_change_color(&tb, TC_BG_BLUE);
  tb_change_color(&tb, TC_BG_BLUE);
  tb_change_color(&tb, TC_BG_BLUE);
  tb_change_color(&tb, TC_BG_BLUE);
  tb_change_color(&tb, TC_BG_BLUE);
  tb_change_color(&tb, TC_BG_BLUE);
  tb_pprintf(&tb, 2, 2, "hello world");
  tb_change_color(&tb, TC_BG_RED);
  tb_pprintf(&tb, tb.row - 2, 50, "hello world");

  for (int i = 0; i < 5; i++) {
    tb_pprintf(&tb, 4 + i, 15, "%9s", lines[i]);
  }

  int x = 5;

  tb_change_color(&tb, TC_RESET);

  tb_pprintf(&tb, 10, 10, "%d", x);

  tb_change_color(&tb, TC_RED);
  tb_pprintf(&tb, 11, 10, "%p", &x);

  tb_draw(&tb);
}

int main(int argc, char *argv[]) {
  tb_init(&tb);

  tb.handler = more_stuff;

  more_stuff();

  for (;;) {
    usleep(1000);
  }

  return 0;
}
