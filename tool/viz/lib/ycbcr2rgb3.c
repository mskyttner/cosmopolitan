/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ This program is free software; you can redistribute it and/or modify         │
│ it under the terms of the GNU General Public License as published by         │
│ the Free Software Foundation; version 2 of the License.                      │
│                                                                              │
│ This program is distributed in the hope that it will be useful, but          │
│ WITHOUT ANY WARRANTY; without even the implied warranty of                   │
│ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU             │
│ General Public License for more details.                                     │
│                                                                              │
│ You should have received a copy of the GNU General Public License            │
│ along with this program; if not, write to the Free Software                  │
│ Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA                │
│ 02110-1301 USA                                                               │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "dsp/core/c11.h"
#include "dsp/core/c1331.h"
#include "dsp/core/c1331s.h"
#include "dsp/core/c161.h"
#include "dsp/core/core.h"
#include "dsp/core/half.h"
#include "dsp/core/illumination.h"
#include "dsp/core/q.h"
#include "dsp/scale/scale.h"
#include "libc/bits/xmmintrin.h"
#include "libc/calls/calls.h"
#include "libc/calls/sigbits.h"
#include "libc/calls/struct/sigset.h"
#include "libc/intrin/pmulhrsw.h"
#include "libc/log/check.h"
#include "libc/log/log.h"
#include "libc/macros.h"
#include "libc/math.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/gc.h"
#include "libc/nexgen32e/nexgen32e.h"
#include "libc/nexgen32e/x86feature.h"
#include "libc/runtime/gc.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/errfuns.h"
#include "libc/time/time.h"
#include "libc/x/x.h"
#include "third_party/avir/lanczos.h"
#include "tool/viz/lib/graphic.h"
#include "tool/viz/lib/knobs.h"
#include "tool/viz/lib/scale.h"
#include "tool/viz/lib/ycbcr.h"

#define M        15
#define CLAMP(X) MIN(255, MAX(0, X))

const double kBt601Primaries[] = {.299, .587, .114};
const double kBt709Primaries[] = {871024 / 4096299., 8788810 / 12288897.,
                                  887015 / 12288897.};

const double kSrgbToXyz[3][3] = {
    {506752 / 1228815., 87881 / 245763., 12673 / 70218.},
    {87098 / 409605., 175762 / 245763., 12673 / 175545.},
    {7918 / 409605., 87881 / 737289., 1001167 / 1053270.},
};

long magikarp_latency_;
long gyarados_latency_;
long ycbcr2rgb_latency_;
long double magikarp_start_;

struct YCbCr {
  bool yonly;
  int magnums[8][4];
  int lighting[6][4];
  unsigned char transfer[2][256];
  struct YCbCrSamplingSolution {
    long dyn, dxn;
    long syn, sxn;
    double ry, rx;
    double oy, ox;
    double py, px;
    struct SamplingSolution *cy, *cx;
  } luma, chroma;
};

/**
 * Computes magnums for Y′CbCr decoding.
 *
 * @param swing should be 219 for TV, or 255 for JPEG
 * @param M is integer coefficient bits
 */
