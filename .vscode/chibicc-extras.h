#ifndef CHIBICC_EXTRAS_H
#define CHIBICC_EXTRAS_H

#include <stdlib.h>

#ifndef Boolean
#define Boolean _Bool
#endif

#ifndef True
#define True 1
#endif

#ifndef False
#define False 0
#endif

#ifndef Maybe
#define Maybe ((rand() & 1) ? 1 : 0)
#endif

int cprintf(const char *fmt, ...);
int dinput(const char *prompt, void *target);

#endif