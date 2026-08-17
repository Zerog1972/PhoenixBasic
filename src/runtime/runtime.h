/*
 * runtime.h - Moteur d'execution GFA Basic 3.5 (Runtime)
 * ========================================================
 * Definit les types fondamentaux du runtime : contexte d'execution,
 * pile d'appels, machine virtuelle a bytecode, opcodes.
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 5, 6, 7
 */

#ifndef GFA_RUNTIME_H
#define GFA_RUNTIME_H

#include "os_layer.h"
#include "strings.h"
#include "gfamath.h"
#include "bit_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Types de variables GFA                                             */
/* ------------------------------------------------------------------ */

typedef enum {
    GFA_VAR_BOOL    = 0,  /* ! : booleen (FALSE=0, TRUE=-1)          */
    GFA_VAR_BYTE    = 1,  /* | : octet non signe (0-255)             */
    GFA_VAR_WORD    = 2,  /* & : entier 16 bits signe                */
    GFA_VAR_LONG    = 3,  /* % : entier 32 bits signe                */
    GFA_VAR_FLOAT   = 4,  /* # / defaut : flottant double precision  */
    GFA_VAR_STRING  = 5,  /* $ : chaine de caracteres                */
    GFA_VAR_ARRAY   = 6,  /* () : tableau                            */
    GFA_VAR_LABEL   = 7   /* etiquette (interne)                     */
} gfa_var_type;

/*
 * Representation d'une variable GFA.
 *
 * Pour les types scalaires, la valeur est stockee directement dans
 * l'union. Pour les chaines, data.str pointe vers une zone allouee.
 * Pour les tableaux, data.arr contient le descripteur complet.
 */
typedef struct gfa_variable {
    gfa_var_type type;          /* Type de la variable               */
    char        *name;          /* Nom (pour debug et lookup)        */
    int          name_len;      /* Longueur du nom                   */
    int          is_global;     /* 1 = globale, 0 = locale            */
    int          is_reserved;   /* 1 = variable reservee (TRUE/FALSE..)*/

    union {
        os_byte   bool_val;     /* Booleen (0 ou 255 pour -1)        */
        os_byte   byte_val;     /* Octet non signe                   */
        os_int16  word_val;     /* Entier 16 bits signe              */
        os_int32  long_val;     /* Entier 32 bits signe              */
        double    float_val;    /* Flottant double precision         */

        struct {
            char    *data;      /* Donnees de la chaine              */
            os_int32 length;    /* Longueur actuelle                 */
            os_int32 capacity;  /* Capacite allouee                  */
        } str;

        struct {
            gfa_var_type elem_type;  /* Type des elements            */
            int    num_dims;         /* Nombre de dimensions (1-7)   */
            os_int32 *dim_sizes;     /* Tailles par dimension        */
            char   *data;            /* Donnees brutes               */
            os_int32 total_elements; /* Nombre total d'elements      */
            os_int32 element_size;   /* Taille d'un element en octets*/
            os_int32 base;           /* Index bas (OPTION BASE)      */
            int      is_matrix;      /* 1 = matrice MAT              */
        } arr;

    } value;

    struct gfa_variable *next;  /* Liste chainee (hash table)        */
} gfa_variable;

/* Table de symboles (stockage des variables) */
typedef struct {
    gfa_variable **buckets;     /* Table de hachage                  */
    int            num_buckets; /* Nombre de buckets                 */
    int            count;       /* Nombre total de variables         */
    int            option_base; /* OPTION BASE (0 ou 1)              */
    char           def_type;    /* Type par defaut (lettre -> type)  */
} gfa_symbol_table;

/* ------------------------------------------------------------------ */
/* Pile de valeurs (Value Stack) - pour l'evaluation d'expressions    */
/* ------------------------------------------------------------------ */

