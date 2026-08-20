/*
 * vm_internal.h - Declarations internes de la VM (entre modules .c)
 * ==============================================================
 * Cree par la decoupe de runtime.c (2026-08-19) :
 *   runtime.c      : coeur de la VM (boucle, opcodes de base, erreurs)
 *   vm_builtin.c   : opcode OP_CALL_BUILTIN (fonctions integrees)
 *   vm_statement.c : opcodes instructions (PRINT, fichiers, gfx, MAT...)
 *
 * En-tete interne : n'est pas destine aux modules externes.
 */

#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include "runtime.h"

/* Contrat de vm_exec_builtin() / vm_exec_statement() :
 * valeur retournee a vm_dispatch() dans runtime.c. */
#define VM_ADV   0   /* "break" : vm_dispatch fait rt->ip++       */
#define VM_RET0  1   /* instruction terminee, ne pas faire ip++   */

/* Declenche une erreur runtime (definie dans runtime.c).
   Retourne 1 si l'execution a saute dans un gestionnaire ON ERROR,
   0 sinon (erreur fatale). */
int runtime_error(gfa_runtime *rt, int code, const char *msg);

/* Execute OP_CALL_BUILTIN (vm_builtin.c) */
int vm_exec_builtin(gfa_runtime *rt, gfa_instruction *inst, os_int32 operand);

/* Execute un opcode instruction (vm_statement.c) */
int vm_exec_statement(gfa_runtime *rt, gfa_instruction *inst, os_int32 operand);

#endif /* VM_INTERNAL_H */
