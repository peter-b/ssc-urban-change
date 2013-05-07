/*
 * Surrey Space Centre urban change detection tool for SAR
 * Copyright (C) 2013 Peter Brett <p.brett@surrey.ac.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <ridgeio.h>
#include <ridgeutil.h>

#include "ridge-changemap.h"

/* ---------------------------------------------------------------- */

/* FIXME this should be made much more general and preferably
 * configurable */

typedef struct _PaletteEntry PaletteEntry;
struct _PaletteEntry {
  float v;
  uint8_t r, g, b;
};

#if 0
static const PaletteEntry PALETTE[] = {
  { 0, 0, 0, 0},
  { 0.75, 255, 0, 0},
  { 1.00, 255, 255, 0},
  { NAN,  0,   0,   0   },
};

static const PaletteEntry PALETTE[] = {
  { 0.00, 230,230,230},
  { 0.50, 191,191,191},
  { 0.75, 128,128,128},
  { 1.00, 0,0,255},
  { NAN, 255, 255, 255},
};
#endif

static const PaletteEntry PALETTE[] = {
  { 0.00, 242,242,242},
  { 0.50, 153,153,153},
  { 0.75, 64,64,64},
  { 1.00, 0,0,255},
  { NAN, 255, 255, 255},
};

void
set_damage_colour (cairo_t *cr, double d)
{
  d = fmax (0, d);
  PaletteEntry start, end;
  start = PALETTE[0];
  for (int i = 1; !isnan(PALETTE[i].v); i++) {
    end = PALETTE[i];
    if (d <= end.v) break;
    start = end;
  }
  double x = (d - start.v) / (end.v - start.v);
  double r = (x * end.r + (1-x) * start.r) / 255;
  double g = (x * end.g + (1-x) * start.g) / 255;
  double b = (x * end.b + (1-x) * start.b) / 255;

  cairo_set_source_rgb (cr, r, g, b);
}

void
set_background_colour (cairo_t *cr)
{
  int i;
  PaletteEntry p;
  for (i = 0, p = PALETTE[i]; !isnan(p.v); p = PALETTE[++i]);
  cairo_set_source_rgb (cr, p.r/255.0, p.g/255.0, p.b/255.0);
}

void
convert_coords (ChangeMapLine *l, int idx, double *x, double *y)
{
  *x = l->coords[1][idx] / 128.0;
  *y = l->coords[0][idx] / 128.0;
}

/* ---------------------------------------------------------------- */

void
export_ridge_lines (ChangeMap *map, OutputOptions *cfg)
{
  cairo_surface_t *surface;
  cairo_status_t status;

  g_assert (map);
  g_assert (cfg);

  /* Create output surface */
  switch (cfg->format) {
  case FORMAT_PNG:
    surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                          cfg->width, cfg->height);
    break;

  case FORMAT_PDF:
    surface = cairo_pdf_surface_create (cfg->filename, cfg->width, cfg->height);
    break;

  default:
    g_assert_not_reached ();
  }

  /* Draw */
  cairo_t *cr = cairo_create (surface);
  cairo_set_line_width (cr, 1);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  set_background_colour (cr);
  cairo_paint (cr);

  int N = rio_data_get_num_entries (map->ridges);

  for (int i = 0; i < N; i++) {
    ChangeMapLine *l = change_map_get_line (map, i);
    for (int j = 0; j < l->n_segments; j++) {
      double x, y;

      set_damage_colour (cr, l->change[j]);

      convert_coords (l, j, &x, &y);
      cairo_move_to (cr, x, y);
      convert_coords (l, j+1, &x, &y);
      cairo_line_to (cr, x, y);

      cairo_stroke (cr);
    }
    change_map_line_free (l);
  }

  cairo_destroy (cr);

  /* Complete output */
  status = cairo_surface_status (surface);
  if (status != CAIRO_STATUS_SUCCESS) {
    fprintf (stderr, "ERROR: %s.\n", cairo_status_to_string (status));
    exit (4);
  }
  switch (cfg->format) {
  case FORMAT_PNG:
    status = cairo_surface_write_to_png (surface, cfg->filename);
    if (status != CAIRO_STATUS_SUCCESS) {
    fprintf (stderr, "ERROR: Could not write to '%s': %s.\n",
             cfg->filename, cairo_status_to_string (status));
    exit (4);
    }
    break;

  case FORMAT_PDF:
    cairo_surface_finish (surface);
    break;

  default:
    g_assert_not_reached ();
  }

  /* Clean up */
  cairo_surface_destroy (surface);
}

void
export_ridge_mask (ChangeMap *map, OutputOptions *cfg) {

  cairo_surface_t *surface;
  cairo_status_t status;

  g_assert (map);
  g_assert (cfg);

  /* Create image surface */
  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                        cfg->width, cfg->height);
  uint8_t *s_data = cairo_image_surface_get_data (surface);
  int stride = cairo_image_surface_get_stride (surface);
  g_assert (s_data != NULL);

  /* Draw */
  cairo_t *cr = cairo_create (surface);

  set_background_colour (cr);
  cairo_paint (cr);

  cairo_surface_flush (surface);

  int N = rio_data_get_num_entries (map->ridges);
  for (int i = 0; i < N; i++) {
    ChangeMapLine *l = change_map_get_line (map, i);
    for (int j = 0; j < l->n_segments; j++) {
      cairo_pattern_t *pattern;
      double r, g, b;
      int row, col;

      change_map_line_get_pixel (l, j, &row, &col);

      /* hack hack hack */
      set_damage_colour (cr, l->change[j]);
      pattern = cairo_get_source (cr);
      cairo_pattern_get_rgba (pattern, &r, &g, &b, NULL);

      /* Directly set pixel value. Build 32-bit value to avoid
       * endianness issues. Use memcpy() to avoid strict aliasing
       * issues. */
      int v = (((int) (r * 255) << 16) +
               ((int) (g * 255) << 8) +
               ((int) (b * 255)));
      size_t offset = stride * row + 4 * col;
      memcpy (s_data + offset, &v, 4);
    }
    change_map_line_free (l);
  }

  cairo_destroy (cr);

  /* Create output file */
  cairo_surface_t *pdf_surface;

  switch (cfg->format) {
  case FORMAT_PNG:
    status = cairo_surface_write_to_png (surface, cfg->filename);
    if (status != CAIRO_STATUS_SUCCESS) {
    fprintf (stderr, "ERROR: Could not write to '%s': %s.\n",
             cfg->filename, cairo_status_to_string (status));
    exit (4);
    }
    break;
  case FORMAT_PDF:
    /* FIXME error checking */
    pdf_surface = cairo_pdf_surface_create (cfg->filename,
                                            cfg->width, cfg->height);
    cr = cairo_create (pdf_surface);

    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint (cr);

    cairo_destroy (cr);
    cairo_surface_finish (pdf_surface);
    cairo_surface_destroy (pdf_surface);
    break;
  default:
    g_assert_not_reached ();
  }

  cairo_surface_destroy (surface);
}
