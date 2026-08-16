/*
 * vmem.c - Memoire virtuelle emulee (PEEK/POKE/BYTE{}/...)
 * ========================================================
 * Tampon de VMEM_SIZE octets emulant la memoire physique ST.
 *
 * Ordre d'octets : 68000 (big-endian) pour les valeurs
 * multi-octets, comme sur la machine cible.
 *
 * Les adresses sont masquee sur VMEM_SIZE (puissance de 2),
 * ce qui emule le comportement de wrap de l'espace d'adresses
 * de l'Atari ST.
 */

#include "vmem.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Etat global                                                         */
/* ------------------------------------------------------------------ */

static unsigned char *g_mem = NULL;

/* ------------------------------------------------------------------ */
/* Initialisation                                                      */
/* ------------------------------------------------------------------ */

int vmem_init(void)
{
    if (g_mem != NULL) {
        return 1; /* deja initialise */
    }
    g_mem = (unsigned char *)malloc((size_t)VMEM_SIZE);
    if (g_mem == NULL) {
        return 0;
    }
    memset(g_mem, 0, (size_t)VMEM_SIZE);
    return 1;
}

void vmem_shutdown(void)
{
    if (g_mem != NULL) {
        free(g_mem);
        g_mem = NULL;
    }
}

os_int32 vmem_size(void)
{
    return (os_int32)VMEM_SIZE;
}

