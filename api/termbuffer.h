#pragma once

// these are basically just wrappers around the ANSI codes.
typedef enum TermColor {
  TC_RED,
  TC_GREEN,
  TC_YELLOW,
  TC_BLUE,
  TC_MAGENTA,
  TC_CYAN,
  TC_WHITE,
  TC_BLACK,

  TC_BG_RED,
  TC_BG_GREEN,
  TC_BG_YELLOW,
  TC_BG_BLUE,
  TC_BG_MAGENTA,
  TC_BG_CYAN,
  TC_BG_WHITE,
  TC_BG_BLACK,

  TC_RESET,

  TC_COUNT,
} TermColor;

// store the buffer contiguously and modify the buffer printing using a list of
// TermCommand structures. for example, don't inline the ANSI codes before
// printing.
// on each draw, format the character literal buffer with the list of
// TermCommands, and that's how we insert the ANSI.
typedef enum TermCommandType {
  COMTYPE_COLOR,
  COMTYPE_COUNT,
} TermCommandType;

typedef struct TermCommand {
  TermCommandType type;
  int offset; // offset into the literal text. not the actual offset after
              // applying the ANSI codes.
  union {
    TermColor color;
  } data;
} TermCommand;

typedef void (*sigwinch_handler)();

#define MAX_COMMANDS 256

// stores character buffer and the potential size of the terminal, queried
// directly using ioctl.
typedef struct Termbuffer {
  char *buf;
  int len;

  int last_written_pos; // where did we stop writing last? this is for the
                        // commands.

  sigwinch_handler handler;

  TermCommand commands[MAX_COMMANDS];
  int num_commands;

  // the size of our term.
  int row, col;
} Termbuffer;

void tb_init(Termbuffer *tb);
void tb_handle_resize(Termbuffer *tb);

// positional formatting into the buffer. will print on one line (doesn't handle
// newlines).
void tb_pprintf(Termbuffer *tb, int row, int col, const char *format, ...);

// add a color command to the list, automatically processed on a draw.
void tb_change_color(Termbuffer *tb, TermColor color);

// print out all the characters in the framebuffer.
void tb_draw(Termbuffer *tb);

// fill the tb with spaces.
void tb_clear(Termbuffer *tb);