typedef enum {
    GFA_VAL_NONE    = 0,
    GFA_VAL_BOOL    = 1,
    GFA_VAL_BYTE    = 2,
    GFA_VAL_WORD    = 3,
    GFA_VAL_LONG    = 4,
    GFA_VAL_FLOAT   = 5,
    GFA_VAL_STRING  = 6,
    GFA_VAL_ADDRESS = 7   /* Adresse memoire (entier 32 bits) */
} gfa_value_type;

typedef struct {
    gfa_value_type type;
    union {
        os_byte  b;
        os_int16 w;
        os_int32 l;
        double   f;
        char    *s;     /* Chaine (allouee dynamiquement) */
        void    *addr;  /* Pointeur generique */
    } data;
    int owns_string;    /* 1 si la chaine doit etre liberee */
    os_int32 str_len;   /* Longueur explicite (0 = strlen) : chaines
                          binaires avec octets nuls (MKI$ et co). */
} gfa_value;

#define GFA_VALUE_STACK_SIZE 1024

/* ------------------------------------------------------------------ */
/* Pile d'appels (Call Stack)                                         */
/* ------------------------------------------------------------------ */

typedef struct gfa_call_frame {
    int     return_ip;         /* Adresse de retour (IP)             */
    int     return_sp;         /* Stack pointer au moment de l'appel */
    int     proc_index;        /* Index de la procedure              */
    gfa_variable **local_vars; /* Variables locales                  */
    int     local_count;       /* Nombre de variables locales        */
    int     is_gosub;          /* 1 = GOSUB, 0 = PROCEDURE           */
    int     saved_count;       /* Nb de variables sauvegardees       */
    gfa_variable *saved_vars[16]; /* Variables sauvegardees          */
    gfa_value *saved_vals[16]; /* Valeurs sauvegardees (allouees)  */
} gfa_call_frame;

#define GFA_MAX_CALL_DEPTH 256

/* Taille initiale du buffer de bytecode */
#define GFA_BYTECODE_INIT_SIZE 1024

/* ------------------------------------------------------------------ */
/* Bytecode - Instructions de la machine virtuelle                    */
/* ------------------------------------------------------------------ */

