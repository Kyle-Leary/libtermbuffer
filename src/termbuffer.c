#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ansi.h"

#include "macros.h"
#include "termbuffer.h"

static Termbuffer *winched_tb = NULL;

static void sigwinch(int signal) {
  if (winched_tb) {
    tb_handle_resize(winched_tb);
    if (winched_tb->handler) {
      winched_tb->handler();
    }
  }
}

static void move_cursor(int row, int col) {
  printf("\033[%d;%dH", row, col);
  fflush(stdout); // Flush the output buffer
}

void tb_handle_resize(Termbuffer *tb) {
  // save the old lengths
  int old_row = tb->row;
  int old_col = tb->col;
  int old_len = tb->len;

  { // get the new lengths.
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    tb->row = w.ws_row;
    tb->col = w.ws_col;

    // don't hold ansi codes inline with the data. that's injected from the
    // outside, since it's all variable-length.
    tb->len = tb->row * tb->col;
  }

  if (old_row == 0 || old_col == 0) {
    // new tb, malloc
    tb->buf = malloc(tb->len + 1);
  } else {
    // old tb, try realloc
    if (old_len < tb->len) {
      // then we need to resize.
      tb->buf = realloc(tb->buf, tb->len);
    }
  }

  printf("new sz: %d\n", tb->len);
}

void tb_init(Termbuffer *tb) {
  tb->row = 0;
  tb->col = 0;
  tb_handle_resize(tb);

  for (int i = 0; i < tb->len; i++) {
    tb->buf[i] = ' ';
  }

  winched_tb = tb;
  signal(SIGWINCH, sigwinch);
}

void tb_pprintf(Termbuffer *tb, int row, int col, const char *format, ...) {
  RUNTIME_ASSERT(row >= 0);
  RUNTIME_ASSERT(row < tb->row);
  RUNTIME_ASSERT(col >= 0);
  RUNTIME_ASSERT(col < tb->col);

  // how many columns do we have left in the current row?
  int left_in_row = tb->col - col;
  char buf[left_in_row];

  va_list args;
  va_start(args, format);
  // cut off the rest of the text if it doesn't fit on the line.
  int required_size = vsnprintf(buf, left_in_row, format, args);

  int bytes_to_copy =
      (required_size < left_in_row) ? required_size : left_in_row - 1;

  int pos = col + (row * tb->col);
  memcpy(&tb->buf[pos], buf, bytes_to_copy);

  tb->last_written_pos = bytes_to_copy + pos + 1;

  va_end(args);
}

// print out all the characters in the framebuffer.
void tb_draw(Termbuffer *tb) {
  move_cursor(0, 0);
  fflush(stdout);

  system("clear");

  int last_offset = 0;
  int ansi_offset =
      0; // count up the offsets that all the commands generate in the text, so
         // that we can keep writing to the proper place.
  for (int i = 0; i < tb->num_commands; i++) {
    TermCommand tc = tb->commands[i];

    char *code_string = NULL;
    int code_strlen = 0;

    switch (tc.type) {
    case COMTYPE_COLOR: {
      TermColor color = tc.data.color;

      switch (color) {
#define CASE(color)                                                            \
  case TC_##color: {                                                           \
    code_string = ANSI_##color;                                                \
    code_strlen = 5;                                                           \
  } break;

#define BGCASE(color)                                                          \
  case TC_BG_##color: {                                                        \
    code_string = ANSI_BG_##color;                                             \
    code_strlen = 5;                                                           \
  } break;

#define CASES(color)                                                           \
  CASE(color)                                                                  \
  BGCASE(color)

        CASES(RED)
        CASES(GREEN)
        CASES(YELLOW)
        CASES(BLUE)
        CASES(MAGENTA)
        CASES(CYAN)
        CASES(WHITE)
        CASES(BLACK)

#undef CASE
#undef BGCASE
#undef CASES

      case TC_RESET: {
        code_string = ANSI_RESET;
        code_strlen = 4;
      } break;
      default:
        break;
      }

    } break;
    default:
      break;
    }

    // print from the end of the last code to the beginning of the new code, and
    // all the characters that should go between.
    fwrite(&tb->buf[last_offset], sizeof(char), tc.offset - last_offset,
           stdout);
    if (code_string) {
      // then, write the actual code string into the right place.
      fwrite(code_string, sizeof(char), code_strlen, stdout);
      ansi_offset += code_strlen;
    }

    last_offset = tc.offset;
  }

  // then, print the rest after the last command.
  printf("%s", &tb->buf[last_offset]);

  // we've used up all the commands, reset them for the next draw.
  tb->num_commands = 0;

  tb->buf[tb->len] = '\0';
}

void tb_clear(Termbuffer *tb) {
  for (int i = 0; i < tb->len; i++) {
    tb->buf[i] = ' ';
  }
}

static inline void _append_command(Termbuffer *tb, TermCommand *tc) {
  memcpy(&tb->commands[tb->num_commands], tc, sizeof(TermCommand));
  tb->num_commands++;
  RUNTIME_ASSERT_MSG(tb->num_commands < MAX_COMMANDS, "increase MAX_COMMANDS.");
}

void tb_change_color(Termbuffer *tb, TermColor color) {
  _append_command(tb, &(TermCommand){.offset = tb->last_written_pos,
                                     .data.color = color,
                                     .type = COMTYPE_COLOR});
}

void tb_clean(Termbuffer *tb) {
  // no longer winching the tb, since its freed.
  if (tb == winched_tb)
    winched_tb = NULL;

  free(tb->buf);
}
