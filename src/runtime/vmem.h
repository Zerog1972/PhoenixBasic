/*
 * vmem.h - Memoire virtuelle emulee (PEEK/POKE/BYTE{}/...)
 * ========================================================
 * Emule la memoire physique de l'Atari ST dans un tampon
 * alloue dynamiquement. Toutes les lectures/ecritures
 * multi-octets utilisent l'ordre d'octets 68000 (grand
 * finale, big-endian), conforme au format natif de la
 * machine cible.
 *
 * C89 strict : commentaires slash-etoile, declarations en debut
 * de bloc, pas de VLA, pas d'inline.
 */

#ifndef VMEM_H
#define VMEM_H

#include <stddef.h>
#include "../utils/os_layer.h"  /* os_int32 */

#ifdef __cplusplus
extern "C" {
#endif

/* Taille de la region emulee :
 *   - hote (developpement) : 16 Mo (espace d'adresses ST)
 *   - Atari ST (MINT)      : 256 Ko (budget memoire restreint) */
#ifdef GFA_TARGET_MINT
#define VMEM_SIZE (256 * 1024)
#else
#define VMEM_SIZE (16 * 1024 * 1024)
#endif

/*
 * vmem_init — Alloue la region memoire et l'initialise a zero.
 * Retourne 1 si succes, 0 si echec d'allocation.
 */
int vmem_init(void);

/*
 * vmem_shutdown — Libere la region memoire.
 */
void vmem_shutdown(void);

/*
 * vmem_size — Retourne la taille de la region en octets.
 */
os_int32 vmem_size(void);

/*
 * vmem_addr_valid — Retourne 1 si addr est dans la region.
 */
int vmem_addr_valid(os_int32 addr);

/* --- Allocation dynamique (MALLOC/MFREE/FRE) ----------------------- */

/*
 * vmem_malloc — Alloque n octets dans la memoire virtuelle.
 * Retourne l'adresse (big-endian, compatible ST) ou 0 si echec.
 */
os_int32 vmem_malloc(os_int32 n);

/*
 * vmem_free — Libere un bloc obtenu par vmem_malloc.
 * Retourne 1 si le bloc a ete trouve et libere, 0 sinon.
 */
int vmem_free(os_int32 addr);

/*
 * vmem_free_size — Retourne le nombre d'octets libres.
 */
os_int32 vmem_free_size(void);

/*
 * vmem_copy — Copie len octets de src vers dst (wrap d'adresse).
 */
void vmem_copy(os_int32 dst, os_int32 src, os_int32 len);

/* --- Lectures (big-endian 68000) ---------------------------------- */

/* Octet non signe (0-255) */
unsigned char vmem_read_byte(os_int32 addr);

/* Mot 16 bits non signe (CARD{} / DPEEK) */
unsigned short vmem_read_card(os_int32 addr);

/* Mot 16 bits signe (WORD{}) */
short vmem_read_word(os_int32 addr);

/* Long 32 bits signe (LONG{} / LPEEK) */
os_int32 vmem_read_long(os_int32 addr);

/* Flottant simple (SINGLE{}) */
float vmem_read_single(os_int32 addr);

/* Flottant double (DOUBLE{}) */
double vmem_read_double(os_int32 addr);

/* --- Ecritures (big-endian 68000) ---------------------------------- */

/* Octet (POKE / SPOKE) */
void vmem_write_byte(os_int32 addr, unsigned char v);

/* Mot 16 bits (DPOKE / SDPOKE / CARD{}=) */
void vmem_write_word(os_int32 addr, unsigned short v);

/* Long 32 bits (LPOKE / SLPOKE / LONG{}=) */
void vmem_write_long(os_int32 addr, os_int32 v);

#ifdef __cplusplus
}
#endif

#endif /* VMEM_H */
