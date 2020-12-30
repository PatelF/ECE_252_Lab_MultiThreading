#pragma once

#include <stdio.h>  /* for printf().  man 3 printf */
#include <stdlib.h> /* for exit().    man 3 exit   */
#include <string.h> /* for strcat().  man strcat   */
#include <errno.h>  /* for errno                   */

#include "zutil.h"
#include "crc.h"     /* for crc()                   */
#include "lab_png.h" /* simple PNG data structures  */

/* FUNCTION PROTOTYPES */
int powerFunction(int x, int y);
int hexToDec(unsigned char *hexVal, int hexLen);
int getDimensions(unsigned char *IHDR);

simple_PNG_p changeHeight(simple_PNG_p png1, simple_PNG_p png2);
simple_PNG_p inflatePNGS(simple_PNG_p png);
simple_PNG_p deflatePNGS(simple_PNG_p png);
simple_PNG_p combinePNGS(simple_PNG_p pngs[], int numPNGS);
simple_PNG_p getPNGInfo(U8* buffer);
