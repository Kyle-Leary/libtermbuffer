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

#include <stdio.h>
#include <stdlib.h>

static void _swap(TermCommand *a_cmd_ptr, TermCommand *b_cmd_ptr) {
  TermCommand temp;
  memcpy(&temp, a_cmd_ptr, sizeof(TermCommand));
  memcpy(a_cmd_ptr, b_cmd_ptr, sizeof(TermCommand));
  memcpy(b_cmd_ptr, &temp, sizeof(TermCommand));
}

// we need to weigh certain commands higher than others, so use a proper
// calculation that adds onto the cmd->offset. for instance, if there are two
// commands on the same offset, the RESET should come before the other color
// command.
static int _get_command_value(TermCommand *cmd) {
  int value = cmd->offset;
  value *= 1000;
  switch (cmd->type) {
  case COMTYPE_COLOR: {
    // the reset command has the lowest value assignment in the enum.
    // this way, any other color will be preferred over a reset command.
    value += cmd->data.color;
  } break;

  default: {
    // then just compare offsets, that's fine.
  } break;
  }
  return value;
}

static int _partition(TermCommand *cmds, int low, int high) {
  // comparing offsets.
  int pivot = _get_command_value(&cmds[high]);
  int i = low - 1;

  for (int j = low; j <= high - 1; j++) {
    if (_get_command_value(&cmds[j]) < pivot) {
      i++;
      _swap(&cmds[i], &cmds[j]);
    }
  }

  _swap(&cmds[i + 1], &cmds[high]);
  return (i + 1);
}

void quicksort(TermCommand *cmds, int low, int high) {
  if (low < high) {
    int pi = _partition(cmds, low, high);

    quicksort(cmds, low, pi - 1);
    quicksort(cmds, pi + 1, high);
  }
}

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

int tb_write(Termbuffer *tb, int row, int col, int num_chars, const char *str) {
  if (row < 0 || col < 0 || row >= tb->row || col >= tb->col) {
    // oob write, just block it for now.
    return 0;
  }

  // manually copy this, since we need to skip newlines, as they'll mess up our
  // buffer. later on, we could also handle other escape codes.
  int j = 0;
  int pos = col + (row * tb->col);
  for (int i = 0; i < num_chars; ++i) {
    if (str[i] < 32) {
      // this is a control character, just ignore it.
    } else {
      // if we haven't hit a funny character, just place it into the buffer.
      tb->buf[pos + j] = str[i];
      j++;
    }
  }

  tb->last_written_pos = j + pos + 1;

  return j;
}

void tb_pprintf(Termbuffer *tb, int row, int col, const char *format, ...) {
  // how many columns do we have left in the current row?
  int left_in_row = tb->col - col;
  char buf[left_in_row];

  va_list args;
  va_start(args, format);
  // cut off the rest of the text if it doesn't fit on the line.
  int required_size = vsnprintf(buf, left_in_row, format, args);

  int bytes_to_copy =
      (required_size < left_in_row) ? required_size : left_in_row - 1;

  tb_write(tb, row, col, bytes_to_copy, buf);

  va_end(args);
}

// print out all the characters in the framebuffer.
void tb_draw(Termbuffer *tb) {
  // make sure all the ansi codes from last pass aren't still affecting this
  // one.
  system("clear");

  move_cursor(0, 0);

  int last_offset = 0;
  int ansi_offset =
      0; // count up the offsets that all the commands generate in the text, so
         // that we can keep writing to the proper place.

  quicksort(
      tb->commands, 0,
      tb->num_commands); // make sure that all the command offsets are in order.

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

#undef CASES
#undef BGCASE
#undef CASE

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
  fflush(stdout);

  tb->buf[tb->len] = '\0';
}

void tb_clear(Termbuffer *tb) {
  // we've used up all the commands, reset them for the next draw.
  tb->num_commands = 0;
  tb->last_written_pos = 0;

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

void tb_change_positional_color(Termbuffer *tb, TermColor color, int row,
                                int col) {
  _append_command(tb, &(TermCommand){.offset = col + (row * tb->col),
                                     .data.color = color,
                                     .type = COMTYPE_COLOR});
}

void tb_clean(Termbuffer *tb) {
  // no longer winching the tb, since its freed.
  if (tb == winched_tb)
    winched_tb = NULL;

  free(tb->buf);
}