typedef enum {
    /* Empilement / depilement */
    OP_NOP          = 0,
    OP_PUSH_CONST   = 1,    /* Empile une constante                  */
    OP_PUSH_VAR     = 2,    /* Empile la valeur d'une variable       */
    OP_PUSH_STRING  = 3,    /* Empile une chaine constante           */
    OP_POP          = 4,    /* Depile et ignore                      */
    OP_POP_STORE    = 5,    /* Depile et stocke dans une variable    */
    OP_DUP          = 6,    /* Duplique le sommet de pile            */
    OP_SWAP         = 7,    /* Echange les deux sommets de pile      */

    /* Arithmetique */
    OP_ADD          = 10,
    OP_SUB          = 11,
    OP_MUL          = 12,
    OP_DIV          = 13,
    OP_MOD          = 14,
    OP_POW          = 15,
    OP_INT_DIV      = 17,   /* DIV : division entiere */
    OP_NEG          = 16,

    /* Comparaisons */
    OP_EQ           = 20,
    OP_NE           = 21,
    OP_LT           = 22,
    OP_LE           = 23,
    OP_GT           = 24,
    OP_GE           = 25,
    OP_APPROX_EQ    = 26,   /* == comparaison approximative */

    /* Logique */
    OP_AND          = 30,
    OP_OR           = 31,
    OP_XOR          = 32,
    OP_NOT          = 33,
    OP_EQV          = 34,
    OP_IMP          = 35,

    /* Controle de flux */
    OP_JMP          = 40,   /* Saut inconditionnel                   */
    OP_JMP_IF_FALSE = 41,   /* Saut si faux (depile et teste)        */
    OP_JMP_IF_TRUE  = 42,   /* Saut si vrai                          */
    OP_CALL         = 43,   /* Appel procedure/GOSUB                 */
    OP_RET          = 44,   /* Retour de procedure/GOSUB             */
    OP_CALL_BUILTIN = 45,   /* Appel fonction integree               */

    /* Instructions GFA specifiques */
    OP_PRINT        = 50,
    OP_PRINT_CHAN   = 51,
    OP_INPUT        = 52,
    OP_LINE_INPUT   = 53,
    OP_CLS          = 54,
    OP_LOCATE       = 55,
    OP_COLOR        = 56,
    OP_LINE_GFX     = 57,
    OP_CIRCLE_GFX   = 58,
    OP_BOX_GFX      = 59,
    OP_PBOX_GFX     = 60,
    OP_OPENW        = 61,
    OP_CLOSEW       = 62,

    /* Fichiers */
    OP_OPEN_FILE    = 70,
    OP_CLOSE_FILE   = 71,
    OP_PRINT_FILE   = 72,
    OP_INPUT_FILE   = 73,

    /* Tableaux */
    OP_DIM          = 80,
    OP_ARRAY_LOAD   = 81,
    OP_ARRAY_STORE  = 82,

    /* Memory */
    OP_PEEK         = 90,
    OP_POKE         = 91,
    OP_DPEEK        = 92,
    OP_DPOKE        = 93,
    OP_LPEEK        = 94,
    OP_LPOKE        = 95,
    OP_SPOKE        = 96,
    OP_SDPOKE       = 97,
    OP_SLPOKE       = 98,

    /* Evenements */
    OP_EVERY        = 100,
    OP_AFTER        = 101,
    OP_ON_ERROR     = 102,
    OP_ERROR        = 103,
    OP_RESUME       = 104,
    OP_FATAL        = 105,

    /* Son */
    OP_SOUND        = 110,
    OP_BEEP          = 111,

    /* Systeme */
    OP_GEMDOS       = 120,
    OP_BIOS         = 121,
    OP_XBIOS        = 122,
    OP_QUIT         = 123,
    OP_END          = 124,
    OP_STOP         = 125,

    /* Locals */
    OP_SAVE_LOCAL   = 126,  /* Pop arg, save old val, assign to var*/
    OP_BIND_REF     = 127,  /* Pop arg, assign to var (no save, VAR) */
    OP_PRINT_NL     = 128,  /* Output newline                        */
    OP_BLOAD        = 150,  /* BLOAD filename, addr                   */
    OP_BSAVE        = 151,  /* BSAVE filename, start, end             */
    OP_BGET         = 152,  /* BGET #channel, addr, count             */
    OP_BPUT         = 153,  /* BPUT #channel, addr, count             */
    OP_PRINT_AT     = 156,  /* PRINT AT(x,y); expr                    */
    OP_PRINT_USING  = 157,  /* PRINT USING fmt$; expr                 */
    OP_ON_GOTO      = 154,  /* ON expr GOTO label1, label2, ...       */
    OP_ON_GOSUB     = 155,  /* ON expr GOSUB label1, label2, ...      */

    /* Debug */
    OP_TRON         = 130,
    OP_TROFF        = 131,

    /* Meta */
    OP_LABEL        = 140,  /* Marqueur d'etiquette (pas execute)    */
    OP_LINE_NUM     = 141,  /* Marqueur de numero de ligne           */

    /* Tableaux : operations sur elements / tri */
    OP_ERASE_VAR   = 160,  /* operand = ptr var ; efface tableau in place */
    OP_CLEAR_ALL    = 161,  /* CLEAR : reset toutes les variables        */
    OP_QSORT        = 162,  /* operand = ptr arr ; pile [lo][hi]         */
    OP_SSORT        = 163,  /* tri Shell                                  */
    OP_INSERT_ELEM  = 164,  /* operand = ptr arr ; pile [idx][val]       */
    OP_DELETE_ELEM  = 165,  /* operand = ptr arr ; pile [idx]            */

    /* Graphismes etendus (VDI ANSI) */
    OP_PLOT_GFX     = 166,  /* pile [x][y]                               */
    OP_TEXT_GFX     = 167,  /* pile [x][y][texte$]                       */
    OP_POLY_GFX     = 168,  /* pile [n][xy...][fill] ; operand = mode    */
    OP_FILL_GFX     = 169,  /* pile [x][y][limite]                       */
    OP_GETBIT_GFX   = 170,  /* pile [x1][y1][x2][y2][var$] capture       */
    OP_PUTBIT_GFX   = 171,  /* pile [x][y][var$]                         */
    OP_SETCOLOR     = 172,  /* pile [n][val] registre palette            */
    OP_MODE_GFX     = 173,  /* pile [mode]                               */
    OP_CLIP_GFX     = 174,  /* pile [x1][y1][x2][y2] (ACLIP)             */

    /* Fenetres GEM (emulation ANSI) */
    OP_WINDOW_STMT  = 175,  /* operand = sous-op (CLEARW/TITLEW/...)     */

    /* Turtle (DRAW) */
    OP_DRAW_TURTLE  = 176,  /* pile [prog$]                              */
    OP_DRAW_QUERY   = 177,  /* pile [q] : 0=X, 1=Y, 2=angle              */

    /* Matrices (MAT) */
    OP_MAT_CLR      = 180,  /* operand = ptr mat                         */
    OP_MAT_ONE      = 181,  /* operand = ptr mat                         */
    OP_MAT_CPY      = 182,  /* operand = src ptr ; pile [dst ptr]        */
    OP_MAT_ADD      = 183,  /* pile [dst][b][a]                          */
    OP_MAT_SUB      = 184,
    OP_MAT_MUL      = 185,
    OP_MAT_TRANS    = 186,  /* pile [dst][src]                           */
    OP_MAT_INV      = 187,
    OP_MAT_DET      = 188,  /* pile [src] -> scalaire                    */
    OP_MAT_RANG     = 189,
    OP_MAT_NORM     = 190,
    OP_MAT_SET      = 191,  /* pile [dst][val]                           */
    OP_MAT_PRINT    = 192,  /* pile [src]                                */
    OP_MAT_READ     = 193,  /* str_index = nom cible (lit depuis DATA)  */
    OP_ELLIPSE_GFX  = 195,  /* pile [x][y][rx][ry][fill]                */
    OP_ACHAR_GFX    = 196,  /* pile [x][y][code]                        */
    OP_MAT_INPUT    = 197,  /* str_index = nom cible (console)          */
    OP_ARRAYFILL    = 194,  /* pile [valeur] ; operand = tableau        */
    OP_DIM_QUESTION = 200,  /* operand = tableau ; push chaine dims     */
    OP_LINE_INPUT_FILE = 198, /* pile [canal] ; operand = var$          */
    OP_WINDOW_GFX   = 199,  /* pile [x0][y0][x1][y1] : WINDOW (..),(..) */

    OP_COUNT
} gfa_opcode;