void YCbCrComputeCoefficients(int swing, double gamma,
                              const double primaries[3],
                              const double illuminant[3], int out_magnums[8][4],
                              int out_lighting[6][4],
                              unsigned char out_transfer[256]) {
  int i, j;
  double x;
  double f1[6][3];
  long longs[6][6];
  long bitlimit = roundup2pow(swing);
  long wordoffset = rounddown2pow((bitlimit - swing) / 2);
  long chromaswing = swing + 2 * (bitlimit / 2. - swing / 2. - wordoffset);
  long lumamin = wordoffset;
  long lumamax = wordoffset + swing;
  long diffmax = wordoffset + chromaswing - bitlimit / 2;
  long diffmin = -diffmax;
  double rEb = 1 - primaries[2] + primaries[0] + primaries[1];
  double rEgEb = 1 / primaries[1] * primaries[2] * rEb;
  double rEr = 1 - primaries[0] + primaries[1] + primaries[2];
  double rEgEr = 1 / primaries[1] * primaries[0] * rEr;
  double unswing = 1. / swing * bitlimit;
  double digital = 1. / swing * chromaswing;
  double reals[6][6] = {
      {rEr / digital},
      {-rEgEb / digital, -rEgEr / digital},
      {rEb / digital},
      {0, 0, unswing},
  };
  for (i = 0; i < 4; ++i) {
    GetIntegerCoefficients(longs[i], reals[i], M, diffmin, diffmax);
  }
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) {
      out_magnums[i][j] = longs[i][j];
    }
  }
  out_magnums[3][0] = wordoffset;
  out_magnums[3][1] = bitlimit / 2;
  GetChromaticAdaptationMatrix(f1, kIlluminantD65, illuminant);
  for (i = 0; i < 3; ++i) {
    for (j = 0; j < 3; ++j) {
      reals[i][j] = f1[i][j];
    }
  }
  for (i = 0; i < 6; ++i) {
    GetIntegerCoefficients(longs[i], reals[i], M, diffmin * 2, lumamax * 2);
  }
  for (i = 0; i < 6; ++i) {
    for (j = 0; j < 3; ++j) {
      out_lighting[i][j] = longs[i][j];
    }
  }
  for (i = 0; i < 256; ++i) {
    x = i;
    x /= 255;
    x = tv2pcgamma(x, gamma);
    x *= 255;
    out_transfer[i] = CLAMP(x);
  }
  memset(out_transfer, out_transfer[lumamin], lumamin);
  memset(out_transfer + lumamax + 1, out_transfer[lumamax], bitlimit - lumamax);
}

void YCbCrInit(struct YCbCr **ycbcr, bool yonly, int swing, double gamma,
               const double gamut[3], const double illuminant[3]) {
  if (!*ycbcr) *ycbcr = xcalloc(1, sizeof(struct YCbCr));
  (*ycbcr)->yonly = yonly;
  memset((*ycbcr)->magnums, 0, sizeof((*ycbcr)->magnums));
  memset((*ycbcr)->lighting, 0, sizeof((*ycbcr)->lighting));
  YCbCrComputeCoefficients(swing, gamma, gamut, illuminant, (*ycbcr)->magnums,
                           (*ycbcr)->lighting, (*ycbcr)->transfer[0]);
  imapxlatab((*ycbcr)->transfer[1]);
}

void YCbCrFree(struct YCbCr **ycbcr) {
  if (*ycbcr) {
    FreeSamplingSolution((*ycbcr)->luma.cy), (*ycbcr)->luma.cy = NULL;
    FreeSamplingSolution((*ycbcr)->luma.cx), (*ycbcr)->luma.cx = NULL;
    FreeSamplingSolution((*ycbcr)->chroma.cy), (*ycbcr)->chroma.cy = NULL;
    FreeSamplingSolution((*ycbcr)->chroma.cx), (*ycbcr)->chroma.cx = NULL;
    free(*ycbcr), *ycbcr = NULL;
  }
}

void *YCbCrReallocPlane(long ys, long xs, const unsigned char p[ys][xs],
                        long yn, long xn) {
  long y;
  unsigned char(*res)[yn][xn];
  res = xmemalign(32, yn * xn);
  for (y = 0; y < yn; ++y) {
    memcpy((*res)[y], p[y], xn);
  }
  return res;
}

