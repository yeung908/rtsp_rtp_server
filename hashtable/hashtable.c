/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdio.h>
/* hashtable.c: Open addressing hashtable implementation */
#include <stdlib.h>
#include <strings.h>
#include "hashtable.h"
#include "../common.h"

/* A cell is free when key = NULL */
struct _cell {
    void *key;
    void *value;
};
/* Open addressing hashtable implementation
   The size of the table is doubled when nelems/size > 0.75
   The size of the table is halved when nelems/size < 0.25
   The size can never be less than minsize
   calchash is a pointer to a function that generates the hash from a key
   equalkeys returns 1 when its two arguments are equal 
   When inserting a key, if the key already exists, the value is overwritten*/
struct _hashtable {
    unsigned long nelems;
    unsigned long size;
    unsigned long minsize;
    hashfunc calchash;
    cmpfunc equalkeys;
    cell *cells;
    short freeelems;
};

void
freehashtable (hashtable **ht)
{
    free ((*ht)->cells);
    free (*ht);
    *ht = NULL;
    return;
}

void
clearhashtable (hashtable **ht)
{
    cell *curcell;
    curcell = (*ht)->cells + (*ht)->size;
    if ((*ht)->freeelems) {
        while (curcell-- != (*ht)->cells)
            if (curcell->key) {
                free (curcell->key);
                free (curcell->value);
                curcell->key = NULL;
            }
    } else {
        bzero((*ht)->cells, sizeof(cell) * (*ht)->size);
    }
    (*ht)->nelems = 0;
    return;
}
static Hashstatus
transfercells (hashtable **dest, hashtable **orig)
{
    Hashstatus st;
    cell *curcell;
    if ((*dest)->size < (*orig)->nelems)
        return(ERR);
    curcell = (*orig)->cells + (*orig)->size;

    while (curcell-- != (*orig)->cells)
        if (curcell->key)
            if ( (st = puthashtable (dest, curcell->key, curcell->value)) != OK ) {
                return(st);
            }
    return(OK);
}

static Hashstatus
doublehashtable (hashtable **ht)
{
    hashtable *newht;
    Hashstatus st;
    if ( (newht = newhashtable ((*ht)->calchash, (*ht)->equalkeys, (*ht)->size*2 + 1, (*ht)->freeelems)) == NULL )
        return(ERR);
    newht->minsize = (*ht)->minsize;
    if ( (st = transfercells (&newht,ht)) != OK ) {
        freehashtable (&newht);
        return(st);
    }
    freehashtable (ht);
    *ht = newht;
    return(OK);
}

static Hashstatus
halvehashtable (hashtable **ht)
{
    hashtable *newht;
    unsigned long newsize;
    Hashstatus st;
    /* The size cannot be less than minsize
       If size is equal to minsize do nothing
       If size is less than minsize, set size to minsize */
    if ((*ht)->size == (*ht)->minsize)
        return(MINSIZE);
    newsize = (*ht)->size / 2;
    if (newsize < (*ht)->minsize)
        newsize = (*ht)->minsize;
    if (newsize % 2 == 0)
        newsize++;
    if ( (newht = newhashtable ((*ht)->calchash, (*ht)->equalkeys, newsize, (*ht)->freeelems)) == NULL)
        return(ERR);
    newht->minsize = (*ht)->minsize;
    if ( (st = transfercells (&newht, ht))  != OK ) {
        freehashtable (&newht);
        return(st);
    }
    freehashtable (ht);
    *ht = newht;
    return(OK);
}

/*@null@*/
hashtable*
newhashtable (hashfunc hfun, cmpfunc cfun, unsigned long initsize, char freeelems)
{
    hashtable *ht = NULL;
    if ( (ht = (hashtable *) malloc (sizeof(hashtable))) == NULL )
        return(NULL);
    if (initsize % 2 == 0)
        initsize++;
    if ( (ht->cells = malloc (sizeof(cell) * initsize)) == NULL ) {
        free (ht);
        return(NULL);
    }
    ht->nelems = 0;
    ht->size = ht->minsize = initsize;
    ht->calchash = hfun;
    ht->equalkeys = cfun;
    ht->freeelems = freeelems;

    bzero(ht->cells, sizeof(cell) * initsize);
    return(ht);
}

Hashstatus
puthashtable (hashtable **ht, void *key, void *value)
{
    unsigned long index;
    cell *curcell;
    index = (*ht)->calchash (key) % (*ht)->size;
    curcell = &((*ht)->cells[index]);
    /* Find an empty slot to insert the key and value 
       If the key already exists, the value is overwritten */
    while (curcell->key &&
            !(*ht)->equalkeys (key, curcell->key))
        if (++curcell == (*ht)->cells + (*ht)->size)
            curcell = (*ht)->cells;
    curcell->key = key;
    curcell->value = value;

    if ((double)++((*ht)->nelems) / (double)(*ht)->size > 0.75)
        return(doublehashtable (ht));
    return(OK);
}

/*@null@*/
void *
gethashtable (hashtable **ht, void *key)
{
    unsigned long index;
    index = (*ht)->calchash (key) % (*ht)->size;
    /* Search the occupied slots for a match with the key */
    while ((*ht)->cells[index].key) {
        if ((*ht)->equalkeys (key, (*ht)->cells[index].key))
            return((*ht)->cells[index].value);
        index = (index + 1) % (*ht)->size;
    }
    return(NULL);
}

/*@null@*/
Hashstatus
delhashtable (hashtable **ht, void *key)
{
    int found;
    unsigned long index;
    unsigned long hash;
    cell *curcell;
    cell *oldcell;
    cell *hashcell;
    Hashstatus st;
    hash = (*ht)->calchash (key);
    index = hash % (*ht)->size;
    curcell = &((*ht)->cells[index]);
    found = 0;
    st = MINSIZE;
    while (curcell->key) {
        if (!found && (*ht)->equalkeys (key, curcell->key)) {
            if ((*ht)->freeelems) {
                free (curcell->key);
                free (curcell->value);
            }
            curcell->key = NULL;
            oldcell = curcell;
            found = 1;
            if ((double)--(*ht)->nelems / (double)(*ht)->size < 0.25) {
                st = halvehashtable (ht);
            }
        } else if (st == MINSIZE && found) {
            /* Get the pointer to the first valid position
               of the element in this cell */
            hashcell = &((*ht)->cells[(*ht)->calchash (curcell->key) %
                    (*ht)->size]);
            /* This expression avoids hashcell being between
               curcell (low) and oldcell (high) */
            if ((curcell < oldcell && curcell < hashcell && hashcell <= oldcell) ||
                    (curcell > oldcell && (curcell < hashcell || hashcell <= oldcell))) {
                oldcell->key = curcell->key;
                oldcell->value = curcell->value;
                oldcell = curcell;
                curcell->key = NULL;
            }
        }
        if (st != MINSIZE)
            return(st);
        if (++curcell == (*ht)->cells + (*ht)->size)
            curcell = (*ht)->cells;
    }
    return(OK);
}
