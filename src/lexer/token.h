/*
 * token.h - Types de tokens pour le lexer GFA Basic 3.5
 * ======================================================
 * Definit l'ensemble des categories de tokens reconnus par le lexer.
 * Enumeration complete de tous les mots-cles GFA Basic 3.5 (~280).
 *
 * Reference : cahier-des-charges-gfabasic.md, section 3
 */

#ifndef GFA_TOKEN_H
#define GFA_TOKEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Types de tokens                                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    /* Fin de flux */
    TOK_EOF = 0,
    TOK_EOL,

    /* Mots-cles - Controle de flux */
    TOK_IF, TOK_THEN, TOK_ELSE, TOK_ENDIF,
    TOK_FOR, TOK_TO, TOK_STEP, TOK_NEXT, TOK_DOWNTO,
    TOK_WHILE, TOK_WEND,
    TOK_REPEAT, TOK_UNTIL,
    TOK_DO, TOK_LOOP, TOK_EXIT_IF,
    TOK_GOTO, TOK_GOSUB, TOK_RETURN,
    TOK_ON, TOK_ON_ERROR, TOK_ON_BREAK, TOK_ON_MENU,
    TOK_SELECT, TOK_CASE, TOK_DEFAULT, TOK_ENDSELECT,
    TOK_STOP, TOK_END, TOK_CONT, TOK_QUIT,

    /* Definitions */
    TOK_LET, TOK_DIM, TOK_ERASE, TOK_CLEAR, TOK_CLR,
    TOK_OPTION_BASE,
    TOK_PROCEDURE, TOK_FUNCTION, TOK_ENDFUNC, TOK_LOCAL, TOK_VAR,
    TOK_PROC, TOK_ENDPROC,
    TOK_DEFFN, TOK_FN,
    TOK_DEFBIT, TOK_DEFBYT, TOK_DEFWRD, TOK_DEFNUM, TOK_DEFFLT,
    TOK_DEFSTR, TOK_DEFDBL, TOK_DEFLIST, TOK_DEFMARK, TOK_DEFLINE,
    TOK_DEFTEXT, TOK_DEFMOUSE, TOK_DEFFILL,

    /* Donnees */
    TOK_DATA, TOK_READ, TOK_RESTORE, TOK__DATA,

    /* Entrees/Sorties console */
    TOK_PRINT, TOK_PRINT_AT, TOK_PRINT_USING,
    TOK_INPUT, TOK_LINE_INPUT,
    TOK_INKEY,    /* INKEY$ */
    TOK_INP, TOK_OUT,
    TOK_CLS, TOK_LOCATE, TOK_POS, TOK_CRSCOL, TOK_CRSLIN,
    TOK_HTAB, TOK_VTAB, TOK_TAB, TOK_SPC,
    TOK_CLIP, TOK_ACLIP,

    /* Fichiers */
    TOK_OPEN, TOK_CLOSE, TOK_OPENW, TOK_CLOSEW, TOK_CLEARW,
    TOK_SEEK, TOK_RELSEEK, TOK_EOF_TOK, TOK_LOF, TOK_LOC,
    TOK_BLOAD, TOK_BSAVE, TOK_BGET, TOK_BPUT, TOK_SGET, TOK_SPUT,
    TOK_FIELD, TOK_LSET, TOK_RSET,
    TOK_EXIST, TOK_KILL, TOK_NAME, TOK_FILES, TOK_DIR_TOK,
    TOK_MKDIR, TOK_RMDIR, TOK_CHDIR, TOK_CHDRIVE,
    TOK_FSFIRST, TOK_FSNEXT, TOK_FSETDTA, TOK_FGETDTA,
    TOK_SAVE, TOK_LOAD, TOK_MERGE, TOK_LIST, TOK_LLIST,
    TOK_NEW, TOK_RUN, TOK_CHAIN, TOK_DELETE, TOK_RENUM, TOK_AUTO,
    TOK_PSAVE, TOK_STORE, TOK_RECALL,

    /* Graphisme (VDI) */
    TOK_COLOR, TOK_SETCOLOR, TOK_VSETCOLOR,
    TOK_LINE_TOK, TOK_ALINE, TOK_HLINE,
    TOK_BOX, TOK_RBOX, TOK_PBOX, TOK_PRBOX, TOK_ARECT,
    TOK_CIRCLE_TOK, TOK_PCIRCLE, TOK_PELLIPSE,
    TOK_POLYLINE, TOK_POLYFILL, TOK_POLYMARK, TOK_APOLY,
    TOK_FILL, TOK_BOUNDARY,
    TOK_PLOT, TOK_POINT, TOK_PTST,
    TOK_CURVE, TOK_ATEXT, TOK_ACHAR, TOK_TEXT, TOK_BITBLT,
    TOK_GET, TOK_PUT,
    TOK_MODE, TOK_HARDCOPY, TOK_SETDRAW, TOK_DRAW,

    /* Fenetres GEM */
    TOK_TITLEW, TOK_INFOW, TOK_TOPW, TOK_GETSIZE,
    TOK_WINDTAB,
    TOK_WIND_OPEN, TOK_WIND_CLOSE, TOK_WIND_DELETE, TOK_WIND_FIND,
    TOK_WIND_CREATE, TOK_WIND_CALC, TOK_WIND_GET, TOK_WIND_SET,
    TOK_WIND_UPDATE, TOK_W_HAND, TOK_W_INDEX, TOK_MW_OUT,
    TOK_SHOWM, TOK_HIDEM,

    /* AES (GEM) */
    TOK_ALERT, TOK_FILESELECT, TOK_FSEL_INPUT,
    TOK_APPL_INIT, TOK_APPL_EXIT, TOK_APPL_FIND,
    TOK_APPL_READ, TOK_APPL_WRITE, TOK_APPL_TPLAY, TOK_APPL_TRECORD,
    TOK_FORM_ALERT, TOK_FORM_BUTTON, TOK_FORM_CENTER,
    TOK_FORM_DIAL, TOK_FORM_DO, TOK_FORM_ERROR, TOK_FORM_KEYBD,
    TOK_FORM_INPUT, TOK_FORM_INPUT_AS,
    TOK_MENU_BAR, TOK_MENU_ICHECK, TOK_MENU_IENABLE,
    TOK_MENU_REGISTER, TOK_MENU_TEXT, TOK_MENU_TNORMAL,
    TOK_MENU, TOK_MENU_KILL, TOK_MENU_OFF,
    TOK_ON_MENU_GOSUB, TOK_ON_MENU_BUTTON, TOK_ON_MENU_KEY,
    TOK_ON_MENU_IBOX, TOK_ON_MENU_MESSAGE, TOK_ON_MENU_OBOX,
    TOK_OB_ADR, TOK_OB_FLAGS, TOK_OB_H, TOK_OB_HEAD,
    TOK_OB_NEXT, TOK_OB_SPEC, TOK_OB_STATE, TOK_OB_TAIL,
    TOK_OB_TYPE, TOK_OB_X, TOK_OB_Y, TOK_OB_W,
    TOK_OBJC_ADD, TOK_OBJC_CHANGE, TOK_OBJC_DELETE,
    TOK_OBJC_DRAW, TOK_OBJC_EDIT, TOK_OBJC_FIND,
    TOK_OBJC_OFFSET, TOK_OBJC_ORDER,
    TOK_OBJC_ADDMOVE, TOK_OBJC_MOVE, TOK_OBJC_PICK,
    TOK_OBJC_STATE, TOK_OBJC_TNORMAL,
    TOK_RSRC_LOAD, TOK_RSRC_FREE, TOK_RSRC_GADDR,
    TOK_RSRC_SADDR, TOK_RSRC_OBFIX,
    TOK_GRAF_DRAGBOX, TOK_RC_COPY, TOK_RC_INTERSECT, TOK_RCALL,
    TOK_SCRP_READ, TOK_SCRP_WRITE,

    /* Shell */
    TOK_SHEL_READ, TOK_SHEL_WRITE, TOK_SHEL_GET, TOK_SHEL_PUT,
    TOK_SHEL_FIND, TOK_SHEL_ENVRN, TOK_EXEC, TOK_SYSTEM,

    /* TOS */
    TOK_GEMDOS, TOK_BIOS, TOK_XBIOS, TOK_GEMSYS, TOK_VDISYS,

    /* VDI interne */
    TOK_CONTRL, TOK_INTIN, TOK_INTOUT, TOK_PTSIN, TOK_PTSOUT,
    TOK_GINTIN, TOK_GINTOUT, TOK_WORK_OUT,
    TOK_V_OPNWK, TOK_V_CLSWK, TOK_V_OPNVWK, TOK_V_CLSVWK,
    TOK_V_CLRWK, TOK_V_UPDWK,
    TOK_VQT_EXTENT, TOK_VQT_NAME,
    TOK_VST_LOAD_FONTS, TOK_VST_UNLOAD_FONTS, TOK_VSYNC,

    /* Son */
    TOK_SOUND, TOK_BEEP, TOK_WAVE,
    TOK_DMACONTROL, TOK_DMASOUND,

    /* Console / touches */
    TOK_KEY, TOK_ON_KEY, TOK_CONIN, TOK_CONOUT, TOK_CONOUTI,

    /* Graphique */
    TOK_WINDOW, TOK_GRAPHICS,

    /* Evenements */
    TOK_EVERY, TOK_AFTER,
    TOK_EVNT_MULTI, TOK_EVNT_MESAG, TOK_EVNT_KEYBD,
    TOK_EVNT_MOUSE, TOK_EVNT_BUTTON, TOK_EVNT_TIMER, TOK_EVNT_DCLICK,

    /* Souris / Joystick */
    TOK_MOUSE, TOK_MOUSEX, TOK_MOUSEY, TOK_MOUSEK, TOK_SETMOUSE,
    TOK_STICK, TOK_STRIG,
    TOK_PADX, TOK_PADY, TOK_PADT,
    TOK_LPENX, TOK_LPENY, TOK_TOUCH,

    /* Memoire */
    TOK_PEEK, TOK_POKE, TOK_DPEEK, TOK_DPOKE,
    TOK_LPEEK, TOK_LPOKE, TOK_SPOKE, TOK_SDPOKE, TOK_SLPOKE,
    TOK_BMOVE, TOK_MALLOC, TOK_MFREE, TOK_FRE, TOK_MSHRINK,
    TOK_HIMEM, TOK_RESERVE, TOK_BASEPAGE,
    TOK_ABSOLUTE, TOK_ARRPTR, TOK_VARPTR,

    /* Bit operations */
    TOK_BTST, TOK_BSET, TOK_BCLR, TOK_BCHG,
    TOK_SHL, TOK_SHR, TOK_ROL, TOK_ROR,

    /* Matrices */
    TOK_MAT, TOK_MAT_READ, TOK_MAT_INPUT, TOK_MAT_PRINT,
    TOK_MAT_SET, TOK_MAT_CLR, TOK_MAT_ONE,
    TOK_MAT_CPY, TOK_MAT_XCPY, TOK_MAT_TRANS,
    TOK_MAT_ADD, TOK_MAT_SUB, TOK_MAT_MUL,
    TOK_MAT_DET, TOK_MAT_QDET, TOK_MAT_RANG,
    TOK_MAT_INV, TOK_MAT_NORM, TOK_MAT_BASE, TOK_MAT_ABS, TOK_MAT_NEG,

    /* Operations sur tableaux */
    TOK_ARRAYFILL, TOK_INSERT, TOK_SWAP,
    TOK_QSORT, TOK_SSORT,

    /* Temps */
    TOK_TIME_TOK, TOK_TIMER_TOK, TOK_DATE_TOK, TOK_SETTIME,
    TOK_DELAY, TOK_PAUSE,

    /* Debug */
    TOK_TRON, TOK_TROFF, TOK_TRACE, TOK_MONITOR,

    /* Divers */
    TOK_REM, TOK_VOID, TOK_TILDE,
    TOK_SPRITE, TOK_DUMP,
    TOK_DEBUG, TOK_DEC, TOK_INC, TOK_RECORD,
    TOK_TRUE, TOK_FALSE, TOK_PI_TOK,
    TOK__C, TOK__X, TOK__Y,
    TOK_C,     /* CALL C */
    TOK_CALL,
    TOK_INLINE, TOK_RANDOMIZE, TOK_RANDOM,
    TOK_ERROR, TOK_FATAL, TOK_ERR, TOK_RESUME,
    TOK_ADD, TOK_SUB_TOK, TOK_MUL_TOK, TOK_DIV_TOK,
    TOK_STE, TOK_TT,

    /* Fonctions integrees (tokens pour les appels de fonction) */
    TOK_ABS, TOK_ASC, TOK_ATN, TOK_ASIN, TOK_ACOS,
    TOK_SIN, TOK_COS, TOK_TAN, TOK_SINH, TOK_COSH, TOK_TANH,
    TOK_SINQ, TOK_COSQ,
    TOK_EXP, TOK_LOG, TOK_LOG10, TOK_SQR,
    TOK_INT, TOK_FRAC, TOK_FIX, TOK_ROUND, TOK_CEIL_TOK, TOK_TRUNC_TOK,
    TOK_SGN, TOK_DEG, TOK_RAD,
    TOK_MIN, TOK_MAX, TOK_EVEN, TOK_ODD, TOK_PRED, TOK_SUCC,
    TOK_FACT, TOK_COMBIN, TOK_VARIAT, TOK_RND,
    TOK_CFLOAT, TOK_CINT,
    TOK_LEN, TOK_MID_TOK, TOK_LEFT_TOK, TOK_RIGHT_TOK,
    TOK_INSTR, TOK_RINSTR,
    TOK_STR_TOK, TOK_VAL, TOK_VAL_COUNT,
    TOK_CHR_TOK, TOK_BIN_TOK, TOK_HEX_TOK, TOK_OCT_TOK,
    TOK_UPPER_TOK, TOK_LCASE_TOK, TOK_LOWER_TOK, TOK_UCASE,
    TOK_TRIM_TOK, TOK_STRING_TOK, TOK_SPACE_TOK,
    TOK_MKI_TOK, TOK_MKL_TOK, TOK_MKS_TOK, TOK_MKF_TOK, TOK_MKD_TOK,
    TOK_CVI_TOK, TOK_CVL_TOK, TOK_CVS_TOK, TOK_CVF_TOK, TOK_CVD_TOK,
    TOK_BYTE_TOK, TOK_CARD, TOK_WORD_TOK, TOK_LONG_TOK,
    TOK_SINGLE, TOK_DOUBLE_TOK,
    TOK_LPEN_TOK, TOK_STICK_TOK, TOK_STRIG_TOK, TOK_PAD_TOK, TOK_TOUCH_TOK,
    TOK_KEYDEF, TOK_KEYGET, TOK_KEYLOOK, TOK_KEYPAD, TOK_KEYPRESS, TOK_KEYTEST,
    TOK_INPUT_TOK, TOK_INPMID, TOK_DIR_TOK2,
    TOK_DIM_QUESTION, TOK_DFREE,
    TOK_TYPE_TOK,

    /* Operateurs */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_CARET,
    TOK_EQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_NE,
    TOK_AND_OP, TOK_OR_OP, TOK_XOR_OP, TOK_NOT_OP, TOK_EQV_OP, TOK_IMP_OP,
    TOK_MOD_OP, TOK_DIV_OP,
    TOK_AMPERSAND,  /* & concatenation */
    TOK_APPROX_EQ,  /* == comparaison approximative */
    TOK_AT,         /* @ synonyme GOSUB */

    /* Separateurs */
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,  /* { } acces memoire typed (BYTE{}/...) */
    TOK_COMMA, TOK_SEMICOLON,
    TOK_COLON,
    TOK_HASH,        /* # canal fichier */
    TOK_APOSTROPHE,  /* ' separateur PRINT / commentaire */
    TOK_EXCLAMATION, /* ! commentaire de fin de ligne */

    /* Litteraux */
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_STRING,
    TOK_IDENTIFIER,
    TOK_LABEL,       /* etiquette : nom suivi de : */

    /* W: et L: prefixes pour passage de parametres */
    TOK_W_COLON,
    TOK_L_COLON,

    TOK_COUNT
} gfa_token_type;

/* ------------------------------------------------------------------ */
/* Structure d'un token                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    gfa_token_type type;
    int            line;          /* Numero de ligne dans la source   */
    int            column;        /* Colonne dans la ligne            */

    union {
        long    int_value;        /* Valeur entiere                   */
        double  float_value;      /* Valeur flottante                 */
        char   *string_value;     /* Chaine (allouee dynamiquement)   */
        char   *ident_name;       /* Nom d'identifiant                */
    } value;

} gfa_token;

/* ------------------------------------------------------------------ */
/* Messages d'erreur du lexer                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    LEX_OK = 0,
    LEX_ERR_UNEXPECTED_CHAR,
    LEX_ERR_UNTERMINATED_STRING,
    LEX_ERR_INVALID_NUMBER,
    LEX_ERR_HEX_DIGIT_EXPECTED,
    LEX_ERR_BIN_DIGIT_EXPECTED,
    LEX_ERR_OCTAL_DIGIT_EXPECTED,
    LEX_ERR_LINE_TOO_LONG,
    LEX_ERR_STRING_TOO_LONG
} lexer_error;

#ifdef __cplusplus
}
#endif

#endif /* GFA_TOKEN_H */