/*
 * Une instruction bytecode.
 * les operandes sont stockees dans des tableaux paralleles.
 */
typedef struct {
    gfa_opcode opcode;
    union {
        os_int32  int_val;     /* Entier / index                     */
        double    float_val;   /* Flottant                           */
        int       str_index;   /* Index dans la table des chaines    */
        int       var_index;   /* Index dans la table des variables  */
        void     *ptr_val;     /* Pointeur (variable, etc.)          */
    } operand;
    int has_operand2;          /* 1 si deuxieme operande present     */
    union {
        os_int32  int_val2;
        int       index2;
    } operand2;
} gfa_instruction;

/*
 * Module de bytecode complet (programme compile).
 */
typedef struct {
    gfa_instruction *code;     /* Tableau d'instructions              */
    int              length;   /* Nombre d'instructions               */
    int              capacity; /* Capacite allouee                    */
    char           **strings;  /* Table des chaines constantes        */
    int              str_count;
    double          *data_values; /* Table des valeurs DATA             */
    int              data_count;
    int              data_ptr;    /* Pointeur DATA courant              */
} gfa_bytecode;

/* ------------------------------------------------------------------ */
/* Contexte d'execution (Runtime Context)                             */
/* ------------------------------------------------------------------ */

typedef struct gfa_runtime {
    /* Bytecode en cours d'execution */
    gfa_bytecode   *program;
    int             ip;            /* Instruction pointer              */

    /* Pile de valeurs */
    gfa_value       value_stack[GFA_VALUE_STACK_SIZE];
    int             sp;            /* Stack pointer                    */

    /* Pile d'appels */
    gfa_call_frame  call_stack[GFA_MAX_CALL_DEPTH];
    int             call_depth;

    /* Table de symboles */
    gfa_symbol_table *globals;

    /* Donnees DATA/READ */
    double         *data_values;  /* Valeurs DATA                      */
    int             data_count;
    int             data_ptr;
    struct gfa_runtime *data_restore_target; /* Pour RESTORE           */
    os_int32        data_label_ptr;         /* Pointeur DATA en memoire*/

    /* Fichiers (delegue a files.c) */
    int             files_initialized;

    /* Etat fenetres / graphique */
    int             windows[16];
    int             window_count;
    int             cursor_x;
    int             cursor_y;
    int             current_color;
    int             fill_color;
    int             fill_style;
    int             fill_pattern;
    int             line_style;
    int             line_thickness;

    /* Evenements */
    int             error_code;
    int             error_label;
    int             on_error_active;
    int             on_break_label;
    int             resume_ip;       /* IP to resume after error handler */
    int             fatal_error;     /* 1 if FATAL was called, blocks RESUME */
    int             break_pending;
    int             every_active;
    int             every_ticks;
    int             every_label;
    int             after_active;
    int             after_ticks;
    int             after_label;

    /* Trace */
    int             trace_on;
    int             current_line;

    /* Etat general */
    int             running;       /* 1 = en cours d'execution        */
    int             stopped;       /* 1 = STOP rencontre              */
    int             quit_code;     /* Code de sortie                  */

    /* Tampon clavier emule (KEYPRESS/KEYGET/KEYTEST/KEYLOOK) */
    int             keybuf[32];    /* Codes ASCII en attente          */
    int             keybuf_count;  /* Nombre d'entrees dans keybuf    */

    /* Resolution ecran */
    int             screen_mode;   /* 0=LOW, 1=MEDIUM, 2=HIGH         */
    int             screen_width;
    int             screen_height;

    /* Variable "courante" (derniere referencee) pour SGET/SPUT/BGET/BPUT */
    gfa_variable   *last_var;

    /* Dimensions de la derniere zone GET (pour PUT) */
    int             capture_w;
    int             capture_h;

    /* Turtle (DRAW) : x, y en pixels, angle en degres */
    int             turtle_x;
    int             turtle_y;
    int             turtle_angle;
    int             turtle_pen_down;
    int             turtle_color;

    /* KEYDEF : chaine associee a chaque touche de fonction (1-10) */
    char           *keydef[11];

    /* Retour de fonction */
    gfa_value       function_return;

    /* CHAIN : hook appele pour executer le programme cible
       (branche par main.c). NULL = CHAIN indisponible. */
    int           (*chain_fn)(const char *path);

} gfa_runtime;

