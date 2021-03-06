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

#include <gnuradio/io_signature.h>
#include "ofdm_insert_pilot_vcc_impl.h"

namespace gr {
  namespace dab {

ofdm_insert_pilot_vcc::sptr
ofdm_insert_pilot_vcc::make(const std::vector<gr_complex> &pilot)
{
  return gnuradio::get_initial_sptr
    (new ofdm_insert_pilot_vcc_impl(pilot));
}

ofdm_insert_pilot_vcc_impl::ofdm_insert_pilot_vcc_impl(const std::vector<gr_complex> &pilot)
  : gr::block("ofdm_insert_pilot_vcc",
             gr::io_signature::make2 (2, 2, sizeof(gr_complex)*pilot.size(), sizeof(char)),
             gr::io_signature::make2 (2, 2, sizeof(gr_complex)*pilot.size(), sizeof(char))),
  d_pilot(pilot), d_start(0)
{
}

void 
ofdm_insert_pilot_vcc_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
{
  unsigned ninputs = ninput_items_required.size ();
  for (unsigned int i = 0; i < ninputs; i++)
    ninput_items_required[i] = noutput_items;
}


int 
ofdm_insert_pilot_vcc_impl::general_work (int noutput_items,
                        gr_vector_int &ninput_items,
                        gr_vector_const_void_star &input_items,
                        gr_vector_void_star &output_items)
{
  const gr_complex *iptr = (const gr_complex *) input_items[0];
  const char *frame_start = (const char *) input_items[1];
  
  gr_complex *optr = (gr_complex *) output_items[0];
  char *o_frame_start = (char *) output_items[1];

  int n_produced = 0;
  int n_consumed = 0;

  for ( ; n_consumed<ninput_items[1] && n_consumed<ninput_items[1] && n_produced<noutput_items; n_produced++) {
    if (*frame_start == 1 && d_start==0) {
      d_start = 1;
      for (unsigned int j=0; j<d_pilot.size(); j++)
        *optr++ = d_pilot[j];
    } else {
      for (unsigned int j=0; j<d_pilot.size(); j++)
        *optr++ = *iptr++;
      frame_start++;
      n_consumed++;
      d_start=0;
    }
    *o_frame_start++ = d_start;
  }
  consume_each(n_consumed);

  // printf("ninput_items: %d, noutput_items: %d, consumed: %d, produced: %d\n",ninput_items[0],noutput_items, n_consumed, n_produced);

  return n_produced;
}

}
}
