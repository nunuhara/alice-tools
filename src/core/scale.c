/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include "math.h"
#include "system4.h"
#include "system4/cg.h"
#include "alice.h"

/* Bicubic interpolation implementation adapted from:
 * https://blog.demofox.org/2015/08/15/resizing-images-with-bicubic-interpolation/
 *
 * Copyright 2019 Alan Wolfe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#define CLAMP(v, min, max) if (v < min) { v = min; } else if (v > max) { v = max; }

// t is a value that goes from 0 to 1 to interpolate in a C1 continuous way
// across uniformly sampled data points. When t is 0, this will return B. When
// t is 1, this will return C. Inbetween values will return an interpolation
// between B and C. A and B are used to calculate slopes at the edges.
static float cubic_hermite(float A, float B, float C, float D, float t)
{
	float a = -A / 2.0f + (3.0f*B) / 2.0f - (3.0f*C) / 2.0f + D / 2.0f;
	float b = A - (5.0f*B) / 2.0f + 2.0f*C - D / 2.0f;
	float c = -A / 2.0f + C / 2.0f;
	float d = B;

	return a*t*t*t + b*t*t + c*t + d;
}

static const uint8_t *get_pixel_clamped(const struct cg *in, int x, int y)
{
	CLAMP(x, 0, in->metrics.w - 1);
	CLAMP(y, 0, in->metrics.h - 1);
	return &((uint8_t*)in->pixels)[(y * in->metrics.w * 4) + (x * 4)];
}

static void sample_bicubic(struct cg *in, uint8_t *out, float u, float v)
{
	// calculate coordinates -> also need to offset by half a pixel to keep
	// image from shifting down and left half a pixel
	float x = (u * in->metrics.w) - 0.5f;
	int xint = x;
	float xfract = x - floorf(x);

	float y = (v * in->metrics.h) - 0.5f;
	int yint = y;
	float yfract = y - floorf(y);

	// 1st row
	const uint8_t *p00 = get_pixel_clamped(in, xint - 1, yint - 1);
	const uint8_t *p10 = get_pixel_clamped(in, xint + 0, yint - 1);
	const uint8_t *p20 = get_pixel_clamped(in, xint + 1, yint - 1);
	const uint8_t *p30 = get_pixel_clamped(in, xint + 2, yint - 1);

	// 2nd row
	const uint8_t *p01 = get_pixel_clamped(in, xint - 1, yint + 0);
	const uint8_t *p11 = get_pixel_clamped(in, xint + 0, yint + 0);
	const uint8_t *p21 = get_pixel_clamped(in, xint + 1, yint + 0);
	const uint8_t *p31 = get_pixel_clamped(in, xint + 2, yint + 0);

	// 3rd row
	const uint8_t *p02 = get_pixel_clamped(in, xint - 1, yint + 1);
	const uint8_t *p12 = get_pixel_clamped(in, xint + 0, yint + 1);
	const uint8_t *p22 = get_pixel_clamped(in, xint + 1, yint + 1);
	const uint8_t *p32 = get_pixel_clamped(in, xint + 2, yint + 1);

	// 4th row
	const uint8_t *p03 = get_pixel_clamped(in, xint - 1, yint + 2);
	const uint8_t *p13 = get_pixel_clamped(in, xint + 0, yint + 2);
	const uint8_t *p23 = get_pixel_clamped(in, xint + 1, yint + 2);
	const uint8_t *p33 = get_pixel_clamped(in, xint + 2, yint + 2);

	// interpolate bi-cubically
	// clamp the values since the curve can put the value below 0 or above 255
	for (int i = 0; i <  4; i++) {
		float col0 = cubic_hermite(p00[i], p10[i], p20[i], p30[i], xfract);
		float col1 = cubic_hermite(p01[i], p11[i], p21[i], p31[i], xfract);
		float col2 = cubic_hermite(p02[i], p12[i], p22[i], p32[i], xfract);
		float col3 = cubic_hermite(p03[i], p13[i], p23[i], p33[i], xfract);
		float value = cubic_hermite(col0, col1, col2, col3, yfract);
		CLAMP(value, 0.0f, 255.0f);
		out[i] = value;
	}
}

struct cg *scale_cg_bicubic(struct cg *in, float scale)
{
	struct cg *out = xmalloc(sizeof(struct cg));
	*out = *in;
	out->metrics.w = in->metrics.w * scale;
	out->metrics.h = in->metrics.h * scale;
	// FIXME: pitch?
	out->pixels = xmalloc(out->metrics.w * out->metrics.h * 4);

	uint8_t *row = out->pixels;
	for (int y = 0; y < out->metrics.h; y++) {
		uint8_t *pixel = row;
		float v = (float)y / (float)(out->metrics.h - 1);
		for (int x = 0; x < out->metrics.w; x++) {
			float u = (float)x / (float)(out->metrics.w - 1);
			sample_bicubic(in, pixel, u, v);
			pixel += 4;
		}
		row += out->metrics.w * 4;
	}

	return out;
}