/* ------------------------------------------------------------------ */
/* API du Runtime                                                     */
/* ------------------------------------------------------------------ */

/*
 * gfa_runtime_init - Initialise le runtime.
 * Retourne un pointeur vers le contexte cree.
 */
gfa_runtime *gfa_runtime_init(void);

/*
 * gfa_runtime_set_chain_fn - Branche le hook CHAIN.
 * fn est appele avec le nom du programme cible ; son code retour
 * est celui de l'executeur (0 = succes). NULL desactive CHAIN.
 */
void gfa_runtime_set_chain_fn(gfa_runtime *rt, int (*fn)(const char *path));

/*
 * gfa_runtime_shutdown - Libere toutes les ressources du runtime.
 */
void gfa_runtime_shutdown(gfa_runtime *rt);

/*
 * gfa_runtime_load - Charge un bytecode dans le runtime.
 */
int gfa_runtime_load(gfa_runtime *rt, gfa_bytecode *bc);

/*
 * gfa_runtime_execute - Execute le bytecode charge.
 * Retourne 0 si succes, code d'erreur sinon.
 */
int gfa_runtime_execute(gfa_runtime *rt);

/*
 * gfa_runtime_step - Execute une seule instruction.
 * Retourne 0 si succes, -1 si fin de programme.
 */
