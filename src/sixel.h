#ifndef SIXEL_H
#define SIXEL_H

#include "config.h"

#define DECSIXEL_PARAMS_MAX 16
#define DECSIXEL_PALETTE_MAX 1024
#define DECSIXEL_PARAMVALUE_MAX 65535
#define DECSIXEL_WIDTH_MAX 4096
#define DECSIXEL_HEIGHT_MAX 4096

typedef uint16_t sixel_color_no_t;

typedef struct sixel_image_buffer {
  sixel_color_no_t *data;
  int32_t width;
  int32_t height;
  int32_t palette[DECSIXEL_PALETTE_MAX];
  int16_t ncolors;
  int8_t palette_modified;
  int8_t use_private_register;
} sixel_image_t;

typedef enum parse_state {
  PS_ESC        = 1,  /* ESC */
  PS_DECSIXEL   = 2,  /* DECSIXEL body part ", $, -, ? ... ~ */
  PS_DECGRA     = 3,  /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
  PS_DECGRI     = 4,  /* DECGRI Graphics Repeat Introducer ! Pn Ch */
  PS_DECGCI     = 5,  /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
} parse_state_t;

typedef struct parser_context {
  parse_state_t state;
  int32_t pos_x;
  int32_t pos_y;
  int32_t max_x;
  int32_t max_y;
  int32_t attributed_pan;
  int32_t attributed_pad;
  int32_t attributed_ph;
  int32_t attributed_pv;
  int32_t repeat_count;
  int32_t color_index;
  int32_t bgindex;
  int32_t grid_width;
  int32_t grid_height;
  int32_t param;
  int32_t nparams;
  int32_t params[DECSIXEL_PARAMS_MAX];
  sixel_image_t image;
} sixel_state_t;

int sixel_parser_init(sixel_state_t *st, int32_t fgcolor, int32_t bgcolor, uint8_t use_private_register);
int sixel_parser_parse(sixel_state_t *st, uint8_t *p, size_t len);
int sixel_parser_set_default_color(sixel_state_t *st);
int sixel_parser_finalize(sixel_state_t *st, uint8_t *pixels);
void sixel_parser_deinit(sixel_state_t *st);

#endif
