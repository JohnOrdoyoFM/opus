/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Copyright (c) 2008 Gregory Maxwell 
   Written by Jean-Marc Valin and Gregory Maxwell */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "celt.h"
#include "modes.h"
#include "rate.h"
#include "os_support.h"
#include "stack_alloc.h"
#include "quant_bands.h"

static const celt_int16 eband5ms[] = {
  0,  1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 20, 24, 28, 34, 40, 48, 60, 78, 100
};

#if 0

#define BITALLOC_SIZE 9
/* Bit allocation table in units of 1/32 bit/sample (0.1875 dB SNR) */
static const unsigned char band_allocation[] = {
/*0  200 400 600 800  1k 1.2 1.4 1.6  2k 2.4 2.8 3.2  4k 4.8 5.6 6.8  8k 9.6 12k 15.6 */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 95, 90, 80, 75, 65, 60, 55, 50, 44, 40, 35, 30, 15,  1,  0,  0,  0,  0,  0,  0,  0,
100, 95, 90, 88, 85, 82, 78, 74, 70, 65, 60, 54, 45, 35, 25, 15,  1,  0,  0,  0,  0,
120,110,110,110,100, 96, 90, 88, 84, 76, 70, 65, 60, 45, 35, 25, 20,  1,  1,  0,  0,
135,125,125,125,115,112,104,104,100, 96, 83, 78, 70, 55, 46, 36, 32, 28, 20,  8,  0,
170,165,157,155,149,145,143,138,138,138,129,124,108, 96, 88, 83, 72, 56, 44, 28,  2,
192,192,160,160,160,160,160,160,160,160,150,140,120,110,100, 90, 80, 70, 60, 50, 20,
224,224,192,192,192,192,192,192,192,160,160,160,160,160,160,160,160,120, 80, 64, 40,
255,255,224,224,224,224,224,224,224,192,192,192,192,192,192,192,192,192,192,192,120,
};

#else

/* Alternate tuning (partially derived from Vorbis) */
#define BITALLOC_SIZE 12
/* Bit allocation table in units of 1/32 bit/sample (0.1875 dB SNR) */
static const unsigned char band_allocation[] = {
/*0  200 400 600 800  1k 1.2 1.4 1.6  2k 2.4 2.8 3.2  4k 4.8 5.6 6.8  8k 9.6 12k 15.6 */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 90, 80, 75, 69, 63, 56, 49, 40, 34, 29, 20, 18, 10,  0,  0,  0,  0,  0,  0,  0,  0,
110,100, 90, 84, 78, 71, 65, 58, 51, 45, 39, 32, 26, 20, 12,  0,  0,  0,  0,  0,  0,
118,110,103, 93, 86, 80, 75, 70, 65, 59, 53, 47, 40, 31, 23, 15,  4,  0,  0,  0,  0,
126,119,112,104, 95, 89, 83, 78, 72, 66, 60, 54, 47, 39, 32, 25, 17, 12,  1,  0,  0,
134,127,120,114,103, 97, 91, 85, 78, 72, 66, 60, 54, 47, 41, 35, 29, 23, 16, 10,  1,
144,137,130,124,113,107,101, 95, 88, 82, 76, 70, 64, 57, 51, 45, 39, 33, 26, 15,  1,
152,145,138,132,123,117,111,105, 98, 92, 86, 80, 74, 67, 61, 55, 49, 43, 36, 20,  1,
162,155,148,142,133,127,121,115,108,102, 96, 90, 84, 77, 71, 65, 59, 53, 46, 30,  1,
172,165,158,152,143,137,131,125,118,112,106,100, 94, 87, 81, 75, 69, 63, 56, 45, 20,
200,200,200,200,200,200,200,200,198,193,188,183,178,173,168,163,158,153,148,143,120,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};
#endif

#ifdef STATIC_MODES
#ifdef FIXED_POINT
#include "static_modes_fixed.c"
#else
#include "static_modes_float.c"
#endif
#endif

#ifndef M_PI
#define M_PI 3.141592653
#endif