void YCbCrComputeSamplingSolution(struct YCbCrSamplingSolution *scale, long dyn,
                                  long dxn, long syn, long sxn, double ry,
                                  double rx, double oy, double ox, double py,
                                  double px) {
  if (scale->dyn != dyn || scale->dxn != dxn || scale->syn != syn ||
      scale->sxn != sxn || fabs(scale->ry - ry) > .001 ||
      fabs(scale->rx - rx) > .001 || fabs(scale->oy - oy) > .001 ||
      fabs(scale->ox - ox) > .001 || fabs(scale->py - py) > .001 ||
      fabs(scale->px - px) > .001) {
    LOGF("recomputing sampling solution");
    FreeSamplingSolution(scale->cy), scale->cy = NULL;
    FreeSamplingSolution(scale->cx), scale->cx = NULL;
    scale->cy = ComputeSamplingSolution(dyn, syn, ry, oy, py);
    scale->cx = ComputeSamplingSolution(dxn, sxn, rx, ox, px);
    scale->dyn = dyn, scale->dxn = dxn;
    scale->syn = syn, scale->sxn = sxn;
    scale->ry = ry, scale->rx = rx;
    scale->oy = oy, scale->ox = ox;
    scale->py = py, scale->px = px;
  }
}

void Y2Rgb(long yn, long xn, unsigned char RGB[restrict 3][yn][xn], long yys,
           long yxs, const unsigned char Y[restrict yys][yxs],
           const int K[8][4], const unsigned char T[256]) {
  long i, j;
  for (i = 0; i < yn; ++i) {
    for (j = 0; j < xn; ++j) {
      RGB[0][i][j] = T[Y[i][j]];
    }
  }
  memcpy(RGB[1], RGB[0], yn * xn);
  memcpy(RGB[2], RGB[0], yn * xn);
}

/**
 * Converts Y′CbCr samples to RGB.
 */
void YCbCr2Rgb(long yn, long xn, unsigned char RGB[restrict 3][yn][xn],
               long yys, long yxs, const unsigned char Y[restrict yys][yxs],
               long cys, long cxs, const unsigned char Cb[restrict cys][cxs],
               const unsigned char Cr[restrict cys][cxs], const int K[8][4],
               const int L[6][4], const unsigned char T[256]) {
  long i, j;
  short y, u, v, r, g, b, A, B, C;
  for (i = 0; i < yn; ++i) {
    for (j = 0; j < xn; ++j) {
      y = T[Y[i][j]];
      u = Cb[i][j] - K[3][1];
      v = Cr[i][j] - K[3][1];
      r = y + QRS(M, v * K[0][0]);
      g = y + QRS(M, u * K[1][0] + v * K[1][1]);
      b = y + QRS(M, u * K[2][0]);
      r = QRS(M, (MIN(235, MAX(16, r)) - K[3][0]) * K[3][2]);
      g = QRS(M, (MIN(235, MAX(16, g)) - K[3][0]) * K[3][2]);
      b = QRS(M, (MIN(235, MAX(16, b)) - K[3][0]) * K[3][2]);
      RGB[0][i][j] = CLAMP(QRS(M, r * L[0][0] + g * L[0][1] + b * L[0][2]));
      RGB[1][i][j] = CLAMP(QRS(M, r * L[1][0] + g * L[1][1] + b * L[1][2]));
      RGB[2][i][j] = CLAMP(QRS(M, r * L[2][0] + g * L[2][1] + b * L[2][2]));
    }
  }
}

void YCbCrConvert(struct YCbCr *me, long yn, long xn,
                  unsigned char RGB[restrict 3][yn][xn], long yys, long yxs,
                  const unsigned char Y[restrict yys][yxs], long cys, long cxs,
                  unsigned char Cb[restrict cys][cxs],
                  unsigned char Cr[restrict cys][cxs]) {
  long double ts;
  ts = nowl();
  if (!me->yonly) {
    YCbCr2Rgb(yn, xn, RGB, yys, yxs, Y, cys, cxs, Cb, Cr, me->magnums,
              me->lighting, me->transfer[pf10_]);
  } else {
    Y2Rgb(yn, xn, RGB, yys, yxs, Y, me->magnums, me->transfer[pf10_]);
  }
  ycbcr2rgb_latency_ = lroundl((nowl() - ts) * 1e6l);
}

