
//
//t.elliott converted from original liquid-dsp code to something that would be usable on the STM32H7 series
//
/*
 * Copyright (c) 2007 - 2015 Joseph Gaeddert
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __NCO_CF_H__
#define __NCO_CF_H__

enum {
	LIQUID_NCO=0,
	LIQUID_VCO
};

struct nco_cf_s {
    int  type;           // NCO type (e.g. LIQUID_VCO)
    float               theta;          // NCO phase [radians]
    volatile float               d_theta;        // NCO frequency [radians/sample]
    float               sintab[256];    // sine table
    unsigned int    index;          // table index
    float               sine;           // output sine component
    float               cosine;         // output cosine component
    void (*compute_sincos)(struct nco_cf_s *  _q); // function pointer to computing sine/cosine

    // phase-locked loop
    float               alpha;          // frequency proportion
    float               beta;           // phase proportion
};

void nco_constrain_frequency(struct nco_cf_s *_q);
void nco_mix_block_down(struct nco_cf_s *_q,float complex *_x,float complex *_y,unsigned int _n);
void nco_mix_block_up(struct nco_cf_s *_q,float complex *_x,float complex *_y,unsigned int _n);
void nco_mix_down(struct nco_cf_s *_q,float complex _x,float complex *_y);
void nco_mix_up(struct nco_cf_s *_q,float complex _x,float complex *_y);
void nco_pll_step(struct nco_cf_s *_q,float _dphi);
void nco_cexpf(struct nco_cf_s *_q,float complex *_y);
void nco_sincos(struct nco_cf_s *_q,float *_s,float *_c);
float nco_cos(struct nco_cf_s *_q);
float nco_sin(struct nco_cf_s *_q);
float nco_get_frequency(struct nco_cf_s *_q);
float nco_get_phase(struct nco_cf_s *_q);
void nco_step(struct nco_cf_s *_q);
void nco_adjust_phase(struct nco_cf_s *_q,float _dphi);
void nco_constrain_phase(struct nco_cf_s *_q);
void nco_set_phase(struct nco_cf_s *_q,float _phi);
void nco_adjust_frequency(struct nco_cf_s *_q,float _df);
void nco_set_frequency(struct nco_cf_s *_q,float _dtheta);
void nco_pll_reset(struct nco_cf_s *_q);
void nco_reset(struct nco_cf_s *_q);
void nco_compute_sincos_vco(struct nco_cf_s *_q);
void nco_compute_sincos_nco(struct nco_cf_s *_q);
void nco_pll_set_bandwidth(struct nco_cf_s *_q,float _bw);
struct nco_cf_s *nco_create(int _type);
#endif