int celt_mode_info(const CELTMode *mode, int request, celt_int32 *value)
{
   switch (request)
   {
      case CELT_GET_LOOKAHEAD:
         *value = mode->overlap;
         break;
      case CELT_GET_BITSTREAM_VERSION:
         *value = CELT_BITSTREAM_VERSION;
         break;
      case CELT_GET_SAMPLE_RATE:
         *value = mode->Fs;
         break;
      default:
         return CELT_UNIMPLEMENTED;
   }
   return CELT_OK;
}

#ifndef STATIC_MODES

/* Defining 25 critical bands for the full 0-20 kHz audio bandwidth
   Taken from http://ccrma.stanford.edu/~jos/bbt/Bark_Frequency_Scale.html */
#define BARK_BANDS 25
static const celt_int16 bark_freq[BARK_BANDS+1] = {
      0,   100,   200,   300,   400,
    510,   630,   770,   920,  1080,
   1270,  1480,  1720,  2000,  2320,
   2700,  3150,  3700,  4400,  5300,
   6400,  7700,  9500, 12000, 15500,
  20000};

static celt_int16 *compute_ebands(celt_int32 Fs, int frame_size, int res, int *nbEBands)
{
   celt_int16 *eBands;
   int i, lin, low, high, nBark, offset=0;

   /* All modes that have 2.5 ms short blocks use the same definition */
   if (Fs == 400*(celt_int32)frame_size)
   {
      *nbEBands = sizeof(eband5ms)/sizeof(eband5ms[0])-1;
      eBands = celt_alloc(sizeof(celt_int16)*(*nbEBands+1));
      for (i=0;i<*nbEBands+1;i++)
         eBands[i] = eband5ms[i];
      return eBands;
   }
   /* Find the number of critical bands supported by our sampling rate */
   for (nBark=1;nBark<BARK_BANDS;nBark++)
    if (bark_freq[nBark+1]*2 >= Fs)
       break;

   /* Find where the linear part ends (i.e. where the spacing is more than min_width */
   for (lin=0;lin<nBark;lin++)
      if (bark_freq[lin+1]-bark_freq[lin] >= res)
         break;

   low = (bark_freq[lin]+res/2)/res;
   high = nBark-lin;
   *nbEBands = low+high;
   eBands = celt_alloc(sizeof(celt_int16)*(*nbEBands+2));
   
   if (eBands==NULL)
      return NULL;
   
   /* Linear spacing (min_width) */
   for (i=0;i<low;i++)
      eBands[i] = i;
   if (low>0)
      offset = eBands[low-1]*res - bark_freq[lin-1];
   /* Spacing follows critical bands */
   for (i=0;i<high;i++)
   {
      int target = bark_freq[lin+i];
      /* Round to an even value */
      eBands[i+low] = (target+offset/2+res)/(2*res)*2;
      offset = eBands[i+low]*res - target;
   }
   /* Enforce the minimum spacing at the boundary */
   for (i=0;i<*nbEBands;i++)
      if (eBands[i] < i)
         eBands[i] = i;
   /* Round to an even value */
   eBands[*nbEBands] = (bark_freq[nBark]+res)/(2*res)*2;
   if (eBands[*nbEBands] > frame_size)
      eBands[*nbEBands] = frame_size;
   for (i=1;i<*nbEBands-1;i++)
   {
      if (eBands[i+1]-eBands[i] < eBands[i]-eBands[i-1])
      {
         eBands[i] -= (2*eBands[i]-eBands[i-1]-eBands[i+1])/2;
      }
   }
   /*for (i=0;i<=*nbEBands+1;i++)
      printf ("%d ", eBands[i]);
   printf ("\n");
   exit(1);*/
   /* FIXME: Remove last band if too small */
   return eBands;
}

