/* stub tos.h emulant Pure C 1.1 - validation croisee */
#ifndef STUB_TOS_H
#define STUB_TOS_H

typedef struct {
    char  dta_reserved[21];
    char  dta_attr;
    long  dta_time;
    long  dta_date;
    long  dta_size;
    char  dta_name[14];
} DTA;

extern int  Cconin(void);
extern int  Cconis(void);
extern int  Cconout(int c);
extern void Bconout(int dev, int c);
extern long Dcreate(const char *p);
extern long Ddelete(const char *p);
extern long Dsetpath(const char *p);
extern long Dgetpath(char *b, int d);
extern long Dsetdrv(int d);
extern long Dfree(long *i, int d);
extern long Frename(int k, const char *n, const char *o);
extern long Fsfirst(const char *n, int a);
extern long Fsnext(void);
extern DTA *Fgetdta(void);
extern long Tgettime(void);
extern long Tgetdate(void);
extern void *Malloc(long s);
extern long Pterm(long c);
extern long _BasPag;

#endif