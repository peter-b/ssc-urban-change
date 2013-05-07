/*
 * Surrey Space Centre ridge-based urban change detection tools
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

typedef struct _ChangeMap ChangeMap;
typedef struct _ChangeMapLine ChangeMapLine;

struct _ChangeMap {
  /* --- Set by user --- */
  RioData *ridges;
  RutSurface *pre;
  RutSurface *post;
  double nan_val;

  /* --- Generated internally --- */
  int height, width;
  double calibration;
};

struct _ChangeMapLine {
  size_t n_segments;
  uint32_t *coords[2]; /* Arrays of length n_segments+1 */
  float *change;
};

ChangeMap *change_map_new (void);
void change_map_free (ChangeMap *map);
void change_map_set_ridge_data (ChangeMap *map, RioData *data);
void change_map_set_pre_image (ChangeMap *map, RutSurface *pre);
void change_map_set_post_image (ChangeMap *map, RutSurface *post);
void change_map_set_nan (ChangeMap *map, double nan_val);
ChangeMapLine *change_map_get_line (ChangeMap *map, int index);

void change_map_line_free (ChangeMapLine *line);
void change_map_line_get_pixel (const ChangeMapLine *line, int segment,
                                int *row, int *col);

/* ---------------------------------------------------------------- */

enum OutputFormat {
  FORMAT_NONE,
  FORMAT_PDF,
  FORMAT_PNG,
};

typedef struct _OutputOptions OutputOptions;

struct _OutputOptions {
  const char *filename;
  int format;
  size_t height, width;
};

void export_ridge_lines (ChangeMap *map, OutputOptions *cfg);
void export_ridge_mask (ChangeMap *map, OutputOptions *cfg);
