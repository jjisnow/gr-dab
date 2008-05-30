/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dab_ofdm_ffs_sample.h>
#include <gr_io_signature.h>

/*
 * Create a new instance of dab_ofdm_ffs_sample and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
dab_ofdm_ffs_sample_sptr 
dab_make_ofdm_ffs_sample (unsigned int symbol_length, unsigned int fft_length, unsigned int num_symbols, float alpha, unsigned int sample_rate)
{
  return dab_ofdm_ffs_sample_sptr (new dab_ofdm_ffs_sample (symbol_length, fft_length, num_symbols, alpha, sample_rate));
}

dab_ofdm_ffs_sample::dab_ofdm_ffs_sample (unsigned int symbol_length, unsigned int fft_length, unsigned int num_symbols, float alpha, unsigned int sample_rate) : 
  gr_sync_block ("ofdm_ffs_sample",
             gr_make_io_signature2 (2, 2, sizeof(float), sizeof(char)),
             gr_make_io_signature (1, 1, sizeof(float))),
  d_symbol_length(symbol_length), d_fft_length(fft_length), d_num_symbols(num_symbols), d_alpha(alpha), d_sample_rate(sample_rate), d_cur_symbol(num_symbols), d_cur_sample(0), d_ffs_error_sum(0), d_estimated_error(0), d_estimated_error_per_sample(0)
{
}



int 
dab_ofdm_ffs_sample::work (int noutput_items,
      gr_vector_const_void_star &input_items,
      gr_vector_void_star &output_items)
{
  const float *iptr = (const float *) input_items[0];
  const char *trigger = (const char *) input_items[1];
  float *optr = (float *) output_items[0];

  float new_estimate;

  for (int i=0; i<noutput_items; i++) {
    if (*trigger++ == 1) { /* new frame starts */
      d_cur_symbol = 0;
      d_cur_sample = 0;
      d_ffs_error_sum = 0;
    } 
    
    d_cur_sample++;

    if (d_cur_sample==d_symbol_length) { /* new symbol starts */
      d_cur_sample = 0;

      if (d_cur_symbol<d_num_symbols) {
        new_estimate = *iptr;

        if (d_cur_symbol>0) {
          if (d_ffs_error_sum/d_cur_symbol < -M_PI/2 && new_estimate > M_PI/2)
            new_estimate -= 2*M_PI;
          else if (d_ffs_error_sum/d_cur_symbol > M_PI/2 && new_estimate < -M_PI/2)
            new_estimate += 2*M_PI;
        }

        d_ffs_error_sum += new_estimate;
      }

      if (d_cur_symbol == d_num_symbols-1) { /* update estimated error */
        d_ffs_error_sum /= d_num_symbols; /* average */

        /* if the offset is close to half of the subcarrier bandwidth, it may
         * jump from some large positive value to some large negative value.
         * with averaging, this is a problem - we have to detect it (although
         * it really only makes a difference when the offset is very close to
         * half the subcarrier bandwidth)
         
         * note: if there is an offset of one subcarrier bandwidth, the phase
         * offset in fft_length samples is 2pi */
        if (d_estimated_error < -M_PI/2 && d_ffs_error_sum > M_PI/2) {
          fprintf(stderr, "ofdm_ffs_sample: switch detected: neg -> pos\n");
          d_estimated_error += 2*M_PI; 
        } else if (d_estimated_error > M_PI/2 && d_ffs_error_sum < -M_PI/2) {
          fprintf(stderr, "ofdm_ffs_sample: switch detected: pos -> neg\n");
          d_estimated_error -= 2*M_PI; 
        }

        /* the following distinction is not really needed; but without it,
         * simulation would need to run much longer, becuase the
         * synchronisation would need time to adjust to the offset */
        if (d_estimated_error == 0)
          d_estimated_error = d_ffs_error_sum; /* first time -> fast adjustment */
        else
          d_estimated_error = d_alpha*d_ffs_error_sum + (1-d_alpha)*d_estimated_error; /* slow adjustment */

        d_estimated_error_per_sample = d_estimated_error / (float)d_fft_length;
        fprintf(stderr, "ofdm_ffs_sample: d_estimated_error: %f (%3.2f Hz)\n", d_estimated_error, d_estimated_error_per_sample*d_sample_rate/(2*M_PI));
      }

      d_cur_symbol++;
    } 

    *optr++ = d_estimated_error_per_sample;
    iptr++;
  }

  return noutput_items;
}
