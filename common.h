/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _COMMON_H_
#define _COMMON_H_

//#define DEBUG

#ifdef DEBUG
#include <stdio.h>
#include "strnstr.h"
/*#define return(X) { fprintf(stderr, "DEBUG Retorno en: %s, %d es %d\n", __FILE__, __LINE__, X); return (X); } 0*/

#define kill(X, Y) { fprintf(stderr, "DEBUG Killed process in: %s, %d is %d\n", __FILE__, __LINE__, X); kill((X), (Y)); } 0
#endif

typedef unsigned short PORT;

#endif