static void compute_allocation_table(CELTMode *mode)
{
   int i, j;
   unsigned char *allocVectors;
   int maxBands = sizeof(eband5ms)/sizeof(eband5ms[0])-1;

   mode->nbAllocVectors = BITALLOC_SIZE;
   allocVectors = celt_alloc(sizeof(unsigned char)*(BITALLOC_SIZE*mode->nbEBands));
   if (allocVectors==NULL)
      return;

   /* Check for standard mode */
   if (mode->Fs == 400*(celt_int32)mode->shortMdctSize)
   {
      for (i=0;i<BITALLOC_SIZE*mode->nbEBands;i++)
         allocVectors[i] = band_allocation[i];
      mode->allocVectors = allocVectors;
      return;
   }
   /* If not the standard mode, interpolate */
   /* Compute per-codec-band allocation from per-critical-band matrix */
   for (i=0;i<BITALLOC_SIZE;i++)
   {
      for (j=0;j<mode->nbEBands;j++)
      {
         int k;
         for (k=0;k<maxBands;k++)
         {
            if (400*(celt_int32)eband5ms[k] > mode->eBands[j]*(celt_int32)mode->Fs/mode->shortMdctSize)
               break;
         }
         if (k>mode->nbEBands-1)
            allocVectors[i*mode->nbEBands+j] = band_allocation[i*maxBands + maxBands-1];
         else {
            celt_int32 a0, a1;
            a1 = mode->eBands[j]*(celt_int32)mode->Fs/mode->shortMdctSize - 400*(celt_int32)eband5ms[k-1];
            a0 = 400*(celt_int32)eband5ms[k] - mode->eBands[j]*(celt_int32)mode->Fs/mode->shortMdctSize;
            allocVectors[i*mode->nbEBands+j] = (a0*band_allocation[i*maxBands+k-1]
                                             + a1*band_allocation[i*maxBands+k])/(a0+a1);
         }
      }
   }

   /*printf ("\n");
   for (i=0;i<BITALLOC_SIZE;i++)
   {
      for (j=0;j<mode->nbEBands;j++)
         printf ("%d ", allocVectors[i*mode->nbEBands+j]);
      printf ("\n");
   }
   exit(0);*/

   mode->allocVectors = allocVectors;
}

#endif /* STATIC_MODES */

