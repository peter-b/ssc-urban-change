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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>

#include <glib.h>
#include <ridgeutil.h>
#include <ridgeio.h>

#include "ridge-changemap.h"

#define DEFAULT_CLASS_LABEL 1

enum OutputMode {
  MODE_RIDGE_LINES,
  MODE_RIDGE_MASK,
};

/* -------------------------------------------------------------------- */

#define GETOPT_OPTIONS "c:hi:m:s"

struct option long_options[] =
  {
    {"class", 1, 0, 'c'},
    {"help", 0, 0, 'h'},
    {"mode", 1, 0, 'm'},
    {"nan", 1, 0, 'i'},
    {"smooth", 0, 0, 's'},
    {0, 0, 0, 0} /* Guard */
  };

static void
usage (char *name, int status)
{
  printf (
"Usage: %s [OPTION ...] [-m MODE] CRDG PRE POST OUTFILE\n"
"\n"
"Modes:\n"
"  ridgelines      Draw vector features coloured by change\n"
"  ridgemask       Draw masked ratio image coloured by change\n"
"\n"
"Options:\n"
"  -m, --mode=MODE Set changemap rendering mode [ridgelines]\n"
"  -s, --smooth    Smooth along ridge lines before evaluating change\n"
"  -c, --class=CLASS  Set class label to use for detection [%i]\n"
"  -i, --nan=VAL   Set non-finite input values to VAL [default 0]\n"
"  -h, --help      Display this message and exit\n"
"\n"
"Generates a change map using a pre-event SAR amplitude image PRE, a\n"
"post-event image POST, and a classified ridge data file CRDG.  Output\n"
"is generated in OUTFILE.  All images should be single-channel 32-bit\n"
"floating point TIFF files.\n"
"\n"
"Please report bugs to %s.\n",
name, DEFAULT_CLASS_LABEL, PACKAGE_BUGREPORT);
  exit (status);
}

/* -------------------------------------------------------------------------- */

static RioData *
ridges_load_check (const char *crdg_fn, int class_label,
                   uint32_t *height, uint32_t *width)
{
  g_assert (crdg_fn);
  g_assert (height);
  g_assert (width);

  RioData *data = rio_data_from_file (crdg_fn);
  if (data == NULL) {
    fprintf (stderr, "ERROR: Failed to load ridge data from '%s': %s.\n",
             crdg_fn, strerror (errno));
    exit (2);
  }

  if (rio_data_get_type (data) != RIO_DATA_LINES) {
    fprintf (stderr, "ERROR: '%s' does not contain ridge line data.\n",
             crdg_fn);
    exit (2);
  }

  /* Determine original image height & width */
  if (!(rio_data_get_metadata_uint32 (data, RIO_KEY_IMAGE_ROWS, height)
        && rio_data_get_metadata_uint32 (data, RIO_KEY_IMAGE_COLS, width))) {
    fprintf (stderr, "ERROR: '%s' contains invalid image size metadata.\n",
             crdg_fn);
    exit (2);
  }

  size_t N = rio_data_get_num_entries (data);

  /* Load classification metadata */
  size_t Nc;
  uint8_t *classification =
    (uint8_t *) rio_data_get_metadata (data, RIO_KEY_IMAGE_CLASSIFICATION, &Nc);

  if (classification == NULL || Nc != N) {
    fprintf (stderr, "WARNING: '%s' contains invalid classification metadata.\n",
             crdg_fn);

    /* If no classification data is present, just return the original
     * ridge data set. */
    return data;
  }

  /* Create a new ridge data set, and copy over all of the features
   * that have the correct class label. */
  RioData *data_c = rio_data_new (RIO_DATA_LINES);
  rio_data_set_metadata_uint32 (data_c, RIO_KEY_IMAGE_ROWS, *height);
  rio_data_set_metadata_uint32 (data_c, RIO_KEY_IMAGE_COLS, *width);
  for (int i = 0; i < N; i++) {
    if (classification[i] != class_label) continue;

    RioLine *l = rio_data_get_line (data, i);
    RioLine *l_c = rio_data_new_line (data_c);
    int M = rio_line_get_length (l);
    for (int j = 0; j < M; j++) {
      RioPoint *p = rio_line_get_point (l, j);
      RioPoint *p_c = rio_line_new_point (l_c);
      memcpy (p_c, p, sizeof(RioPoint));
    }
  }

  rio_data_destroy (data);
  return data_c;
}

static RutSurface *
img_load_check (const char *fn, uint32_t rows, uint32_t cols)
{
  RutSurface *img = rut_surface_from_tiff (fn);
  if (img == NULL) {
    fprintf (stderr, "ERROR: Failed to load TIFF from '%s'.\n", fn);
    exit (3);
  }
  /* Check size */
  if (img->rows != rows || img->cols != cols) {
    fprintf (stderr, "ERROR: Bad image size for '%s' (expected %ux%u).\n", fn,
             rows, cols);
    exit (3);
  }
  return img;
}