int vmem_addr_valid(os_int32 addr)
{
    return (addr >= 0 && addr < VMEM_SIZE) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Masquage d'adresse (wrap de l'espace d'adresses)                    */
/* ------------------------------------------------------------------ */

static unsigned int vmem_wrap(os_int32 addr)
{
    return (unsigned int)(addr & (VMEM_SIZE - 1));
}

/* Lecture d'un octet avec wrap, pour les valeurs multi-octets
   qui chevauchent la fin de la region. */
static unsigned char vmem_byte_at(os_int32 addr)
{
    if (g_mem == NULL) return 0;
    return g_mem[vmem_wrap(addr)];
}

/* ------------------------------------------------------------------ */
/* Lectures                                                            */
/* ------------------------------------------------------------------ */

unsigned char vmem_read_byte(os_int32 addr)
{
    unsigned int a;
    if (g_mem == NULL) return 0;
    a = vmem_wrap(addr);
    return g_mem[a];
}

unsigned short vmem_read_card(os_int32 addr)
{
    if (g_mem == NULL) return 0;
    /* Big-endian 68000 : octet MSB a l'adresse basse */
    return (unsigned short)((unsigned short)vmem_byte_at(addr) << 8 |
                            vmem_byte_at(addr + 1));
}

short vmem_read_word(os_int32 addr)
{
    return (short)vmem_read_card(addr);
}

os_int32 vmem_read_long(os_int32 addr)
{
    unsigned int v;
    if (g_mem == NULL) return 0;
    v = ((unsigned int)vmem_byte_at(addr) << 24) |
        ((unsigned int)vmem_byte_at(addr + 1) << 16) |
        ((unsigned int)vmem_byte_at(addr + 2) << 8) |
        (unsigned int)vmem_byte_at(addr + 3);
    return (os_int32)v;
}

float vmem_read_single(os_int32 addr)
{
    unsigned char buf[4];
    float f;
    int i;
    if (g_mem == NULL) return 0.0f;
    for (i = 0; i < 4; i++) buf[i] = vmem_byte_at(addr + i);
    /* IEEE 754 big-endian : on inverse si l'hote est little-endian */
    {
        unsigned char probe[2];
        probe[0] = 1;
        probe[1] = 0;
        if (*(unsigned short *)probe == 1) {
            unsigned char tmp;
            tmp = buf[0]; buf[0] = buf[3]; buf[3] = tmp;
            tmp = buf[1]; buf[1] = buf[2]; buf[2] = tmp;
        }
    }
    memcpy(&f, buf, sizeof(f));
    return f;
}

double vmem_read_double(os_int32 addr)
{
    unsigned char buf[8];
    double d;
    int i;
    if (g_mem == NULL) return 0.0;
    for (i = 0; i < 8; i++) buf[i] = vmem_byte_at(addr + i);
    {
        unsigned char probe[2];
        probe[0] = 1;
        probe[1] = 0;
        if (*(unsigned short *)probe == 1) {
            unsigned char tmp;
            tmp = buf[0]; buf[0] = buf[7]; buf[7] = tmp;
            tmp = buf[1]; buf[1] = buf[6]; buf[6] = tmp;
            tmp = buf[2]; buf[2] = buf[5]; buf[5] = tmp;
            tmp = buf[3]; buf[3] = buf[4]; buf[4] = tmp;
        }
    }
    memcpy(&d, buf, sizeof(d));
    return d;
}

/* ------------------------------------------------------------------ */
/* Ecritures                                                           */
/* ------------------------------------------------------------------ */

void vmem_write_byte(os_int32 addr, unsigned char v)
{
    unsigned int a;
    if (g_mem == NULL) return;
    a = vmem_wrap(addr);
    g_mem[a] = v;
}

void vmem_write_word(os_int32 addr, unsigned short v)
{
    unsigned int a;
    if (g_mem == NULL) return;
    a = vmem_wrap(addr);
    g_mem[a]     = (unsigned char)((v >> 8) & 0xFF);
    g_mem[a + 1] = (unsigned char)(v & 0xFF);
}

void vmem_write_long(os_int32 addr, os_int32 v)
{
    unsigned int a;
    unsigned int uv = (unsigned int)v;
    if (g_mem == NULL) return;
    a = vmem_wrap(addr);
    g_mem[a]     = (unsigned char)((uv >> 24) & 0xFF);
    g_mem[a + 1] = (unsigned char)((uv >> 16) & 0xFF);
    g_mem[a + 2] = (unsigned char)((uv >> 8) & 0xFF);
    g_mem[a + 3] = (unsigned char)(uv & 0xFF);
}

/* ------------------------------------------------------------------ */
/* Allocation dynamique (MALLOC / MFREE / FRE)                         */
/* ------------------------------------------------------------------ */

/* Region d'allocation : 64 Ko reserves en bas (vectors), 256 Ko en
   haut (zone TOS) ; le reste est disponible pour MALLOC. */
#define VMEM_HEAP_BOTTOM  (64 * 1024)
#define VMEM_HEAP_TOP     (VMEM_SIZE - 256 * 1024)
#define VMEM_MAX_BLOCKS   128

static struct {
    os_int32 addr;   /* adresse debut du bloc                        */
    os_int32 size;   /* taille du bloc                               */
    int      used;   /* 1 = alloue, 0 = libre                        */
} g_slots[VMEM_MAX_BLOCKS];
static int g_slot_count = 0;

static int vmem_slot_fits(os_int32 a, os_int32 s)
{
    int i;
    for (i = 0; i < g_slot_count; i++) {
        if (g_slots[i].addr <= a &&
            a + s <= g_slots[i].addr + g_slots[i].size) {
            return 1;
        }
    }
    return 0;
}

os_int32 vmem_malloc(os_int32 n)
{
    int i;
    os_int32 addr;

    if (g_mem == NULL || n <= 0) return 0;
    if (n & 1) n++; /* alignement sur 2 octets */

    /* 1. Premier trou libre assez grand (eventuellement scinde) */
    for (i = 0; i < g_slot_count; i++) {
        if (!g_slots[i].used && g_slots[i].size >= n) {
            os_int32 rest;
            g_slots[i].used = 1;
            addr = g_slots[i].addr;
            rest = g_slots[i].size - n;
            if (rest >= 16) {
                /* scinder : reste libre apres le bloc alloue */
                g_slots[i].size = n;
                if (g_slot_count < VMEM_MAX_BLOCKS) {
                    g_slots[g_slot_count].addr = addr + n;
                    g_slots[g_slot_count].size = rest;
                    g_slots[g_slot_count].used = 0;
                    g_slot_count++;
                } else {
                    /* pas de slot : on perd le reste (fragmentation) */
                    g_slots[i].size = n + rest;
                }
            }
            return addr;
        }
    }

    /* 2. Bump : la zone libre est en dessous du plus bas bloc alloue
       (et au-dessus de VMEM_HEAP_BOTTOM). On alloue vers le bas. */
    {
        os_int32 lowest = VMEM_HEAP_TOP;
        for (i = 0; i < g_slot_count; i++) {
            if (g_slots[i].used && g_slots[i].addr < lowest)
                lowest = g_slots[i].addr;
        }
        if (lowest - n < VMEM_HEAP_BOTTOM) return 0;
        addr = lowest - n;
        if (g_slot_count < VMEM_MAX_BLOCKS && vmem_slot_fits(addr, n)) {
            g_slots[g_slot_count].addr = addr;
            g_slots[g_slot_count].size = n;
            g_slots[g_slot_count].used = 1;
            g_slot_count++;
            return addr;
        }
    }
    return 0;
}

int vmem_free(os_int32 addr)
{
    int i;
    if (g_mem == NULL) return 0;
    for (i = 0; i < g_slot_count; i++) {
        if (g_slots[i].used && g_slots[i].addr == addr) {
            g_slots[i].used = 0;
            /* Fusion avec le bloc voisin (adresse suivante) s'il est libre */
            {
                int j;
                for (j = 0; j < g_slot_count; j++) {
                    if (j != i && !g_slots[j].used &&
                        g_slots[j].addr == addr + g_slots[i].size) {
                        g_slots[i].size += g_slots[j].size;
                        g_slots[j].size = 0;
                        g_slots[j].addr = 0;
                    }
                }
            }
            return 1;
        }
    }
    return 0;
}

os_int32 vmem_free_size(void)
{
    int i;
    os_int32 free_bytes;
    os_int32 lowest;

    if (g_mem == NULL) return 0;

    /* Taille des trous libres */
    free_bytes = 0;
    for (i = 0; i < g_slot_count; i++) {
        if (!g_slots[i].used)
            free_bytes += g_slots[i].size;
    }

    /* Espace bump disponible : sous le plus bas bloc, au-dessus du bottom */
    lowest = VMEM_HEAP_TOP;
    for (i = 0; i < g_slot_count; i++) {
        if (g_slots[i].used && g_slots[i].addr < lowest)
            lowest = g_slots[i].addr;
    }
    if (lowest > VMEM_HEAP_BOTTOM)
        free_bytes += lowest - VMEM_HEAP_BOTTOM;

    return free_bytes;
}

void vmem_copy(os_int32 dst, os_int32 src, os_int32 len)
{
    os_int32 i;
    if (g_mem == NULL || len <= 0) return;
    /* Copie octet a octet avec wrap (les plages peuvent chevaucher la fin) */
    for (i = 0; i < len; i++) {
        g_mem[vmem_wrap(dst + i)] = g_mem[vmem_wrap(src + i)];
    }
}
