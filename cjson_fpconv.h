#pragma once
/* Lua CJSON floating point conversion routines */

/* Buffer required to store the largest string representation of a double.
 *
 * Longest double printed with %.14g is 21 characters long:
 * -1.7976931348623e+308 */
# define FPCONV_G_FMT_BUFSIZE   32

void fpconv_init();
int fpconv_g_fmt(char*, double, int);
double fpconv_strtod(const char*, char**);