int gfa_runtime_step(gfa_runtime *rt);

/*
 * gfa_runtime_stop - Arrete l'execution en cours.
 */
void gfa_runtime_stop(gfa_runtime *rt);

/*
 * gfa_runtime_continue - Continue apres un STOP.
 */
void gfa_runtime_continue(gfa_runtime *rt);

/*
 * gfa_runtime_get_error - Retourne le dernier code d'erreur.
 */
int gfa_runtime_get_error(gfa_runtime *rt);

/* ------------------------------------------------------------------ */
/* Gestion des variables                                              */
/* ------------------------------------------------------------------ */

/*
 * gfa_var_create - Cree une nouvelle variable dans la table.
 */
gfa_variable *gfa_var_create(gfa_symbol_table *table, const char *name,
                              gfa_var_type type);

/*
 * gfa_var_lookup - Recherche une variable par nom.
 */
gfa_variable *gfa_var_lookup(gfa_symbol_table *table, const char *name);

/*
 * gfa_var_delete - Supprime une variable.
 */
void gfa_var_delete(gfa_symbol_table *table, gfa_variable *var);

/*
 * gfa_var_get_as_float - Lit une variable sous forme de flottant.
 */
double gfa_var_get_as_float(gfa_variable *var);

/*
 * gfa_var_get_as_long - Lit une variable sous forme d'entier long.
 */
os_int32 gfa_var_get_as_long(gfa_variable *var);

/*
 * gfa_var_get_as_string - Lit une variable sous forme de chaine.
 * Retourne un pointeur interne, ne pas liberer.
 */
const char *gfa_var_get_as_string(gfa_variable *var);

/*
 * gfa_var_set_from_float - Affecte un flottant a une variable.
 */
void gfa_var_set_from_float(gfa_variable *var, double value);

/*
 * gfa_var_set_from_long - Affecte un entier long a une variable.
 */
void gfa_var_set_from_long(gfa_variable *var, os_int32 value);

/*
 * gfa_var_set_from_string - Affecte une chaine a une variable.
 */
void gfa_var_set_from_string(gfa_variable *var, const char *value);

/*
 * gfa_var_set_from_string_len - Affecte une chaine a une variable
 * avec longueur explicite (donnees binaires, ex : MKI$).
 */
void gfa_var_set_from_string_len(gfa_variable *var, const char *value,
                                 os_int32 len);

/*
 * gfa_var_get_address - Retourne un pointeur vers la valeur brute
 * de la variable (pour PEEK/POKE/VARPTR).
 */
void *gfa_var_get_address(gfa_variable *var);

/*
 * gfa_symbol_table_init - Cree une nouvelle table de symboles.
 */
gfa_symbol_table *gfa_symbol_table_init(int num_buckets);

/*
 * gfa_symbol_table_free - Libere une table de symboles.
 */
void gfa_symbol_table_free(gfa_symbol_table *table);

/*
 * gfa_symbol_table_clear_vars - Efface les variables (CLEAR).
 */
void gfa_symbol_table_clear_vars(gfa_symbol_table *table);

/*
 * gfa_var_array_create - Cree un tableau dimensionne.
 */
gfa_variable *gfa_var_array_create(gfa_symbol_table *table,
                                    const char *name, gfa_var_type elem_type,
                                    int num_dims, os_int32 *dim_sizes,
                                    os_int32 base);

/*
 * gfa_var_type_from_name - Type GFA d'une variable d'apres le suffixe
 * du dernier caractere de son nom ($=chaine, %=long, &=word,
 * |=byte, !=bool, # ou aucun=flottant).
 */
gfa_var_type gfa_var_type_from_name(const char *name);

/*
 * gfa_var_array_get_element - Retourne un pointeur vers l'element
 * d'un tableau multidimensionnel.
 */