void YCbCr2RgbScaler(struct YCbCr *me, long dyn, long dxn,
                     unsigned char RGB[restrict 3][dyn][dxn], long yys,
                     long yxs, unsigned char Y[restrict yys][yxs], long cys,
                     long cxs, unsigned char Cb[restrict cys][cxs],
                     unsigned char Cr[restrict cys][cxs], long yyn, long yxn,
                     long cyn, long cxn, double syn, double sxn, double pry,
                     double prx) {
  long double ts;
  long y, x, scyn, scxn;
  double yry, yrx, cry, crx, yoy, yox, coy, cox, err, oy;
  scyn = syn * cyn / yyn;
  scxn = sxn * cxn / yxn;
  if (HALF(yxn) > dxn && HALF(scxn) > dxn) {
    YCbCr2RgbScaler(me, dyn, dxn, RGB, yys, yxs,
                    Magikarp2xX(yys, yxs, Y, syn, sxn), cys, cxs,
                    Magkern2xX(cys, cxs, Cb, scyn, scxn),
                    Magkern2xX(cys, cxs, Cr, scyn, scxn), yyn, HALF(yxn), cyn,
                    HALF(cxn), syn, sxn / 2, pry, prx);
  } else if (HALF(yyn) > dyn && HALF(scyn) > dyn) {
    YCbCr2RgbScaler(me, dyn, dxn, RGB, yys, yxs,
                    Magikarp2xY(yys, yxs, Y, syn, sxn), cys, cxs,
                    Magkern2xY(cys, cxs, Cb, scyn, scxn),
                    Magkern2xY(cys, cxs, Cr, scyn, scxn), HALF(yyn), yxn,
                    HALF(cyn), scxn, syn / 2, sxn, pry, prx);
  } else {
    magikarp_latency_ = lroundl((nowl() - magikarp_start_) * 1e6l);
    ts = nowl();
    yry = syn / dyn;
    yrx = sxn / dxn;
    cry = syn * cyn / yyn / dyn;
    crx = sxn * cxn / yxn / dxn;
    yoy = syn / scyn / 2 - pry * .5;
    yox = sxn / scxn / 2 - prx * .5;
    coy = syn / scyn / 2 - pry * .5;
    cox = sxn / scxn / 2 - prx * .5;
    LOGF("gyarados pry=%.3f prx=%.3f syn=%.3f sxn=%.3f dyn=%ld dxn=%ld "
         "yyn=%ld "
         "yxn=%ld cyn=%ld cxn=%ld yry=%.3f yrx=%.3f cry=%.3f crx=%.3f "
         "yoy=%.3f "
         "yox=%.3f coy=%.3f cox=%.3f",
         pry, prx, syn, sxn, dyn, dxn, yyn, yxn, cyn, cxn, yry, yrx, cry, crx,
         yoy, yox, coy, cox);
    YCbCrComputeSamplingSolution(&me->luma, dyn, dxn, syn, sxn, yry, yrx, yoy,
                                 yox, pry, prx);
    YCbCrComputeSamplingSolution(&me->chroma, dyn, dxn, scyn, scxn, cry, crx,
                                 coy, cox, pry, prx);
    if (pf8_) sharpen(1, yys, yxs, (void *)Y, yyn, yxn);
    if (pf9_) unsharp(1, yys, yxs, (void *)Y, yyn, yxn);
    GyaradosUint8(yys, yxs, Y, yys, yxs, Y, dyn, dxn, syn, sxn, 0, 255,
                  me->luma.cy, me->luma.cx, true);
    GyaradosUint8(cys, cxs, Cb, cys, cxs, Cb, dyn, dxn, scyn, scxn, 0, 255,
                  me->chroma.cy, me->chroma.cx, false);
    GyaradosUint8(cys, cxs, Cr, cys, cxs, Cr, dyn, dxn, scyn, scxn, 0, 255,
                  me->chroma.cy, me->chroma.cx, false);
    gyarados_latency_ = lround((nowl() - ts) * 1e6l);
    YCbCrConvert(me, dyn, dxn, RGB, yys, yxs, Y, cys, cxs, Cb, Cr);
    LOGF("done");
  }
}

