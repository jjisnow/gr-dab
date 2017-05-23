#!/usr/bin/env python
# -*- coding: utf-8 -*-
# 
# Copyright 2017 Moritz Luca Schmid, Communications Engineering Lab (CEL) / Karlsruhe Institute of Technology (KIT).
# 
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
# 

from gnuradio import gr, blocks, trellis
import dab
from math import sqrt

class msc_decode(gr.hier_block2):
    """
    @brief block to extract and decode a sub-channel out of the MSC (Main Service Channel) of a demodulated transmission frame

    - get MSC from byte stream
    - repartition MSC to CIFs (Common Interleaved Frames)
    - select a subchannel and extraxt it (dump rest of CIF)
    - do time deinterleaving
    - do convolutional decoding
    - undo energy dispersal
    - output data stream of one subchannel
    """

    def __init__(self, dab_params, address, size, protection, verbose, debug):
        gr.hier_block2.__init__(self,
                                "msc_decode",
                                gr.io_signature(2, 2, gr.sizeof_float * dab_params.num_carriers * 2, gr.sizeof_char),
                                # Input signature
                                gr.io_signature(1, 1, gr.sizeof_char * 32))  # Output signature
        self.dp = dab_params
        self.address = address
        self.size = size
        self.protect = protection
        self.verbose = verbose
        self.debug = debug

        # calculate n factor (multiple of 8kbits etc.)
        self.n = self.size / self.dp.subch_size_multiple_n[self.protect]

        # calculate puncturing factors (EEP, table 33, 34)
        self.msc_I = self.size * self.dp.msc_cu_size
        if (self.n > 1 or self.protect != 1):
            self.puncturing_L1 = [6 * self.n - 3, 2 * self.n - 3, 6 * self.n - 3, 4 * self.n - 3]
            self.puncturing_L2 = [3, 4 * self.n + 3, 3, 2 * self.n + 3]
            self.puncturing_PI1 = [24, 14, 8, 3]
            self.puncturing_PI2 = [23, 13, 7, 2]
            # calculate length of punctured codeword (11.3.2)
            self.msc_punctured_codeword_length = self.puncturing_L1[self.protect] * 4 * self.dp.puncturing_vectors_ones[
                self.puncturing_PI1[self.protect]] + self.puncturing_L2[self.protect] * 4 * \
                                                     self.dp.puncturing_vectors_ones[
                                                         self.puncturing_PI2[self.protect]] + 12
            self.assembled_msc_puncturing_sequence = self.puncturing_L1[self.protect] * 4 * self.dp.puncturing_vectors[
                self.puncturing_PI1[self.protect]] + self.puncturing_L2[self.protect] * 4 * self.dp.puncturing_vectors[
                self.puncturing_PI2[self.protect]] + self.dp.puncturing_tail_vector
            self.msc_conv_codeword_length = 4*self.msc_I + 24 # 4*I + 24 ()
        # exception in table
        else:
            self.msc_punctured_codeword_length = 5 * 4 * self.dp.puncturing_vectors_ones[13] + 1 * 4 * \
                                                                                               self.dp.puncturing_vectors_ones[
                                                                                                   12] + 12

        # MSC selection and block partitioning
        # select OFDM carriers with MSC
        self.select_msc_syms = dab.select_vectors(gr.sizeof_float, self.dp.num_carriers * 2, self.dp.num_msc_syms,
                                                  self.dp.num_fic_syms)
        # repartition MSC data in CIFs
        self.repartition_msc_to_CIFs = dab.repartition_vectors_make(gr.sizeof_float, self.dp.num_carriers * 2,
                                                                    self.dp.cif_bits, self.dp.num_msc_syms,
                                                                    self.dp.num_cifs)
        # select CUs of one subchannel of each CIF
        self.select_subch = dab.select_vectors_make(gr.sizeof_float, self.dp.cif_bits, self.size, self.address)
        self.nullsink = blocks.null_sink(gr.sizeof_char)
        # time deinterleaving
        self.time_deinterleaver = dab.time_deinterleave_ff_make(self.msc_punctured_codeword_length,
                                                                self.dp.scrambling_vector)
        # unpuncture
        self.unpuncture = dab.unpuncture_vff_make(self.assembled_msc_puncturing_sequence, 0)

        # convolutional decoding
        self.fsm = trellis.fsm(1, 4, [0133, 0171, 0145, 0133])  # OK (dumped to text and verified partially)
        self.conv_v2s = blocks.vector_to_stream(gr.sizeof_float, self.msc_conv_codeword_length)
        table = [
            0, 0, 0, 0,
            0, 0, 0, 1,
            0, 0, 1, 0,
            0, 0, 1, 1,
            0, 1, 0, 0,
            0, 1, 0, 1,
            0, 1, 1, 0,
            0, 1, 1, 1,
            1, 0, 0, 0,
            1, 0, 0, 1,
            1, 0, 1, 0,
            1, 0, 1, 1,
            1, 1, 0, 0,
            1, 1, 0, 1,
            1, 1, 1, 0,
            1, 1, 1, 1
        ]
        assert (len(table) / 4 == self.fsm.O())
        table = [(1 - 2 * x) / sqrt(2) for x in table]
        self.conv_decode = trellis.viterbi_combined_fb(self.fsm, self.msc_I + self.dp.conv_code_add_bits_inputs, 0, 0, 4, table, trellis.TRELLIS_EUCLIDEAN)
        self.conv_s2v = blocks.stream_to_vector(gr.sizeof_char, self.msc_I + self.dp.conv_code_add_bits_inputs)
        self.conv_prune = dab.prune_vectors(gr.sizeof_char, self.dp.msc_conv_codeword_length / 4, 0,
                                            self.dp.conv_code_add_bits_inputs)

        #energy descramble
        self.prbs_src = blocks.vector_source_b(self.dp.prbs(self.msc_I), True)
        self.energy_v2s = blocks.vector_to_stream(gr.sizeof_char, self.msc_I)
        self.add_mod_2 = blocks.xor_bb()
        self.energy_s2v = blocks.stream_to_vector(gr.sizeof_char, self.msc_I)

        # Define blocks and connect them
        self.connect((self, 0),
                     (self.select_msc_syms, 0),
                     (self.repartition_msc_to_CIFs, 0),
                     (self.select_subch, 0),
                     self.time_deinterleaver,
                     self.unpuncture,
                     self.conv_v2s,
                     self.conv_decode,
                     self.conv_s2v,
                     self.conv_prune,
                     self.energy_v2s,
                     self.add_mod_2,
                     self.energy_s2v, #better output stream or vector??
                     (self, 1))
        self.connect((self, 1), (self.select_msc_syms, 1), (self.repartition_msc_to_CIFs, 1), (self.select_subch, 1),
                     self.nullsink)
        self.connect(self.prbs_src, (self.add_mod_2, 1))