/* -------------------------------------------------------------------------- */

int
guess_output_format (const char *filename)
{
  struct _FormatSuffix {
    const char *suffix;
    int format;
  };
  struct _FormatSuffix format_suffixes[] = {
    {"png", FORMAT_PNG},
    {"pdf", FORMAT_PDF},
    {NULL, FORMAT_NONE},
  };

  const char *out_suffix = strrchr (filename, '.');
  if (out_suffix != NULL) {
    ++out_suffix; /* Skip '.' */
    for (struct _FormatSuffix *s = format_suffixes; s->suffix != NULL; ++s) {
      if (strcasecmp (out_suffix, s->suffix) == 0) {
        return s->format;
      }
    }
  }
  return FORMAT_NONE;
}

/* -------------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
  int c, status;
  int cfg_mode = MODE_RIDGE_LINES;
  uint8_t cfg_class = DEFAULT_CLASS_LABEL;
  double cfg_nan = 0;
  int cfg_smooth = 0;
  char *cfg_crdg_fn = NULL;
  char *cfg_pre_fn = NULL;
  char *cfg_post_fn = NULL;
  char *cfg_out_fn = NULL;
  int cfg_format = FORMAT_NONE;

  while (1) {
    c = getopt_long (argc, argv, GETOPT_OPTIONS, long_options, NULL);
    if (c == -1) break;

    switch (c) {
    case 'c':
      status = sscanf (optarg, "%hhu", &cfg_class);
      if (status != 1) {
        fprintf (stderr, "ERROR: Bad argument '%s' to -c option.\n\n",
                 optarg);
        usage (argv[0], 1);
      }
      break;
    case 'h':
      usage (argv[0], 0);
      break;
    case 'i':
      status = sscanf (optarg, "%lf", &cfg_nan);
      if (status != 1) {
        fprintf (stderr, "ERROR: Bad argument '%s' to -i option.\n\n",
                 optarg);
        usage (argv[0], 1);
      }
      break;
    case 'm':
      if (strcmp (optarg, "ridgelines") == 0) {
        cfg_mode = MODE_RIDGE_LINES;
      } else if (strcmp (optarg, "ridgemask") == 0) {
        cfg_mode = MODE_RIDGE_MASK;
      } else {
        fprintf (stderr, "ERROR: Bad argument '%s' to -m option.\n\n",
                 optarg);
        usage (argv[0], 1);
      }
      break;
    case 's':
      cfg_smooth = 1;
      break;

    case '?':
      usage (argv[0], 1);
    default:
      g_assert_not_reached ();
    }
  }

  /* Get filenames */
  if (argc - optind < 4) {
    fprintf (stderr,
             "ERROR: You must specify a ridge data file, pre- and post-event SAR\n"
             "images, and an output filename.\n\n");
    usage (argv[0], 1);
  }

  cfg_crdg_fn = argv[optind++];
  cfg_pre_fn = argv[optind++];
  cfg_post_fn = argv[optind++];
  cfg_out_fn = argv[optind++];

  /* Initialise change map structure */
  ChangeMap *changes = change_map_new ();
  change_map_set_nan (changes, cfg_nan);

  /* Load & check ridge data */
  uint32_t height, width;
  RioData *ridges = ridges_load_check (cfg_crdg_fn, cfg_class, &height, &width);
  change_map_set_ridge_data (changes, ridges);

  /* Load & check pre/post SAR images */
  RutSurface *pre, *post;
  pre = img_load_check (cfg_pre_fn, height, width);
  change_map_set_pre_image (changes, pre);
  post = img_load_check (cfg_post_fn, height, width);
  change_map_set_post_image (changes, post);

  /* Figure out desired output file format */
  /* FIXME should be an explicit command-line option */
  if (cfg_format == FORMAT_NONE) {
    cfg_format = guess_output_format (cfg_out_fn);
  }
  if (cfg_format == FORMAT_NONE) {
    fprintf (stderr, "WARNING: Could not guess output format for '%s'. Using PDF.\n",
             cfg_out_fn);
    cfg_format = FORMAT_PDF;
  }

  /* Output! */
  OutputOptions export_opts;
  export_opts.filename = cfg_out_fn;
  export_opts.format = cfg_format;
  export_opts.height = height;
  export_opts.width = width;

  switch (cfg_mode) {
  case MODE_RIDGE_LINES:
    export_ridge_lines (changes, &export_opts);
    break;
  case MODE_RIDGE_MASK:
    export_ridge_mask (changes, &export_opts);
    break;
  default:
    g_assert_not_reached ();
  };

  /* Cleanup */
  change_map_free (changes);
  rio_data_destroy (ridges);
  rut_surface_destroy (pre);
  rut_surface_destroy (post);
  return 0;
}
