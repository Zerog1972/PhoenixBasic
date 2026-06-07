/*
 * codegen.h - Generateur de bytecode GFA Basic 3.5
 * ================================================
 * Transforme un AST (produit par le parser) en bytecode
 * executable par le runtime.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 7.2
 */

#ifndef GFA_CODEGEN_H
#define GFA_CODEGEN_H

#include "ast.h"
#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * gfa_codegen_compile - Compile un AST en bytecode.
 * symbol_table : la table de symboles du runtime (pour resoudre les
 *                variables et allouer les indices)
 * ast : racine de l'AST
 * out_bc : [sortie] le bytecode genere (a liberer avec gfa_bytecode_free)
 *
 * Retourne 0 si succes, -1 si erreur.
 */
typedef struct {
    const char *name;
    int         bytecode_ip;
} gfa_label_info;

int gfa_codegen_compile(gfa_symbol_table *symbol_table,
                        ast_node *ast, gfa_bytecode **out_bc,
                        gfa_label_info *labels, int label_count);

#ifdef __cplusplus
}
#endif

#endif /* GFA_CODEGEN_H */