/**
 * Converts Y′CbCr frame for PC display.
 *
 *    "[The] experiments of Professor J. D. Forbes, which I
 *     witnessed… [established] that blue and yellow do not
 *     make green but a pinkish tint, when neither prevails
 *     in the combination [and the] result of mixing yellow
 *     and blue was, I believe, not previously known.
 *                                 — James Clerk Maxwell
 *
 * This function converts TV to PC graphics. We do that by
 *
 *   1. decimating w/ facebook magikarp photoshop cubic sharpen
 *   2. upsampling color difference planes, to be as big as luma plane
 *   3. converting color format
 *   4. expanding dynamic range
 *   5. transferring gamma from TV to PC convention
 *   6. resampling again to exact requested display / pixel geometry
 *
 * @param dyn/dxn is display height/width after scaling/conversion
 * @param RGB points to memory for packed de-interlaced RGB output
 * @param Y′ ∈ [16,235] is the luminance plane a gamma-corrected RGB
 *     weighted sum; a.k.a. black/white legacy component part of the
 *     TV signal; which may be used independently of the chrominance
 *     planes; and decodes to the range [0,1]
 * @param Cb/Cr ∈ [16,240] is blue/red chrominance difference planes
 *     which (if sampled at a different rate) will get stretched out
 *     over the luma plane appropriately
 * @param yys/yxs dimensions luma sample array
 * @param cys/cxs dimensions chroma sample arrays
 * @param yyn/yxn is number of samples in luma signal
 * @param cyn/cxn is number of samples in each chroma signal
 * @param syn/sxn is size of source signal
 * @param pry/prx is pixel aspect ratio, e.g. 1,1
 * @return RGB
 */
void *YCbCr2RgbScale(long dyn, long dxn,
                     unsigned char RGB[restrict 3][dyn][dxn], long yys,
                     long yxs, unsigned char Y[restrict yys][yxs], long cys,
                     long cxs, unsigned char Cb[restrict cys][cxs],
                     unsigned char Cr[restrict cys][cxs], long yyn, long yxn,
                     long cyn, long cxn, double syn, double sxn, double pry,
                     double prx, struct YCbCr **ycbcr) {
  long minyys, minyxs, mincys, mincxs;
  CHECK_LE(yyn, yys);
  CHECK_LE(yxn, yxs);
  CHECK_LE(cyn, cys);
  CHECK_LE(cxn, cxs);
  LOGF("magikarp2x");
  magikarp_start_ = nowl();
  minyys = MAX(ceil(syn), MAX(yyn, ceil(dyn * pry)));
  minyxs = MAX(ceil(sxn), MAX(yxn, ceil(dxn * prx)));
  mincys = MAX(cyn, ceil(dyn * pry));
  mincxs = MAX(cxn, ceil(dxn * prx));
  YCbCr2RgbScaler(*ycbcr, dyn, dxn, RGB, MAX(yys, minyys), MAX(yxs, minyxs),
                  (yys >= minyys && yxs >= minyxs
                       ? Y
                       : gc(YCbCrReallocPlane(yys, yxs, Y, minyys, minyxs))),
                  MAX(cys, mincys), MAX(cxs, mincxs),
                  (cys >= mincys && cxs >= mincxs
                       ? Cb
                       : gc(YCbCrReallocPlane(cys, cxs, Cb, mincys, mincxs))),
                  (cys >= mincys && cxs >= mincxs
                       ? Cr
                       : gc(YCbCrReallocPlane(cys, cxs, Cr, mincys, mincxs))),
                  yyn, yxn, cyn, cxn, syn, sxn, pry, prx);
  return RGB;
}