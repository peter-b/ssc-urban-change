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

#include <math.h>

#include <glib.h>

#include <ridgeio.h>
#include <ridgeutil.h>

#include "ridge-changemap.h"

#define RATIO_EPSILON 1.0
#define NAN_VAL 0.0

/* ================================================================
 * Internal functions
 * ================================================================ */

static double
square_ratio (ChangeMap *map, int row, int col)
{
  double num = RUT_SURFACE_REF (map->pre, row, col);
  double den = RUT_SURFACE_REF (map->post, row, col);
  if (!isnormal (num)) num = map->nan_val;
  if (!isnormal (den)) den = map->nan_val;
  double r = (RATIO_EPSILON + num) / (RATIO_EPSILON + den);
  r = r*r; /* Square of ratio */
  g_assert (isnormal (r));
  g_assert (r > 0);
  return r;
}

static void
recalibrate (ChangeMap *map)
{
  if (!(map->pre && map->post)) {
    map->calibration = NAN;
  }

  /* Sanity check */
  g_assert (map->pre->rows  >= map->height);
  g_assert (map->post->rows >= map->height);
  g_assert (map->pre->cols  >= map->width);
  g_assert (map->post->cols >= map->width);

  /* Calculate mean square ratio of pre and post images.  Summation is
   * carried out using Kahan sum. In this case, condition number is 1
   * because all values expected to be positive. */
  double N = (double) map->height * (double) map->width;
  double sum = 0;
  double c = 0;
  for (int i = 0; i < map->height; i++) {
    for (int j = 0; j < map->width; j++) {
      double r = square_ratio (map, i, j);
      double y = r - c;
      double t = sum + y;
      c = (t - sum) - y;
      sum = t;
    }
  }

  map->calibration = sum / N;
  g_assert (isnormal (map->calibration));
}

/* ================================================================
 * API functions
 * ================================================================ */

ChangeMap *
change_map_new ()
{
  ChangeMap *result = g_new0 (ChangeMap, 1);
  result->ridges = NULL;
  result->pre = NULL;
  result->post = NULL;
  result->nan_val = NAN_VAL;

  result->height = -1;
  result->width = -1;
  result->calibration = NAN;

  return result;
}

void
change_map_free (ChangeMap *map)
{
  /* Assume the various pointers are owned elsewhere */
  g_free (map);
}

void
change_map_set_ridge_data (ChangeMap *map, RioData *data)
{
  uint32_t height = -1, width = -1;
  g_assert (map);
  g_assert (data);

  g_assert (rio_data_get_type (data) == RIO_DATA_LINES);

  /* Get image size metadata */
  int status;
  status = (rio_data_get_metadata_uint32 (data, RIO_KEY_IMAGE_ROWS, &height)
            && rio_data_get_metadata_uint32 (data, RIO_KEY_IMAGE_COLS, &width));
  if (!status) {
    g_error ("Ridge data has no image size metadata"); /* FIXME */
  }

  map->height = height;
  map->width = width;
  map->ridges = data;
  map->calibration = NAN;
}

void
change_map_set_pre_image (ChangeMap *map, RutSurface *pre)
{
  g_assert (map);
  g_assert (pre);

  /* Check image size */
  if ((pre->rows != map->height) || (pre->cols != map->width)) {
    g_error ("Image size mismatch"); /* FIXME */
  }

  map->pre = pre;
  map->calibration = NAN;
}

void
change_map_set_post_image (ChangeMap *map, RutSurface *post)
{
  g_assert (map);
  g_assert (post);

  /* Check image size */
  if ((post->rows != map->height) || (post->cols != map->width)) {
    g_error ("Image size mismatch"); /* FIXME */
  }

  map->post = post;
  map->calibration = NAN;
}

void
change_map_set_nan (ChangeMap *map, double nan_val)
{
  if (nan_val == map->nan_val) return;
  g_assert (isnormal (nan_val));
  map->nan_val = nan_val;
  map->calibration = NAN;
}

ChangeMapLine *
change_map_get_line (ChangeMap *map, int index)
{
  g_assert (map);
  g_assert (map->ridges);
  g_assert (map->pre);
  g_assert (map->post);
  g_assert (index < rio_data_get_num_entries (map->ridges));

  if (isnan(map->calibration)) recalibrate (map);

  RioLine *ridgeline = rio_data_get_line (map->ridges, index);
  int Np = rio_line_get_length (ridgeline);

  /* Allocate result structure */
  ChangeMapLine *result = g_new0 (ChangeMapLine, 1);
  result->n_segments = Np - 1;
  result->coords[0] = g_new0 (uint32_t, Np);
  result->coords[1] = g_new0 (uint32_t, Np);
  result->change = g_new0 (float, Np - 1);

  /* Copy in coordinate data */
  for (int i = 0; i < Np; i++) {
    RioPoint *p = rio_line_get_point (ridgeline, i);
    result->coords[0][i] = p->row;
    result->coords[1][i] = p->col;
  }

  /* Calculate change coefficients */
  for (int i = 0; i < result->n_segments; i++) {
    int row, col;
    change_map_line_get_pixel (result, i, &row, &col);
    g_assert (row < map->height);
    g_assert (col < map->width);

    double r = square_ratio (map, row, col);
    double d = 1 - map->calibration / r;
    g_assert (isnormal (d));
    result->change[i] = d;
  }

  return result;
}

void
change_map_line_free (ChangeMapLine *line)
{
  if (!line) return;
  g_free (line->coords[0]);
  g_free (line->coords[1]);
  g_free (line->change);
  g_free (line);
}

void
change_map_line_get_pixel (const ChangeMapLine *line, int segment,
                           int *row, int *col)
{
  uint64_t lrow, lcol;
  g_assert (line);
  g_assert (segment < line->n_segments);

  lrow = ((uint64_t) line->coords[0][segment]
          + (uint64_t) line->coords[0][segment + 1]) >> 8;

  lcol = ((uint64_t) line->coords[1][segment]
          + (uint64_t) line->coords[1][segment + 1]) >> 8;

  *row = (int) lrow;
  *col = (int) lcol;
}
