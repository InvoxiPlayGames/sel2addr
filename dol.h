#ifndef _DOL_H
#define _DOL_H

#define DOL_TEXT_SECTIONS 7
#define DOL_DATA_SECTIONS 11

typedef struct _DOL {
    unsigned int textoffset[7];
    unsigned int dataoffset[11];
    unsigned int textloading[7];
    unsigned int dataloading[11];
    unsigned int textsizes[7];
    unsigned int datasizes[11];
    unsigned int bssaddr;
    unsigned int bsssize;
    unsigned int entrypoint;
} DOL;

#endif // _DOL_H