CELTMode *celt_mode_create(celt_int32 Fs, int frame_size, int *error)
{
   int i;
#ifdef STATIC_MODES
   for (i=0;i<TOTAL_MODES;i++)
   {
      if (Fs == static_mode_list[i]->Fs &&
          frame_size == static_mode_list[i]->shortMdctSize*static_mode_list[i]->nbShortMdcts)
      {
         return (CELTMode*)static_mode_list[i];
      }
   }
   if (error)
      *error = CELT_BAD_ARG;
   return NULL;
#else
   int res;
   CELTMode *mode=NULL;
   celt_word16 *window;
   celt_int16 *logN;
   int LM;
#ifdef STDIN_TUNING
   scanf("%d ", &MIN_BINS);
   scanf("%d ", &BITALLOC_SIZE);
   band_allocation = celt_alloc(sizeof(int)*BARK_BANDS*BITALLOC_SIZE);
   for (i=0;i<BARK_BANDS*BITALLOC_SIZE;i++)
   {
      scanf("%d ", band_allocation+i);
   }
#endif
   ALLOC_STACK;
#if !defined(VAR_ARRAYS) && !defined(USE_ALLOCA)
   if (global_stack==NULL)
      goto failure;
#endif 

   /* The good thing here is that permutation of the arguments will automatically be invalid */
   
   if (Fs < 8000 || Fs > 96000)
   {
      if (error)
         *error = CELT_BAD_ARG;
      return NULL;
   }
   if (frame_size < 40 || frame_size > 1024 || frame_size%2!=0)
   {
      if (error)
         *error = CELT_BAD_ARG;
      return NULL;
   }
   
   mode = celt_alloc(sizeof(CELTMode));
   if (mode==NULL)
      goto failure;
   mode->Fs = Fs;

   /* Pre/de-emphasis depends on sampling rate. The "standard" pre-emphasis
      is defined as A(z) = 1 - 0.85*z^-1 at 48 kHz. Other rates should
      approximate that. */
   if(Fs < 12000) /* 8 kHz */
   {
      mode->preemph[0] =  QCONST16(0.3500061035f, 15);
      mode->preemph[1] = -QCONST16(0.1799926758f, 15);
      mode->preemph[2] =  QCONST16(0.2719726562f, SIG_SHIFT);
      mode->preemph[3] =  QCONST16(3.6765136719f, 13);
   } else if(Fs < 24000) /* 16 kHz */
   {
      mode->preemph[0] =  QCONST16(0.6000061035f, 15);
      mode->preemph[1] = -QCONST16(0.1799926758f, 15);
      mode->preemph[2] =  QCONST16(0.4425048828f, SIG_SHIFT);
      mode->preemph[3] =  QCONST16(2.259887f, 13);
   } else if(Fs < 40000) /* 32 kHz */
   {
      mode->preemph[0] =  QCONST16(0.7799987793f, 15);
      mode->preemph[1] = -QCONST16(0.1000061035f, 15);
      mode->preemph[2] =  QCONST16(.75f, SIG_SHIFT);
      mode->preemph[3] =  QCONST16(1.3333740234f, 13);
   } else /* 48 kHz */
   {
      mode->preemph[0] =  QCONST16(0.8500061035f, 15);
      mode->preemph[1] =  QCONST16(0.0f, 15);
      mode->preemph[2] =  QCONST16(1.f, SIG_SHIFT);
      mode->preemph[3] =  QCONST16(1.f, 13);
   }

   if ((celt_int32)frame_size*75 >= Fs && (frame_size%16)==0)
   {
     LM = 3;
   } else if ((celt_int32)frame_size*150 >= Fs && (frame_size%8)==0)
   {
     LM = 2;
   } else if ((celt_int32)frame_size*300 >= Fs && (frame_size%4)==0)
   {
     LM = 1;
   } else
   {
     LM = 0;
   }

   mode->maxLM = LM;
   mode->nbShortMdcts = 1<<LM;
   mode->shortMdctSize = frame_size/mode->nbShortMdcts;
   res = (mode->Fs+mode->shortMdctSize)/(2*mode->shortMdctSize);

   mode->eBands = compute_ebands(Fs, mode->shortMdctSize, res, &mode->nbEBands);
   if (mode->eBands==NULL)
      goto failure;

   mode->effEBands = mode->nbEBands;
   while (mode->eBands[mode->effEBands] > mode->shortMdctSize)
      mode->effEBands--;
   
   /* Overlap must be divisible by 4 */
   mode->overlap = ((mode->shortMdctSize>>2)<<2);

   compute_allocation_table(mode);
   if (mode->allocVectors==NULL)
      goto failure;
   
   window = (celt_word16*)celt_alloc(mode->overlap*sizeof(celt_word16));
   if (window==NULL)
      goto failure;

#ifndef FIXED_POINT
   for (i=0;i<mode->overlap;i++)
      window[i] = Q15ONE*sin(.5*M_PI* sin(.5*M_PI*(i+.5)/mode->overlap) * sin(.5*M_PI*(i+.5)/mode->overlap));
#else
   for (i=0;i<mode->overlap;i++)
      window[i] = MIN32(32767,floor(.5+32768.*sin(.5*M_PI* sin(.5*M_PI*(i+.5)/mode->overlap) * sin(.5*M_PI*(i+.5)/mode->overlap))));
#endif
   mode->window = window;

   logN = (celt_int16*)celt_alloc(mode->nbEBands*sizeof(celt_int16));
   if (logN==NULL)
      goto failure;

   for (i=0;i<mode->nbEBands;i++)
      logN[i] = log2_frac(mode->eBands[i+1]-mode->eBands[i], BITRES);
   mode->logN = logN;

   compute_pulse_cache(mode, mode->maxLM);

   clt_mdct_init(&mode->mdct, 2*mode->shortMdctSize*mode->nbShortMdcts, mode->maxLM);
   if ((mode->mdct.trig==NULL)
#ifndef ENABLE_TI_DSPLIB55
         || (mode->mdct.kfft==NULL)
#endif
   )
      goto failure;

   if (error)
      *error = CELT_OK;

   return mode;
failure: 
   if (error)
      *error = CELT_INVALID_MODE;
   if (mode!=NULL)
      celt_mode_destroy(mode);
   return NULL;
#endif /* !STATIC_MODES */
}

void celt_mode_destroy(CELTMode *mode)
{
#ifndef STATIC_MODES
   if (mode == NULL)
      return;

   celt_free((celt_int16*)mode->eBands);
   celt_free((celt_int16*)mode->allocVectors);
   
   celt_free((celt_word16*)mode->window);
   celt_free((celt_int16*)mode->logN);

   celt_free((celt_int16*)mode->cache.index);
   celt_free((unsigned char*)mode->cache.bits);
   clt_mdct_clear(&mode->mdct);

   celt_free((CELTMode *)mode);
#endif
}
