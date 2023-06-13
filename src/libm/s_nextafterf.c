#include "SDL_internal.h"
/* s_nextafterf.c -- float version of s_nextafter.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include "math.h"
#include "math_private.h"

float nextafterf(float x, float y)
{
	int32_t hx, hy, ix, iy;

	GET_FLOAT_WORD(hx, x);
	GET_FLOAT_WORD(hy, y);
	ix = hx & 0x7fffffff;		/* |x| */
	iy = hy & 0x7fffffff;		/* |y| */

	/* x is nan or y is nan? */
	if ((ix > 0x7f800000) || (iy > 0x7f800000))
		return x + y;

	if (x == y)
		return y;

	if (ix == 0) { /* x == 0? */
/* glibc 2.4 does not seem to set underflow? */
/*		float u; */
		/* return +-minsubnormal */
		SET_FLOAT_WORD(x, (hy & 0x80000000) | 1);
/*		u = x * x;     raise underflow flag */
/*		math_force_eval(u); */
		return x;
	}

	if (hx >= 0) { /* x > 0 */
		if (hx > hy) { /* x > y: x -= ulp */
			hx -= 1;
		} else { /* x < y: x += ulp */
			hx += 1;
		}
	} else { /* x < 0 */
		if (hy >= 0 || hx > hy) { /* x < y: x -= ulp */
			hx -= 1;
		} else { /* x > y: x += ulp */
			hx += 1;
		}
	}
	hy = hx & 0x7f800000;
	if (hy >= 0x7f800000) {
		x = x + x; /* overflow */
		return x; /* overflow */
	}
	if (hy < 0x00800000) {
		float u = x * x; /* underflow */
		math_force_eval(u); /* raise underflow flag */
	}
	SET_FLOAT_WORD(x, hx);
	return x;
}

#if 0
/* "testprog N a b"
 * calculates a = nextafterf(a, b) and prints a as float
 * and as raw bytes; repeats it N times.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
int main(int argc, char **argv)
{
        int cnt, i;
        float a, b;
        cnt = atoi(argv[1]);
        a = strtod(argv[2], NULL);
        b = strtod(argv[3], NULL);
        while (cnt-- > 0) {
                for (i = 0; i < sizeof(a); i++) {
                        unsigned char c = ((char*)(&a))[i];
                        printf("%x%x", (c >> 4), (c & 0xf));
                }
                printf(" %f\n", a);
                a = nextafterf(a, b);
        }
        return 0;
}
#endif