void *gfa_var_array_get_element(gfa_variable *var, int *indices);

/*
 * gfa_var_array_fill - Remplit un tableau avec une valeur (ARRAYFILL).
 */
void gfa_var_array_fill(gfa_variable *var, double value);

/*
 * gfa_array_quicksort / gfa_array_shellsort - Tries de tableaux
 * doubles in place sur la tranche [lo, hi] (bornes incluses).
 */
void gfa_array_quicksort(double *arr, int lo, int hi);
void gfa_array_shellsort(double *arr, int lo, int hi);

/*
 * gfa_var_array_count - Retourne le nombre d'elements (DIM?).
 */
os_int32 gfa_var_array_count(gfa_variable *var);

/* ------------------------------------------------------------------ */
/* Bytecode                                                           */
/* ------------------------------------------------------------------ */

/*
 * gfa_bytecode_create - Cree un module bytecode vide.
 */
gfa_bytecode *gfa_bytecode_create(void);

/*
 * gfa_bytecode_free - Libere un module bytecode.
 */
void gfa_bytecode_free(gfa_bytecode *bc);

/*
 * gfa_bytecode_emit - Emet une instruction dans le bytecode.
 * Retourne l'index de l'instruction emise.
 */
int gfa_bytecode_emit(gfa_bytecode *bc, gfa_opcode opcode);

/*
 * gfa_bytecode_emit_int - Emet une instruction avec operande entier.
 */
int gfa_bytecode_emit_int(gfa_bytecode *bc, gfa_opcode opcode,
                           os_int32 operand);

/*
 * gfa_bytecode_emit_float - Emet une instruction avec operande flottant.
 */
int gfa_bytecode_emit_float(gfa_bytecode *bc, gfa_opcode opcode,
                             double operand);

/*
 * gfa_bytecode_emit_str - Emet une instruction avec operande chaine.
 */
int gfa_bytecode_emit_str(gfa_bytecode *bc, gfa_opcode opcode,
                           const char *str);

/*
 * gfa_bytecode_add_string - Ajoute une chaine a la table des constantes.
 * Retourne l'index.
 */
int gfa_bytecode_add_string(gfa_bytecode *bc, const char *str);

/*
 * gfa_bytecode_patch - Modifie l'operande d'une instruction existante.
 */
void gfa_bytecode_patch(gfa_bytecode *bc, int index, os_int32 operand);

/*
 * gfa_bytecode_current_ip - Retourne l'index de la prochaine instruction.
 */
int gfa_bytecode_current_ip(gfa_bytecode *bc);

/* ------------------------------------------------------------------ */
/* Pile de valeurs                                                    */
/* ------------------------------------------------------------------ */

void gfa_value_push_bool(gfa_runtime *rt, int value);
void gfa_value_push_byte(gfa_runtime *rt, os_byte value);
void gfa_value_push_word(gfa_runtime *rt, os_int16 value);
void gfa_value_push_long(gfa_runtime *rt, os_int32 value);
void gfa_value_push_float(gfa_runtime *rt, double value);
void gfa_value_push_string(gfa_runtime *rt, char *str, int owns);
void gfa_value_push_string_len(gfa_runtime *rt, char *str,
                               os_int32 len, int owns);
void gfa_value_push_addr(gfa_runtime *rt, void *addr);
gfa_value *gfa_value_pop(gfa_runtime *rt);
gfa_value *gfa_value_peek(gfa_runtime *rt, int depth);
void gfa_value_discard(gfa_runtime *rt, int count);

/*
 * gfa_value_to_float - Convertit une valeur en flottant.
 */
double gfa_value_to_float(gfa_value *val);

/*
 * gfa_value_to_long - Convertit une valeur en entier long.
 */
os_int32 gfa_value_to_long(gfa_value *val);

/*
 * gfa_value_to_bool - Convertit une valeur en booleen (0 ou -1).
 */
int gfa_value_to_bool(gfa_value *val);

#ifdef __cplusplus
}
#endif

#endif /* GFA_RUNTIME_H */
