/*******************************************************************************
*                                                                              *
*                           Portable Pascal compiler                           *
*                           ************************                           *
*                                                                              *
*                                 Pascal P6                                    *
*                                                                              *
*                                 ETH May 76                                   *
*                                                                              *
* Authors:                                                                     *
*                                                                              *
*    Urs Ammann                                                                *
*    Kesav Nori                                                                *
*    Christian Jacobi                                                          *
*    K. Jensen                                                                 *
*    N. Wirth                                                                  *
*                                                                              *
* Address:                                                                     *
*                                                                              *
*    Institut Fuer Informatik                                                  *
*    Eidg. Technische Hochschule                                               *
*    CH-8096 Zuerich                                                           *
*                                                                              *
*  This code is fully documented in the book                                   *
*        "Pascal Implementation"                                               *
*   by Steven Pemberton and Martin Daniels                                     *
* published by Ellis Horwood, Chichester, UK                                   *
*         ISBN: 0-13-653-0311                                                  *
*       (also available in Japanese)                                           *
*                                                                              *
* Steven Pemberton, CWI/AA,                                                    *
* Kruislaan 413, 1098 SJ Amsterdam, NL                                         *
* Steven.Pemberton@cwi.nl                                                      *
*                                                                              *
* Adaption from P4 to P5 by:                                                   *
*                                                                              *
*    Scott A. Moore                                                            *
*    samiam@moorecad.com                                                       *
*                                                                              *
* Note for the implementation.                                                 *
* ===========================                                                  *
* This interpreter is written for the case where all the fundamental types     *
* take one storage unit.                                                       *
*                                                                              *
* In an actual implementation, the handling of the sp pointer has to take      *
* into account the fact that the types may have lengths different from one:    *
* in push and pop operations the sp has to be increased and decreased not      *
* by 1, but by a number dep}ing on the type concerned.                       *
*                                                                              *
* However, where the number of units of storage has been computed by the       *
* compiler, the value must not be corrected, since the lengths of the types    *
* involved have already been taken into account.                               *
*                                                                              *
* P5 errors added:                                                             *
*                                                                              *
* 182 identifier too long                                                      *
* 183 For index variable must be local to this block                           *
* 184 Interprocedure goto does not reference outter block of destination       *
*                                                                              *
* P5 instructions modified:                                                    *
*                                                                              *
* lca'string'       '                                                          *
*                                                                              *
* was changed to                                                               *
*                                                                              *
* lca 'string'''                                                               *
*                                                                              *
* That is, lca has a space before the opening quote, no longer pads to the     *
* right, and represents single quotes with a quote image. pint converts quote  *
* images back to single quotes, and pads out strings to their full length.     *
*                                                                              *
* In addition, the way files work was extensively modified. Original P5 could  *
* not represent files as fully expressed variables, such as within an array    *
* or record, and were effectively treated as constants. To treat them as true  *
* variable accesses, the stacking order of the file in all file subroutines    *
* was changed so that the file is on the bottom. This matches the source       *
* order of the file in write(f, ...) or read(f, ...). Also, the file           *
* operations now leave the file on the stack for the duration of a write or    *
* read, then dump them using a specific new instruction "dmp". This allows     *
* multiparameter writes and reads to be effectively a chain of single          *
* operations using one file reference. Finally, files were tied to the type    *
* }ing 'a', because files are now full variable references.                  *
*                                                                              *
* Layout of memory in store:                                                   *
*                                                                              *
*    maxstr ->    ---------------------                                        *
*                 | Stack             |                                        *
*        sp ->    ---------------------                                        *
*                 | Free space        |                                        *
*        np ->    ---------------------                                        *
*                 | Heap              |                                        *
*        gbtop -> ---------------------                                        *
*                 | Globals           |                                        *
*        pctop -> ---------------------                                        *
*                 | Constants         |                                        *
*                 ---------------------                                        *
*                 | Code              |                                        *
*                 ---------------------                                        *
*                                                                              *
* The constants are loaded upside down from the top of memory. The heap grows  *
* down, the stack grows up, and when they cross, it is an overflow error.      *
*                                                                              *
* This is the mach (machine) module. This is a cut down interpreter derived    *
* from pint, whose only job is to interpret the code. It loads from prd in     *
* binary deck form, then interprets the code.                                  *
*                                                                              *
* This version translated to C from pmach.pas [saf]                            *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

/*
 * Enable GPC compatability mode
 *
 * Invokes creation and use of the prd (input) and prr (output) files.
 * This is specific to GPC, which handles header files by taking the name of the
 * header file as the literal name of the file.
 *
 */
#define GPC 1
/*
 * Machine word size
 *
 * Supported sizes are 32 and 64.
 *
 */

#define WRDSIZ32 1
/* #define WRDSIZ64 0 */

/*******************************************************************************

Program object sizes and characteristics, sync with pint. These define
the machine specific characteristics of the target.

The configurations are as follows:

type                  #bits 32  #bits 64
===========================================================
integer               32        64
real                  64        64
char                  8         8
boolean               8         8
set                   256       256
pointers              32        64
marks                 32        64
File logical number   8         8

Both endian types are supported. There is no alignment needed, but you
may wish to use alignment to tune the runtime speed.

The machine characteristics dep}ent on byte accessable machines. This
table is all you should need to adapt to any byte addressable machine.

*/

#ifdef WRDSIZ32
#define INTSIZE             4   /* size of integer */
#define INTAL               4   /* alignment of integer */
#define INTDIG              11  /* number of decimal digits in integer */
#define INTHEX              8   /* number of hex digits of integer */
#define REALSIZE            8   /* size of real */
#define REALAL              4   /* alignment of real */
#define CHARSIZE            1   /* size of char */
#define CHARAL              1   /* alignment of char */
#define CHARMAX             1
#define BOOLSIZE            1   /* size of boolean */
#define BOOLAL              1   /* alignment of boolean */
#define PTRSIZE             4   /* size of pointer */
#define ADRSIZE             4   /* size of address */
#define ADRAL               4   /* alignment of address */
#define SETSIZE            32   /* size of set */
#define SETAL               1   /* alignment of set */
#define FILESIZE            1   /* required runtime space for file (lfn) */
#define FILEIDSIZE          1   /* size of the lfn only */
#define EXCEPTSIZE          1   /* size of exception variable */
#define EXCEPTAL            1
#define STACKAL             4   /* alignment of stack */
#define STACKELSIZE         4   /* stack element size */
#define MAXSIZE            32   /* this is the largest type that can be on the
                                   stack */
/* Heap alignment should be either the natural word alignment of the
  machine, or the largest object needing alignment that will be allocated.
  It can also be used to enforce minimum block allocation policy. */
#define HEAPAL              4   /* alignment for each heap arena */
#define GBSAL               4   /* globals area alignment */
#define SETHIGH           255   /* Sets are 256 values */
#define SETLOW              0
#define ORDMAXCHAR        255   /* Characters are 8 bit ISO/IEC 8859-1 */
#define ORDMINCHAR          0
#define MAXRESULT    REALSIZE   /* maximum size of function result */
#define MARKSIZE           32   /* maxresult+6*ptrsize */

/* Value of nil is 1 because this allows checks for pointers that were
  initialized, which would be zero (since we clear all space to zero).
  In the new unified code/data space scheme, 0 and 1 are always invalid
  addresses, since the startup code is at least that long. */
#define NILVAL              1  /* value of 'nil' */

/* Mark element offsets

  Mark format is:

  -8:  Function return value, 64 bits, enables a full real result.
  -12: Static link.
  -16: Dynamic link.
  -20: Saved EP from previous frame.
  -24: Stack bottom after locals allocate. Used for interprocdural gotos.
  -28: EP from current frame. Used for interprocedural gotos.
  -32: Return address

*/
#define MARKFV              -8   /* function value */
#define MARKSL              -12  /* static link */
#define MARKDL              -16  /* dynamic link */
#define MARKEP              -20  /* (old) maximum frame size */
#define MARKSB              -24  /* stack bottom */
#define MARKET              -28  /* current ep */
#define MARKRA              -32  /* return address */
#endif

#ifdef WRDSIZ64
#define INTSIZE             8  /* size of integer */
#define INTAL               4  /* alignment of integer */
#define INTDIG              20 /* number of decimal digits in integer */
#define INTHEX              16 /* number of hex digits of integer */
#define REALSIZE            8  /* size of real */
#define REALAL              4  /* alignment of real */
#define CHARSIZE            1  /* size of char */
#define CHARAL              1  /* alignment of char */
#define CHARMAX             1
#define BOOLSIZE            1  /* size of boolean */
#define BOOLAL              1  /* alignment of boolean */
#define PTRSIZE             8  /* size of pointer */
#define ADRSIZE             8  /* size of address */
#define ADRAL               4  /* alignment of address */
#define SETSIZE            32  /* size of set */
#define SETAL               1  /* alignment of set */
#define FILESIZE            1  /* required runtime space for file (lfn) */
#define FILEIDSIZE          1  /* size of the lfn only */
#define EXCEPTSIZE          1  /* size of exception variable */
#define EXCEPTAL            1
#define STACKAL             4  /* alignment of stack */
#define STACKELSIZE         8  /* stack element size */
#define MAXSIZE            32  /* this is the largest type that can be on the
                                  stack */
/* Heap alignment should be either the natural word alignment of the
  machine, or the largest object needing alignment that will be allocated.
  It can also be used to enforce minimum block allocation policy. */
#define HEAPAL              4  /* alignment for each heap arena */
#define GBSAL               4  /* globals area alignment */
#define SETHIGH           255  /* Sets are 256 values */
#define SETLOW              0
#define ORDMAXCHAR        255  /* Characters are 8 bit ISO/IEC 8859-1 */
#define ORDMINCHAR          0
#define MAXRESULT    REALSIZE  /* maximum size of function result */
#define MARKSIZE           56  /* maxresult+6*ptrsize */

/* Value of nil is 1 because this allows checks for pointers that were
  initialized, which would be zero (since we clear all space to zero).
  In the new unified code/data space scheme, 0 and 1 are always invalid
  addresses, since the startup code is at least that long. */
#define NILVAL              1  /* value of 'nil' */

/* Mark element offsets

  Mark format is:

  -8:  Function return value, 64 bits, enables a full real result.
  -12: Static link.
  -16: Dynamic link.
  -20: Saved EP from previous frame.
  -24: Stack bottom after locals allocate. Used for interprocdural gotos.
  -28: EP from current frame. Used for interprocedural gotos.
  -32: Return address

*/
#define MARKFV              0  /* function value */
#define MARKSL              8  /* static link */
#define MARKDL              16 /* dynamic link */
#define MARKEP              24 /* (old) maximum frame size */
#define MARKSB              32 /* stack bottom */
#define MARKET              40 /* current ep */
#define MARKRA              48 /* return address */
#endif

/* ******************* } of pcom and pint common parameters *********** */

/* internal constants */

#define TRUE 1                /* value of true */
#define FALSE 0               /* value of false */

#define MAXSTR       16777215 /* maximum size of addressing for program/var */
#define MAXTOP       16777216 /* maximum size of addressing for program/var+1 */
#define MAXDEF       2097152  /* maxstr / 8 for defined bits */
#define MAXDIGH      6        /* number of digits in hex representation of maxstr */
#define MAXDIGD      8        /* number of digits in decimal representation of maxstr */
#define MAXAST       100      /* maximum size of assert message */
#define MAXDBF       30       /* size of numeric conversion buffer */
#define MAXCMD       250      /* size of command line buffer */

#define CODEMAX      MAXSTR   /* set size of code store to maximum possible */

#define MAXLABEL  5000        /* total possible labels in intermediate */
#define MAXCSTFX  10000       /* maximum constant fixup in intermediate */
#define MAXGBLFX  10000       /* maximum global access fixup in intermediate */
#define RESSPC    0           /* reserve space in heap (if you want) */

/* locations of header files after program block mark, each header
  file is two values, a file number and a single character buffer */
#define FILRES      2         /* space reserved for file */
#define INPUTOFF    0         /* 'input' file address */
#define OUTPUTOFF   2         /* 'output' file address */
#define PRDOFF      4         /* 'prd' file address */
#define PRROFF      6         /* 'prr' file address */
#define ERROROFF    8         /* 'error' file address */
#define LISTOFF     10        /* 'list' file address */
#define COMMANDOFF  12        /* 'command' file address */

/* assigned logical channels for header files */
#define INPUTFN     1         /* 'input' file no. */
#define OUTPUTFN    2         /* 'output' file no. */
#define PRDFN       3         /* 'prd' file no. */
#define PRRFN       4         /* 'prr' file no. */
#define ERRORFN     5         /* 'error' file no. */
#define LISTFN      6         /* 'list' file no. */
#define COMMANDFN   7         /* 'command' file no. */

/* locations of standard exceptions */
#define EXCEPTIONBASE                       14
#define VALUEOUTOFRANGE                     14
#define ARRAYLENGTHMATCH                    15
#define CASEVALUENOTFOUND                   16
#define ZERODIVIDE                          17
#define INVALIDOPERAND                      18
#define NILPOINTERDEREFERENCE               19
#define REALOVERFLOW                        20
#define REALUNDERFLOW                       21
#define REALPROCESSINGFAULT                 22
#define TAGVALUENOTACTIVE                   23
#define TOOMANYFILES                        24
#define FILEISOPEN                          25
#define FILEALREADYNAMED                    26
#define FILENOTOPEN                         27
#define FILEMODEINCORRECT                   28
#define INVALIDFIELDSPECIFICATION           29
#define INVALIDREALNUMBER                   30
#define INVALIDFRACTIONSPECIFICATION        31
#define INVALIDINTEGERFORMAT                32
#define INTEGERVALUEOVERFLOW                33
#define INVALIDREALFORMAT                   34
#define ENDOFFILE                           35
#define INVALIDFILEPOSITION                 36
#define FILENAMETOOLONG                     37
#define FILEOPENFAIL                        38
#define FILESIZEFAIL                        39
#define FILECLOSEFAIL                       40
#define FILEREADFAIL                        41
#define FILEWRITEFAIL                       42
#define FILEPOSITIONFAIL                    43
#define FILEDELETEFAIL                      44
#define FILENAMECHANGEFAIL                  45
#define SPACEALLOCATEFAIL                   46
#define SPACERELEASEFAIL                    47
#define SPACEALLOCATENEGATIVE               48
#define CANNOTPERFORMSPECIAL                49
#define COMMANDLINETOOLONG                  50
#define READPASTEOF                         51
#define FILETRANSFERLENGTHZERO              52
#define FILESIZETOOLARGE                    53
#define FILENAMEEMPTY                       54
#define CANNOTOPENSTANDARD                  55
#define TOOMANYTEMPORARYFILES               56
#define INPUTBUFFEROVERFLOW                 57
#define TOOMANYTHREADS                      58
#define CANNOTSTARTTHREAD                   59
#define INVALIDTHREADHANDLE                 60
#define CANNOTSTOPTHREAD                    61
#define TOOMANYINTERTASKLOCKS               62
#define INVALIDLOCKHANDLE                   63
#define LOCKSEQUENCEFAIL                    64
#define TOOMANYSIGNALS                      65
#define CANNOTCREATESIGNAL                  66
#define INVALIDSIGNALHANDLE                 67
#define CANNOTDELETESIGNAL                  68
#define CANNOTSENDSIGNAL                    69
#define WAITFORSIGNALFAIL                   70
#define FIELDNOTBLANK                       71
#define READONWRITEONLYFILE                 72
#define WRITEONREADONLYFILE                 73
#define FILEBUFFERVARIABLEUNDEFINED         74
#define NONDECIMALRADIXOFNEGATIVE           75
#define INVALIDARGUMENTTOLN                 76
#define INVALIDARGUMENTTOSQRT               77
#define CANNOTRESETORREWRITESTANDARDFILE    78
#define CANNOTRESETWRITEONLYFILE            79
#define CANNOTREWRITEREADONLYFILE           80
#define SETELEMENTOUTOFRANGE                81
#define REALARGUMENTTOOLARGE                82
#define BOOLEANOPERATOROFNEGATIVE           83
#define INVALIDDIVISORTOMOD                 84
#define PACKELEMENTSOUTOFBOUNDS             85
#define UNPACKELEMENTSOUTOFBOUNDS           86
#define EXCEPTIONTOP                        86

/* Exceptions that can't be caught.
  Note that these don't have associated exception variables. */

#define UNDEFINEDLOCATIONACCESS             87
#define FUNCTIONNOTIMPLEMENTED              88
#define INVALIDINISO7185MODE                89
#define HEAPFORMATINVALID                   90
#define DISPOSEOFUNINITALIZEDPOINTER        91
#define DISPOSEOFNILPOINTER                 92
#define BADPOINTERVALUE                     93
#define BLOCKALREADYFREED                   94
#define INVALIDSTANDARDPROCEDUREORFUNCTION  95
#define INVALIDINSTRUCTION                  96
#define NEWDISPOSETAGSMISMATCH              97
#define PCOUTOFRANGE                        98
#define STOREOVERFLOW                       99
#define STACKBALANCE                        100
#define SETINCLUSION                        101
#define UNINITIALIZEDPOINTER                102
#define DEREFERENCEOFNILPOINTER             103
#define POINTERUSEDAFTERDISPOSE             104
#define VARIANTNOTACTIVE                    105
#define INVALIDCASE                         106
#define SYSTEMERROR                         107
#define CHANGETOALLOCATEDTAGFIELD           108
#define UNHANDLEDEXCEPTION                  109
#define PROGRAMCODEASSERTION                110

#define MAXSP        81   /* number of predefined procedures/functions */
#define MAXINS       255  /* maximum instruction code, 0-255 or byte */
#define MAXFIL       100  /* maximum number of general (temp) files */
#define UJPLEN       5    /* length of ujp instruction (used for case jumps) */
#define FILLEN       2000 /* maximum length of filenames */
#define REALEF       8    /* real extra field in floating format -1.e+000 */

/* check flags: these turn on runtime checks */
#define DOCHKOVF TRUE /* check arithmetic overflow */

/* debug flags: turn these on for various dumps and traces */
#define DOSRCLIN TRUE /* add source line sets to code */
#define DORECYCL TRUE /* obey heap space recycle requests */
/* invoke a special recycle mode that creates single word entries on
  recycle of any object, breaking off and recycling the rest. Once
  allocated, each entry exists forever, and accesses to it can be
  checked. */
#define DOCHKRPT FALSE /* check reuse of freed entry (automatically
                           invokes dorecycl = false */
#define DOCHKDEF TRUE /* check undefined accesses */
#define ISO7185 FALSE /* iso7185 standard flag */

/* version numbers */

#define MAJORVER 0
#define MINORVER 1
#define EXPERIMENT 1

/* typedefs */

/* These equates define the instruction layout. I have choosen a 32 bit
  layout for the instructions defined by (4 bit) digit:

     byte 0:   Instruction code
     byte 1:   P parameter
     byte 2-5: Q parameter

  This means that there are 256 instructions, 256 procedure levels,
  and 2gb of total addressing. This could be 4gb if we get rid of the
  need for negatives. */
typedef unsigned char byte;    /* 8-bit byte */
typedef int boolean;           /* true/false */
typedef byte lvltyp;           /* procedure/function level */
typedef byte instyp;           /* instruction */
typedef int address;           /* address */

typedef char beta[25];         /* error message */
typedef byte settype[SETSIZE]; /* standard set */

typedef int filnum;            /* logical file number */
typedef char filnam[FILLEN];   /* filename strings */
typedef enum {
  fsclosed,
  fsread,
  fswrite
} filsts;                      /* file states */
typedef int cmdinx;            /* index for command line buffer */
typedef int cmdnum;            /* length of command line buffer */
typedef char cmdbuf[MAXCMD];   /* buffer for command line */

/**************************** Global Variables ********************************/

address pc;      /*program address register*/
address pctop;   /* top of code store */
address gbtop;   /* top of globals, size of globals */
instyp op; lvltyp p; address q;  /*instruction register*/
address q1; /* extra parameter */
byte store[MAXSTR]; /* complete program storage */
byte storedef[MAXDEF]; /* defined bits */
int sdi; /* index for that */
/* mp  points to {ning of a data segment
   sp  points to top of the stack
   ep  points to the maximum extent of the stack
   np  points to top of the dynamically allocated area */
address mp,sp,np,ep;  /* address registers */
address expadr; /* exception address of exception handler starts */
address expstk; /* exception address of sp at handlers */
address expmrk; /* exception address of mp at handlers */

byte bitmsk[8]; /* bits in byte */

int     srclin;  /* current source line executing */
cmdbuf  cmdlin;  /* command line */
cmdnum  cmdlen;  /* length of command line */
cmdinx  cmdpos;  /* current position in command line */
boolean stopins; /* stop instruction executed */

FILE* filtable[MAXFIL+1]; /* general file holders */
filnam filnamtab[MAXFIL+1]; /* assigned name of files */
boolean filanamtab[MAXFIL+1]; /* name has been assigned flags */
filsts filstate[MAXFIL+1]; /* file state holding */
boolean filbuff[MAXFIL+1]; /* file buffer full status */

int i;
char c1;
address ad;
int bai;

/*--------------------------------------------------------------------*/

/* Low level error check and handling */

void finish(int e)
{
    for (i = 1; i < MAXFIL; i++) if (filstate[i] != fsclosed) fclose(filtable[i]);
    printf("\n");
    if (e) printf("Program aborted\n");
    printf("program complete\n");
    exit(e);
}

void errors(address a, address l)
{ printf("\n*** Runtime error\n");
      if (srclin > 0) printf(" [%d]: ", srclin);
      if (l > MAXAST) l = MAXAST;
      while (l > 0) { printf("%c", store[a]); a = a+1; l = l-1; }
      finish(1);
} /*errori*/

/* Error handling:

  To throw a standard exception, errore() is used. To bypass the interceptable
  exceptions and directly print, use errorv().

*/

/* handle exception vector */
void errorv(address ea)
{ printf("\n*** Runtime error");
  if (srclin > 0) printf(" [%d]: ", srclin);
  switch (ea) {

    /* Exceptions that can be intercepted */
    case VALUEOUTOFRANGE:                    printf("Value out of range\n"); break;
    case ARRAYLENGTHMATCH:                   printf("Array length match\n"); break;
    case CASEVALUENOTFOUND:                  printf("Case value not found\n"); break;
    case ZERODIVIDE:                         printf("Zero divide\n"); break;
    case INVALIDOPERAND:                     printf("Invalid operand\n"); break;
    case NILPOINTERDEREFERENCE:              printf("Nil pointer dereference\n"); break;
    case REALOVERFLOW:                       printf("Real overflow\n"); break;
    case REALUNDERFLOW:                      printf("Real underflow\n"); break;
    case REALPROCESSINGFAULT:                printf("Real processing fault\n"); break;
    case TAGVALUENOTACTIVE:                  printf("Tag value not active\n"); break;
    case TOOMANYFILES:                       printf("Too many files\n"); break;
    case FILEISOPEN:                         printf("File is open\n"); break;
    case FILEALREADYNAMED:                   printf("File already named\n"); break;
    case FILENOTOPEN:                        printf("File not open\n"); break;
    case FILEMODEINCORRECT:                  printf("File mode incorrect\n"); break;
    case INVALIDFIELDSPECIFICATION:          printf("Invalid field specification\n"); break;
    case INVALIDREALNUMBER:                  printf("Invalid real number\n"); break;
    case INVALIDFRACTIONSPECIFICATION:       printf("Invalid fraction specification\n"); break;
    case INVALIDINTEGERFORMAT:               printf("Invalid integer format\n"); break;
    case INTEGERVALUEOVERFLOW:               printf("Integer value overflow\n"); break;
    case INVALIDREALFORMAT:                  printf("Invalid real format\n"); break;
    case ENDOFFILE:                          printf("End of file\n"); break;
    case INVALIDFILEPOSITION:                printf("Invalid file position\n"); break;
    case FILENAMETOOLONG:                    printf("Filename too long\n"); break;
    case FILEOPENFAIL:                       printf("File open fail\n"); break;
    case FILESIZEFAIL:                       printf("File size fail\n"); break;
    case FILECLOSEFAIL:                      printf("File close fail\n"); break;
    case FILEREADFAIL:                       printf("File read fail\n"); break;
    case FILEWRITEFAIL:                      printf("File write fail\n"); break;
    case FILEPOSITIONFAIL:                   printf("File position fail\n"); break;
    case FILEDELETEFAIL:                     printf("File delete fail\n"); break;
    case FILENAMECHANGEFAIL:                 printf("File name change fail\n"); break;
    case SPACEALLOCATEFAIL:                  printf("Space allocate fail\n"); break;
    case SPACERELEASEFAIL:                   printf("Space release fail\n"); break;
    case SPACEALLOCATENEGATIVE:              printf("Space allocate negative\n"); break;
    case CANNOTPERFORMSPECIAL:               printf("Cannot perform special\n"); break;
    case COMMANDLINETOOLONG:                 printf("Command line too long\n"); break;
    case READPASTEOF:                        printf("Read past eof\n"); break;
    case FILETRANSFERLENGTHZERO:             printf("File transfer length zero\n"); break;
    case FILESIZETOOLARGE:                   printf("File size too large\n"); break;
    case FILENAMEEMPTY:                      printf("Filename empty\n"); break;
    case CANNOTOPENSTANDARD:                 printf("Cannot open standard\n"); break;
    case TOOMANYTEMPORARYFILES:              printf("Too many temporary files\n"); break;
    case INPUTBUFFEROVERFLOW:                printf("Input buffer overflow\n"); break;
    case TOOMANYTHREADS:                     printf("Too many threads\n"); break;
    case CANNOTSTARTTHREAD:                  printf("Cannot start thread\n"); break;
    case INVALIDTHREADHANDLE:                printf("Invalid thread handle\n"); break;
    case CANNOTSTOPTHREAD:                   printf("Cannot stop thread\n"); break;
    case TOOMANYINTERTASKLOCKS:              printf("Too many inter task locks\n"); break;
    case INVALIDLOCKHANDLE:                  printf("Invalid lock handle\n"); break;
    case LOCKSEQUENCEFAIL:                   printf("Lock sequence fail\n"); break;
    case TOOMANYSIGNALS:                     printf("Too many signals\n"); break;
    case CANNOTCREATESIGNAL:                 printf("Cannot create signal\n"); break;
    case INVALIDSIGNALHANDLE:                printf("Invalid signal handle\n"); break;
    case CANNOTDELETESIGNAL:                 printf("Cannot delete signal\n"); break;
    case CANNOTSENDSIGNAL:                   printf("Cannot send signal\n"); break;
    case WAITFORSIGNALFAIL:                  printf("Wait for signal fail\n"); break;
    case FIELDNOTBLANK:                      printf("Field not blank\n"); break;
    case READONWRITEONLYFILE:                printf("Read on write only file\n"); break;
    case WRITEONREADONLYFILE:                printf("Write on read only file\n"); break;
    case FILEBUFFERVARIABLEUNDEFINED:        printf("File buffer variable undefined\n"); break;
    case NONDECIMALRADIXOFNEGATIVE:          printf("Nondecimal radix of negative\n"); break;
    case INVALIDARGUMENTTOLN:                printf("Invalid argument to ln\n"); break;
    case INVALIDARGUMENTTOSQRT:              printf("Invalid argument to sqrt\n"); break;
    case CANNOTRESETORREWRITESTANDARDFILE:   printf("Cannot reset or rewrite standard file\n"); break;
    case CANNOTRESETWRITEONLYFILE:           printf("Cannot reset write only file\n"); break;
    case CANNOTREWRITEREADONLYFILE :         printf("Cannot rewrite read only file\n"); break;
    case SETELEMENTOUTOFRANGE:               printf("Set element out of range\n"); break;
    case REALARGUMENTTOOLARGE:               printf("Real argument too large\n"); break;
    case BOOLEANOPERATOROFNEGATIVE:          printf("Boolean operator of negative\n"); break;
    case INVALIDDIVISORTOMOD:                printf("Invalid divisor to mod\n"); break;
    case PACKELEMENTSOUTOFBOUNDS:            printf("Pack elements out of bounds\n"); break;
    case UNPACKELEMENTSOUTOFBOUNDS:          printf("Unpack elements out of bounds\n"); break;

    /* Exceptions that can't be intercepted */
    case UNDEFINEDLOCATIONACCESS:            printf("Undefined location access\n"); break;
    case FUNCTIONNOTIMPLEMENTED:             printf("Function not implemented\n"); break;
    case INVALIDINISO7185MODE:               printf("Invalid in ISO 7185 mode\n"); break;
    case HEAPFORMATINVALID:                  printf("Heap format invalid\n"); break;
    case DISPOSEOFUNINITALIZEDPOINTER:       printf("Dispose of uninitalized pointer\n"); break;
    case DISPOSEOFNILPOINTER:                printf("Dispose of nil pointer\n"); break;
    case BADPOINTERVALUE:                    printf("Bad pointer value\n"); break;
    case BLOCKALREADYFREED:                  printf("Block already freed\n"); break;
    case INVALIDSTANDARDPROCEDUREORFUNCTION: printf("Invalid standard procedure or function\n"); break;
    case INVALIDINSTRUCTION:                 printf("Invalid instruction\n"); break;
    case NEWDISPOSETAGSMISMATCH:             printf("New dispose tags mismatch\n"); break;
    case PCOUTOFRANGE:                       printf("Pc out of range\n"); break;
    case STOREOVERFLOW:                      printf("Store overflow\n"); break;
    case STACKBALANCE:                       printf("Stack balance\n"); break;
    case SETINCLUSION:                       printf("Set inclusion\n"); break;
    case UNINITIALIZEDPOINTER:               printf("Uninitialized pointer\n"); break;
    case DEREFERENCEOFNILPOINTER:            printf("Dereference of nil pointer\n"); break;
    case POINTERUSEDAFTERDISPOSE:            printf("Pointer used after dispose\n"); break;
    case VARIANTNOTACTIVE:                   printf("Variant not active\n"); break;
    case INVALIDCASE:                        printf("Invalid case\n"); break;
    case SYSTEMERROR:                        printf("System error\n"); break;
    case CHANGETOALLOCATEDTAGFIELD:          printf("Change to allocated tag field\n"); break;
    case UNHANDLEDEXCEPTION:                 printf("Unhandled exception\n"); break;
    case PROGRAMCODEASSERTION:               printf("Program code assertion\n"); break;
  }
  finish(1);
}

void errorm(address ea)
{
  /* check is a standard exception */
  if (ea-pctop >= EXCEPTIONBASE &&
      ea-pctop <= EXCEPTIONTOP) errorv(ea-pctop);
  else errorv(UNHANDLEDEXCEPTION);
}

/* get bit from defined array */

boolean getdef(address a)

{
    byte b; boolean r;

    if (DOCHKDEF) { b = storedef[a / 8]; r = b / bitmsk[a % 8]&1; }
    else r = TRUE; /* always set defined */
    return(r);
}

/* put bit to defined array */

void putdef(address a, boolean b)

{
    byte sb; boolean r;

    if (DOCHKDEF) {
      sb = storedef[a/8]; /* get byte */
      /* test bit as is */
      r = sb / bitmsk[a % 8]&1;
      if (r != b) {
        if (b) sb = sb+bitmsk[a%8];
        else sb = sb-bitmsk[a%8];
        storedef[a/8] = sb;
      }
    }
}

void chkdef(address a)
{
   if (DOCHKDEF) if (!getdef(a)) errorv(UNDEFINEDLOCATIONACCESS);
}

/* Command line processing */

void getcommandline(int argc, char* argv[], cmdbuf cb, cmdnum* l)
{
    cmdinx i;
    int x;
    char *p;

    for (i = 0; i < MAXCMD; i++) cb[i] = ' '; i = 0;
    while (argc) { /* walk input parameters and concatenate */
        p = *argv;
        while (*p) {
            if (i >= MAXCMD) {
                fprintf(stderr,
                        "*** Too many/too long command line parameters\n");
                finish(1);
            }
            cb[i++] = *p++;
        }
        argc--; argv++; /* advance */
        if (argc) { /* still more */
            if (i >= MAXCMD) {
                fprintf(stderr,
                        "*** Too many/too long command line parameters\n");
                finish(1);
            }
            cb[i++] = ' ';
        }
    }
}

/*--------------------------------------------------------------------*/

/* "fileofy" routines for command line processing.

  These routines implement the command header file by reading from a
  buffer where the command line is stored. The assumption here is that
  there is a fairly simple call to retrieve the command line.

  If it is wanted, the command file primitives can be used to implement
  another type of interface, say, reading from an actual file.

  The command buffer is designed to never be completely full.
  The last two locations indicate:

  maxcmd: end of file
  maxcmd-1: end of line

  These locations are always left as space, thus eoln returns space as
  the standard specifies.
*/

char bufcommand(void)
{ return (cmdlin[cmdpos]); }

void getcommand(void)
{ if (cmdpos <= cmdlen) cmdpos = cmdpos+1; }

boolean eofcommand(void)
{ return (cmdpos > cmdlen+1); }

boolean eolncommand(void)
{ return (cmdpos >= cmdlen+1); }

void readlncommand(void)
{ cmdpos = MAXCMD; }

/* The external assigns read a filename off the command line. The original name
  of the header file is also passed in, and can be used to process. However,
  this implementation ignores them and just reads the names in order off the
  command line (as provided for in Annex C of the Pascaline standard).

  The processing of command line filenames does not exclude the use of the
  command file. The command file simply starts reading after all filename
  parameters have been removed.

*/

void assignexternal(filnum fn, char hfn[])
{
    int i;

    for (i = 0; i < FILLEN; i++) { filnamtab[fn][i] = ' '; }
    /* skip leading spaces */
    while (!eolncommand() && !eofcommand() && bufcommand() == ' ') getcommand();
    i = 0;
    while (!eolncommand() && !eofcommand() &&
           (isalnum(bufcommand()) || bufcommand() == '_')) {
        if (i >= FILLEN) errorv(FILENAMETOOLONG);
        filnamtab[fn][i] = bufcommand();
        getcommand();
        i = i+1;
    }
    if (i >= FILLEN) errorv(FILENAMETOOLONG);
    filnamtab[fn][i] = 0; /* terminate */
}

/*--------------------------------------------------------------------*/

/* Accessor functions

  These translate store variables to internal, and convert to and from store RAM
  formats.

  The acessors are fairly machine independent, they rely here on the machine
  being byte addressable. The endian format is inherent to the machine.

  The exception are the get/put int8,16,32,64 and 128 bit routines, which are
  dependent on the endian mode of the machine.

*/

int getint(address a) { chkdef(a); return (*((int*)(store+a))); }
void putint(address a, int x) { *((int*)(store+a)) = x; putdef(a, TRUE); }
float getrel(address a) { chkdef(a); return (*((float*)(store+a))); }
void putrel(address a, float f) { *((float*)(store+a)) = f; putdef(a, TRUE); }
boolean getbol(address a) { chkdef(a); return (store[a]); }
void putbol(address a, boolean b) { store[a] = b; putdef(a, TRUE); }
char getchr(address a) { chkdef(a); return (store[a]); }
void putchr(address a, char c) { store[a] = c; putdef(a, TRUE); }
byte getbyt(address a) { chkdef(a); return (store[a]); }
void putbyt(address a, byte b) { store[a] = b; putdef(a, TRUE); }
address getadr(address a) { chkdef(a); return (*((address*)(store+a))); }
void putadr(address a, address ad) { *((address*)(store+a)) = ad;
                                     putdef(a, TRUE); }

void getset(address a, settype s)

{
    int i;

    chkdef(a);
    for (i = 0; i < SETSIZE; i++) s[i] = store[a+i];
}

void putset(address a, settype s)

{
   int i;

   for (i = 0; i < SETSIZE; i++) store[a+i] = s[i];
   putdef(a, TRUE);
}

/* Swap pointer on top with second on stack. The size of the second is given. */

void swpstk(address l)

{
    byte sb[MAXSIZE];
    address p;
    int i;

   /* get the top pointer */
   p = getadr(sp);
   /* load up the second on stack */
   for (i = 0; i < l; i++) sb[i] = store[sp+ADRSIZE+i];
   putadr(sp+l, p); /* place pointer at bottom */
   for (i = 0; i < l; i++) {
     store[sp+i] = sb[i]; /* place second as new top */
     putdef(sp+i, TRUE);
   }
}

/* end of accessor functions */

/*--------------------------------------------------------------------*/

/* Push/pop

  These routines handle both the data type, and their lengths on the stack.

*/

void popint(int* i) { *i = getint(sp); sp = sp+INTSIZE; }
void pshint(int i) { sp = sp-INTSIZE; putint(sp, i); }
void poprel(float* r) { *r = getrel(sp); sp = sp+REALSIZE; }
void pshrel(float r) { sp = sp-REALSIZE; putrel(sp, r); }
void popset(settype s) { getset(sp, s); sp = sp+SETSIZE; }
void pshset(settype s) { sp = sp-SETSIZE; putset(sp, s); }
void popadr(address* a) { *a = getadr(sp); sp = sp+ADRSIZE; }
void pshadr(address a) { sp = sp-ADRSIZE; putadr(sp, a); }

/*--------------------------------------------------------------------*/

/* set operations */

void sset(settype s, int b)

{
    int i;

    for (i = 0; i < SETSIZE; i++) s[i] = 0;
    s[b/8] = 1<<b%8;
}

void rset(settype s, int b1, int b2)

{
    int i;

    for (i = 0; i < SETSIZE; i++) s[i] = 0;
    if (b1 > b2) { i = b1; b1 = b2; b2 = i; }
    for (i = b1; i <= b2; i++) s[i/8] = 1<<i%8;
}

void suni(settype s1, settype s2)

{
    int i;

    for (i = 0; i < SETSIZE; i++) s1[i] = s1[i] | s2[i];
}

void sint(settype s1, settype s2)

{
    int i;

    for (i = 0; i < SETSIZE; i++) s1[i] = s1[i] & s2[i];
}

void sdif(settype s1, settype s2)

{
    int i;

    for (i = 0; i < SETSIZE; i++) s1[i] = ~s1[i] & s2[i];
}

boolean sisin(int i, settype s)

{
    !!(s[i/8] & 1<<i%8);
}

/* throw an exception by vector */
void errore(int ei)
{
    address ad;

    if (expadr == 0) errorm(pctop+ei); /* no surrounding frame, throw system */
    mp = expmrk; sp = expstk; pc = expadr; popadr(&ad); pshadr(pctop+ei);
    ep = getadr(mp+MARKET); /* get the mark ep */
}

/* align address, upwards */

void alignu(address algn, address* flc)
{
    int l;
    l = *flc-1;
    *flc = l+algn-(algn+l)%algn;
} /*align*/

/* clear filename string */

void clrfn(filnam fn)
{ int i; for (i = 0; i < FILLEN; i++) fn[i] = ' '; }

/*--------------------------------------------------------------------*/

/* load code file */

void errorl(void) /*error in loading*/
{ printf("\n*** Invalid code deck\n"); finish(1); }

void load(FILE* fp)

{
    address ad, ad2;
    int i, l, cs, csc, b;
    int c;

    ad = 0; l = 1;
    while ((c = fgetc(fp)) != EOF && l > 0) {
        if (c != ':') errorl();
        fscanf(fp, "%2x%16x", &l, &i); ad2 = i;
        if (ad != ad2 && l > 0) errorl();
        cs = 0;
        for (i = 1; i <= l; i++) {
            fscanf(fp, "%2x", &b); putbyt(ad, b); cs = (cs+b)%256;
            ad = ad+1;
        };
        fscanf(fp, "%2x\n", &csc); if (cs != csc) errorl();
    }
    pctop = ad;
} /*load*/

/*------------------------------------------------------------------------*/

/* runtime handlers */

address base(int ld)
{
    address ad;

    ad = mp;
    while (ld>0) { ad = getadr(ad+MARKSL); ld = ld-1; }
    return (ad);
} /*base*/

void compare(boolean* b, address* a1, address* a2)
/*comparing is only correct if result by comparing integers will be*/
{
    int i;

    i = 0; *b = TRUE;
    while (*b && i<q) {
        chkdef(*a1+i); chkdef(*a2+i);
        if (store[*a1+i] = store[*a2+i]) i = i+1;
        else *b = FALSE;
    }
    if (i == q) i = i-1; /* point at last location */
    *a1 = *a1+i; *a2 = *a2+i;
} /*compare*/

void valfil(address fa) /* attach file to file entry */
{
    int i,ff;

    if (store[fa] == 0) { /* no file */
        if (fa == pctop+INPUTOFF) ff = INPUTFN;
        else if (fa == pctop+OUTPUTOFF) ff = OUTPUTFN;
        else if (fa == pctop+PRDOFF) ff = PRDFN;
        else if (fa == pctop+PRROFF) ff = PRRFN;
        else if (fa == pctop+ERROROFF) ff = ERRORFN;
        else if (fa == pctop+LISTOFF) ff = LISTFN;
        else if (fa == pctop+COMMANDOFF) ff = COMMANDFN;
        else {
            i = COMMANDFN+1; /* start search after the header files */
            ff = 0;
            while (i <= MAXFIL) {
                if (filstate[i] == fsclosed) { ff = i; i = MAXFIL; }
                i = i+1;
            }
            if (ff == 0) errore(TOOMANYFILES);
        }
        store[fa] = ff; putdef(fa, TRUE);
    }
}

void valfilwm(address fa) /* validate file write mode */
{
    valfil(fa); /* validate file address */
    if (filstate[store[fa]] != fswrite) errore(FILEMODEINCORRECT);
}

void valfilrm(address fa) /* validate file read mode */
{
    valfil(fa); /* validate file address */
    if (filstate[store[fa]] != fsread) errore(FILEMODEINCORRECT);
}

void getop(void) /* get opcode */
{ op = store[pc]; pc = pc+1; }
void getp(void) /* get p parameter */
{ p = store[pc]; pc = pc+1; }
void getq(void) /* get q parameter */
{ q = getadr(pc); pc = pc+ADRSIZE; }
void getq1(void) /* get q1 parameter */
{ q1 = getadr(pc); pc = pc+ADRSIZE; }

/*

   Blocks in the heap are dead simple. The block begins with a length, including
   the length itself. If the length is positive, the block is free. If negative,
   the block is allocated. This means that AddressOfBLock+abs(lengthOfBlock) is
   address of the next block, and RequestedSize <= LengthOfBLock+adrsize is a
   reasonable test for if a free block fits the requested size, since it will
   never be true of occupied blocks.

*/

/* find free block using length */

void fndfre(address len, address* blk)
{
    address l, b;

    b = 0; /* set no block found */
    *blk = gbtop; /* set to bottom of heap */
    while (*blk < np) { /* search blocks in heap */
        l = getadr(*blk); /* get length */
        if (abs(l) < HEAPAL || abs(l) > np) errorv(HEAPFORMATINVALID);
        if (l >= len+ADRSIZE) { b = *blk; *blk = np; } /* found */
        else *blk = *blk+abs(l); /* go next block */
    }
    if (b > 0) { /* block was found */
        putadr(b, -l); /* allocate block */
        *blk = b+ADRSIZE; /* set base address */
        if (l > len+ADRSIZE+ADRSIZE+RESSPC) {
            /* If there is enough room for the block, header, and another
               header, then a reserve factor if desired. */
            putadr(b, -(len+ADRSIZE)); /* allocate block */
            b = b+len+ADRSIZE; /* go to top of allocated block */
            putadr(b, l-(len+ADRSIZE)); /* set length of stub space */
        }
    } else *blk = 0; /* set no block found */
}

/* coalesce space in heap */

void cscspc(void)
{
    boolean done;
    address ad, ad1, l, l1;

   /* first, colapse all free blocks at the heap top */
   l = 0;
   while (l >= 0 && np > gbtop) {
        /* find last entry */
        ad = gbtop;
        while (ad < np) { ad1 = ad; ad = ad+abs(getadr(ad)); }
        l = getadr(ad1); /* get header length */
        if (l >= 0) np = ad1; /* release to free space */
    }
    /* now, walk up and collapse adjacent free blocks */
    ad = gbtop; /* index bottom */
    while (ad < np) {
        l = getadr(ad); /* get header length */
        if (l >= 0) { /* free */
            ad1 = ad+l; /* index next block */
            if (ad1 < np) { /* not against end */
                l1 = getadr(ad1); /* get length next */
                if (l1 >=0) /* both blocks are free, combine the blocks */
                    putadr(ad, l+l1);
                else ad = ad+l+abs(l1); /* skip both blocks */
            } else ad = ad1; /* skip to end, done */
        } else ad = ad+abs(l); /* this block is not free, skip it */
    }
}

/* allocate space in heap */

void newspc(address len, address* blk)
{
    address ad,ad1;

    alignu(ADRSIZE, &len); /* align to units of address */
    fndfre(len, blk); /* try finding an existing free block */
    if (*blk == 0) { /* allocate from heap top */
        ad = np; /* save base of new block */
        np = np+(len+ADRSIZE); /* find new heap top */
        ad1 = np; /* save address */
        alignu(HEAPAL, &np); /* align to arena */
        len = len+(np-ad1); /* adjust length upwards for alignment */
        if (np > sp) errore(SPACEALLOCATEFAIL);
        putadr(ad, -(len+ADRSIZE)); /* allocate block */
        *blk = ad+ADRSIZE; /* index start of block */
    }
    /* clear block and set undefined */
    for (ad = *blk; ad < *blk+len; ad++) { store[ad] = 0; putdef(ad, FALSE); }
}

/* dispose of space in heap */

void dspspc(address len, address blk)
{
    address ad;

   if (blk == 0) errorv(DISPOSEOFUNINITALIZEDPOINTER);
   else if (blk == NILVAL) errorv(DISPOSEOFNILPOINTER);
   else if (blk < gbtop || blk >= np) errorv(BADPOINTERVALUE);
   ad = blk-ADRSIZE; /* index header */
   if (getadr(ad) >= 0) errorv(BLOCKALREADYFREED);
   if (DORECYCL && !DOCHKRPT) { /* obey recycling requests */
        putadr(ad, abs(getadr(ad))); /* set block free */
        cscspc(); /* coalesce free space */
   } else if (DOCHKRPT) { /* perform special recycle */
        /* check can break off top block */
        len = abs(getadr(ad)); /* get length */
        if (len >= ADRSIZE*2) putadr(ad+ADRSIZE, abs(getadr(ad))-ADRSIZE);
            /* the "marker" is a block with a single address. Since it can't
               hold more than that, it will never be reused */
        putadr(ad, ADRSIZE); /* indicate freed but fixed block */
    }
}

/* check pointer indexes free entry */

boolean isfree(address blk)
{   return (getadr(blk-ADRSIZE) == ADRSIZE); }


/* system routine call */

void readline(FILE* f)
{
    int c;

    do { c = fgetc(f); } while (c != EOF && c != '\n');
    if (c == EOF) errore(ENDOFFILE);
}

boolean eoffile(FILE* fp)
{ int c; c = fgetc(fp); if (c != EOF) ungetc(c, fp); return (c == EOF); }

boolean eolnfile(FILE* fp)
{ int c; c = fgetc(fp); if (c != EOF) ungetc(c, fp); return (c == '\n'); }

int filelength(FILE* fp)
{
    int s, p;

    s = ftell(fp); fseek(fp, 0, SEEK_END);
    p = ftell(fp); fseek(fp, s, SEEK_SET);

    return (p);
}

boolean eoffns(filnum fn)
{
    boolean eoft;

    if (filstate[fn] == fswrite)
        eoft = ftell(filtable[fn]) >= filelength(filtable[fn]);
    else if (filstate[fn] == fsread) eoft = eoffile(filtable[fn]);
    else errore(FILENOTOPEN);
}

char buffn(filnum fn)
{
    int c;

    if (fn <= COMMANDFN) switch(fn) {
        case INPUTFN:   c = fgetc(stdin); ungetc(c, stdin); break;
        case PRDFN:     c = fgetc(filtable[PRDFN]); ungetc(c, filtable[PRDFN]);
                        break;
        case OUTPUTFN: case PRRFN: case ERRORFN:
        case LISTFN:    errore(READONWRITEONLYFILE); break;
        case COMMANDFN: c = bufcommand(); break;
    } else {
        if (filstate[fn] != fsread) errore(FILEMODEINCORRECT);
        c = fgetc(filtable[fn]); ungetc(c, filtable[fn]);
    }

    return (c);
}

void getfn(filnum fn)
{
    if (fn <= COMMANDFN) switch (fn) {
        case INPUTFN:   fgetc(stdin); break;
        case PRDFN:     fgetc(filtable[PRDFN]); break;
        case OUTPUTFN: case PRRFN: case ERRORFN:
        case LISTFN:    errore(READONWRITEONLYFILE); break;
        case COMMANDFN: getcommand(); break;
    } else {
        if (filstate[fn] != fsread) errore(FILEMODEINCORRECT);
        fgetc(filtable[fn]);
    }
}

boolean eoffn(filnum fn)
{
    boolean eof;

    if (fn <= COMMANDFN) switch (fn) {
        case INPUTFN:   eof = eoffile(stdin); break;
        case OUTPUTFN:  eof = TRUE; break;
        case PRDFN:     eof = eoffns(PRDFN); break;
        case PRRFN:     eof = eoffns(PRRFN); break;
        case ERRORFN:   eof = TRUE; break;
        case LISTFN:    eof = TRUE; break;
        case COMMANDFN: eof = eofcommand(); break;
    } else eof = eoffns(fn);

    return (eof);
}

boolean eolnfn(filnum fn)
{
    boolean eoln;

    if (fn <= COMMANDFN) switch (fn) {
        case INPUTFN:   eoln = eolnfile(stdin);
        case PRDFN:     eoln = eolnfile(filtable[PRDFN]);
        case PRRFN:     eoln = eolnfile(filtable[PRRFN]);
        case ERRORFN: case OUTPUTFN:
        case LISTFN:    errore(FILEMODEINCORRECT);;
        case COMMANDFN: eoln = eolncommand();
    } else {
        if (filstate[fn] == fsclosed) errore(FILENOTOPEN);
        eoln = eolnfile(filtable[fn]);
    }

    return (eoln);
}

char chkbuf(filnum fn, int w)
{ if (w > 0) return buffn(fn); else return(' '); }

boolean chkend(filnum fn, int w)
{ return (w = 0 || eoffn(fn)); }

void getbuf(filnum fn, int* w)
{
  if (*w > 0) {
    if (eoffn(fn)) errore(ENDOFFILE);
    getfn(fn); *w = *w-1;
  }
}

void readi(filnum fn, int *i, int* w, boolean fld)
{
    int s;
    int d;

   s = +1; /* set sign */
   /* skip leading spaces */
   while (chkbuf(fn, *w) == ' ' && !chkend(fn, *w)) getbuf(fn, w);
   if (!(chkbuf(fn, *w) == '+' || chkbuf(fn, *w) == '-' ||
         isdigit(chkbuf(fn, *w)))) errore(INVALIDINTEGERFORMAT);
   if (chkbuf(fn, *w) == '+') getbuf(fn, w);
   else if (chkbuf(fn, *w) == '-') { getbuf(fn, w); s = -1; }
   if (!(isdigit(chkbuf(fn, *w))))
     errore(INVALIDINTEGERFORMAT);
   *i = 0; /* clear initial value */
   while (isdigit(chkbuf(fn, *w))) { /* parse digit */
     d = chkbuf(fn, *w)-'0';
     if (*i > INT_MAX/10 ||
         *i == INT_MAX/10 && d > INT_MAX%10)
       errore(INTEGERVALUEOVERFLOW);
     *i = *i*10+d; /* add in new digit */
     getbuf(fn, w);
   }
   *i = *i*s; /* place sign */
   /* if fielded, validate the rest of the field is blank */
   if (fld) while (!chkend(fn, *w)) {
     if (chkbuf(fn, *w) != ' ') errore(FIELDNOTBLANK);
     getbuf(fn, w);
   }
}

/* find power of ten */
float pwrten(int e)
{
    float t; /* accumulator */
    float p; /* current power */

   p = 1.0e+1; /* set 1st power */
   t = 1.0; /* initalize result */
   do {
      if (e&1) t = t*p; /* if bit set, add this power */
      e = e / 2; /* index next bit */
      p = p*p; /* find next power */
   } while (e != 0);
   return (t);
}

void readr(filnum fn, float* r, int w, boolean fld)
{
    int i; /* integer holding */
    int e; /* exponent */
    int d; /* digit */
    boolean s; /* sign */

   e = 0; /* clear exponent */
   s = FALSE; /* set sign */
   *r = 0.0; /* clear result */
   /* skip leading spaces */
   while (chkbuf(fn, w) == ' ' && !chkend(fn, w)) getbuf(fn, &w);
   /* get any sign from number */
   if (chkbuf(fn, w) == '-') { getbuf(fn, &w); s = TRUE; }
   else if (chkbuf(fn, w) == '+') getbuf(fn, &w);
   if (!(isdigit(chkbuf(fn, w)))) errore(INVALIDREALNUMBER);
   while (isdigit(chkbuf(fn, w))) { /* parse digit */
      d = chkbuf(fn, w)-'0';
      *r = *r*10+d; /* add in new digit */
      getbuf(fn, &w);
   }
   if (chkbuf(fn, w) == '.' || tolower(chkbuf(fn, w)) == 'e') { /* it's a real */
      if (chkbuf(fn, w) == '.') { /* decimal point */
         getbuf(fn, &w); /* skip '.' */
         if (!(isdigit(chkbuf(fn, w)))) errore(INVALIDREALNUMBER);
         while (isdigit(chkbuf(fn, w))) { /* parse digit */
            d = chkbuf(fn, w)-'0';
            *r = *r*10+d; /* add in new digit */
            getbuf(fn, &w);
            e = e-1; /* count off right of decimal */
         }
      }
      if (tolower(chkbuf(fn, w)) == 'e') { /* exponent */
         getbuf(fn, &w); /* skip 'e' */
         if (!(isdigit(chkbuf(fn, w)) || chkbuf(fn, w) == '+' ||
               chkbuf(fn, w) == '-'))
            errore(INVALIDREALNUMBER);
         readi(fn, &i, &w, fld); /* get exponent */
         /* find with exponent */
         e = e+i;
      }
      if (e < 0) *r = *r/pwrten(e); else *r = *r*pwrten(e);
   }
   if (s) *r = -*r;
   /* if fielded, validate the rest of the field is blank */
   if (fld) while (!chkend(fn, w)) {
     if (chkbuf(fn, w) != ' ') errore(FIELDNOTBLANK);
     getbuf(fn, &w);
   }
}

void readc(filnum fn, char* c, int w, boolean fld)
{
   *c = chkbuf(fn, w); getbuf(fn, &w);
   /* if fielded, validate the rest of the field is blank */
   if (fld) while (!chkend(fn, w)) {
     if (chkbuf(fn, w) != ' ') errore(FIELDNOTBLANK);
     getbuf(fn, &w);
   }
} /*readc*/

void reads(filnum fn, address ad, int l, int w, boolean fld)
{
  int c;
  while (l > 0) {
    c = chkbuf(fn, w); getbuf(fn, &w); putchr(ad, c); ad = ad+1; l = l-1;
  }
  /* if fielded, validate the rest of the field is blank */
  if (fld) while (!chkend(fn, w)) {
    if (chkbuf(fn, w) != ' ') errore(FIELDNOTBLANK);
    getbuf(fn, &w);
  }
} /*reads*/

void readsp(filnum fn, address ad,  int l)
{
  char c;

  while (l > 0 && !eolnfn(fn)) {
    if (eoffn(fn)) errore(ENDOFFILE);
    read(filtable[fn], c); putchr(ad, c); ad = ad+1; l = l-1;
  }
  while (l > 0) { putchr(ad, ' '); ad = ad+1; l = l-1; }
}

void writestrp(FILE* f, address ad, int l)
{
    int i;
    address ad1;

    ad1 = ad+l-1; /* find end */
    while (l > 0 && getchr(ad1) == ' ')
           { ad1 = ad1-1; l = l-1; }
    for (i = 0; i < l; i++) fprintf(f, "%c", getchr(ad+i));
}

void filllz(FILE* f, int n)
{ while (n > 0) { fputc('0', f); n--; } }

/* Write integer */
void writei(FILE* f, int w, int fl, int r, int lz)
{
    int c, p, i, d, ds;
    char digit[MAXDBF];
    boolean sgn;

    if (w < 0) {
        sgn = TRUE; w = abs(w);
        if (r != 10) errore(NONDECIMALRADIXOFNEGATIVE) ;
    } else sgn = FALSE;
    for (i = 0; i < MAXDBF; i++) digit[i] = ' ';
    i = MAXDBF==1; d = 0;
    do {
        if (w % r < 10) digit[i] = w % r+'0';
        else digit[i] = w % r -10 +'a';
        w = w / r; i = i-1; d = d+1;
    } while (w != 0);
    if (sgn) ds = d+1; else ds = d; /* add sign */
    if (ds > abs(fl)) if (fl < 0) fl = -ds; else fl = ds;
    if (fl > 0 && fl > ds)
      if (lz) filllz(f, fl-ds); else fprintf(f, "%*c", fl-ds, ' ');
    if (sgn) fputc('-', f);
    for (i = MAXDBF-d+1; i <= MAXDBF; i++) fputc(digit[i], f);
    if (fl < 1 && abs(fl) > ds) fprintf(f, "%*c", abs(fl)-ds, ' ');
}

void writeb(FILE* f, boolean b, int w)
{ if (b) fprintf(f, "%*s", w, "true"); else fprintf(f, "%*s", w, "false"); }

void putfile(FILE* f, address ad, filnum fn)
{
    if (!filbuff[fn]) errore(FILEBUFFERVARIABLEUNDEFINED);
    fputc(getchr(ad+FILEIDSIZE), f);
    filbuff[fn] = FALSE;
} /*putfile*/

void resetfn(filnum fn, boolean bin)
{
    /* file was closed, no assigned name, give it a temp name */
    if (filstate[fn] == fsclosed && !filanamtab[fn]) tmpnam(filnamtab[fn]);
    filstate[fn] = fsread;
    if (!fopen(filnamtab[fn], bin?"rb":"r")) errore(FILEOPENFAIL);
    filbuff[fn] = FALSE;
}

void rewritefn(filnum fn, boolean bin)
{
    /* file was closed, no assigned name, give it a temp name */
    if (filstate[fn] == fsclosed && !filanamtab[fn]) tmpnam(filnamtab[fn]);
    filstate[fn] = fswrite;
    if (!fopen(filnamtab[fn], bin?"wb":"w")) errore(FILEOPENFAIL);
    filbuff[fn] = FALSE;
}

void callsp(void)

{
    boolean line;
    int i, j, k, w, l, f;
    char c;
    boolean b;
    address ad,ad1,ad2;
    float r, r1;
    filnum fn;
    int mn,mx;
    filnam fl1, fl2;
    int rd;
    boolean lz;
    boolean fld;
    FILE* fp;

    if (q > MAXSP) errorv(INVALIDSTANDARDPROCEDUREORFUNCTION);

    switch (q) {

    case 0 /*get*/: popadr(&ad); valfil(ad); fn = store[ad]; getfn(fn); break;
    case 1 /*put*/: popadr(&ad); valfil(ad); fn = store[ad];
                    if (fn <= COMMANDFN) switch (fn) {
                       case OUTPUTFN: putfile(stdout, ad, fn); break;
                       case PRRFN: putfile(filtable[PRRFN], ad, fn); break;
                       case ERRORFN: putfile(stderr, ad, fn); break;
                       case LISTFN: putfile(stdout, ad, fn); break;
                       case INPUTFN: case PRDFN:
                       case COMMANDFN: errore(WRITEONREADONLYFILE); break;
                    } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         putfile(filtable[fn], ad, fn);
                    }
                    break;
    case 3 /*rln*/: popadr(&ad); pshadr(ad); valfil(ad); fn = store[ad];
                    if (fn <= COMMANDFN) switch (fn) {
                       case INPUTFN: readline(stdin); break;
                       case PRDFN: readline(filtable[PRDFN]); break;
                       case OUTPUTFN: prrfn: errorfn:
                       case LISTFN: errore(READONWRITEONLYFILE); break;
                       case COMMANDFN: readlncommand(); break;
                    } else {
                         if (filstate[fn] != fsread) errore(FILEMODEINCORRECT);
                         readline(filtable[fn]);
                    }
                    break;
    case 4 /*new*/: popadr(&ad1); newspc(ad1, &ad);
                    /*top of stack gives the length in units of storage */
                    popadr(&ad1); putadr(ad1, ad);
                    break;
    case 39 /*nwl*/: /* size of record, size of f const list */
                     popadr(&ad1); popint(&i);
                     /* alloc record, size of list, number in list */
                     newspc(ad1+(i+1)*INTSIZE, &ad);
                     ad1 = ad+i*INTSIZE; putint(ad1, i+ADRSIZE+1);
                     k = i; /* save n tags for later */
                     while (k > 0)
                     {
                        ad1 = ad1-INTSIZE; popint(&j);
                        putint(ad1, j); k = k-1;
                     }
                     /* get pointer to dest var, place base above taglist and
                        list of fixed consts */
                     popadr(&ad1); putadr(ad1, ad+(i+1)*INTSIZE);
                     break;
    case 5 /*wln*/: popadr(&ad); pshadr(ad); valfil(ad); fn = store[ad];
                    if (fn <= COMMANDFN) switch (fn) {
                       case OUTPUTFN: fprintf(stdout, "\n"); break;
                       case PRRFN: fprintf(filtable[PRRFN], "\n"); break;
                       case ERRORFN: fprintf(stderr, "\n"); break;
                       case LISTFN: fprintf(filtable[LISTFN], "\n"); break;
                       case PRDFN: case INPUTFN:
                       case COMMANDFN: errore(WRITEONREADONLYFILE); break;
                    } else {
                       if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                       fprintf(filtable[fn], "\n");
                    }
                    break;
    case 6 /*wrs*/: popint(&l); popint(&w); popadr(&ad1);
                    popadr(&ad); pshadr(ad); valfil(ad); fn = store[ad];
                    if (w < 1 && ISO7185) errore(INVALIDFIELDSPECIFICATION);
                    if (fn <= COMMANDFN) switch (fn) {
                       case OUTPUTFN: fprintf(stdout, "%*.*s", w, l,
                                              (char*)(store+ad1)); break;
                       case PRRFN: fprintf(filtable[PRRFN], "%*.*s", w, l,
                                           (char*)(store+ad1)); break;
                       case ERRORFN: fprintf(stderr, "%*.*s", w, l,
                                             (char*)(store+ad1));
                                     break;
                       case LISTFN: fprintf(stdout, "%*.*s", w, l,
                                            (char*)(store+ad1));
                                    break;
                       case PRDFN: case INPUTFN:
                       case COMMANDFN: errore(WRITEONREADONLYFILE); break;
                    } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         fprintf(filtable[fn], "%*.*s", w, l,
                                 (char*)(store+ad1));
                    }
                    break;
    case 65 /*wrsp*/: popint(&l); popadr(&ad1); popadr(&ad); pshadr(ad);
                    valfil(ad); fn = store[ad];
                    if (w < 1 && ISO7185) errore(INVALIDFIELDSPECIFICATION);
                    if (fn <= COMMANDFN) switch (fn) {
                      case OUTPUTFN: writestrp(stdout, ad1, l); break;
                      case PRRFN: writestrp(filtable[PRRFN], ad1, l); break;
                      case ERRORFN: writestrp(stderr, ad1, l); break;
                      case LISTFN: writestrp(stdout, ad1, l); break;
                      case PRDFN: case INPUTFN:
                      case COMMANDFN: errore(WRITEONREADONLYFILE); break;
                    } else {
                      if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                      writestrp(filtable[fn], ad1, l);
                    }
                    break;
    case 41 /*eof*/: popadr(&ad); valfil(ad); fn = store[ad];
                    pshint(eoffn(fn));
                    break;
    case 42 /*efb*/:
                 popadr(&ad); valfilrm(ad); fn = store[ad];
                 if (filstate[fn] == fswrite) pshint(TRUE);
                 else if (filstate[fn] == fsread)
                   pshint(eoffile(filtable[fn]) && !filbuff[fn]);
                 break;
    case 7 /*eln*/: popadr(&ad); valfil(ad); fn = store[ad];
                    pshint(eolnfn(fn));
                    break;
    case 8 /*wri*/:
    case 62 /*wrih*/:
    case 63 /*wrio*/:
    case 64 /*wrib*/:
    case 66 /*wiz*/:
    case 67 /*wizh*/:
    case 68 /*wizo*/:
    case 69 /*wizb*/: popint(&w); popint(&i); popadr(&ad); pshadr(ad);
                     rd = 10;
                     if (q == 62 || q == 67) rd = 16;
                     else if (q == 63 || q == 68) rd = 8;
                     else if (q == 64 || q == 69) rd = 2;
                     lz = q >= 66 && q <= 69;
                     valfil(ad); fn = store[ad];
                     if (w < 1 && ISO7185) errore(INVALIDFIELDSPECIFICATION);
                     if (fn <= COMMANDFN) switch (fn) {
                          case OUTPUTFN: writei(stdout, i, w, rd, lz);
                          case PRRFN: writei(filtable[PRRFN], i, w, rd, lz);
                          case ERRORFN: writei(stderr, i, w, rd, lz);
                          case LISTFN: writei(stdout, i, w, rd, lz);
                          case PRDFN: case INPUTFN:
                          case COMMANDFN: errore(WRITEONREADONLYFILE);
                     } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         writei(filtable[fn], i, w, rd, lz);
                     }
                     break;
    case 9 /*wrr*/: popint(&w); poprel(&r); popadr(&ad); pshadr(ad);
                     valfil(ad); fn = store[ad];
                     if (w < 1) errore(INVALIDFIELDSPECIFICATION);
                     if (fn <= COMMANDFN) switch (fn) {
                          case OUTPUTFN: fprintf(stdout, "%*.*e", w, w-REALEF, r); break;
                          case PRRFN: fprintf(filtable[PRRFN], "%*.*e", w, w-REALEF, r); break;
                          case ERRORFN: fprintf(stderr, "%*.*e", w, w-REALEF, r); break;
                          case LISTFN: fprintf(stdout, "%*.*e", w, w-REALEF, r); break;
                          case PRDFN: case INPUTFN:
                          case COMMANDFN: errore(WRITEONREADONLYFILE);
                     } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         fprintf(filtable[fn], "%*.*e", w, w-REALEF, r);
                     };
                     break;
    case 10/*wrc*/: popint(&w); popint(&i); c = i; popadr(&ad);
                     pshadr(ad); valfil(ad); fn = store[ad];
                     if (w < 1 && ISO7185) errore(INVALIDFIELDSPECIFICATION);
                     if (fn <= COMMANDFN) switch (fn) {
                          case OUTPUTFN: fprintf(stdout, "%*c", w, c);
                          case PRRFN: fprintf(filtable[PRRFN], "%*c", w, c);
                          case ERRORFN: fprintf(stderr, "%*c", w, c);
                          case LISTFN: fprintf(stdout, "%*c", w, c);
                          case PRDFN: case INPUTFN:
                          case COMMANDFN: errore(WRITEONREADONLYFILE);
                     } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         fprintf(filtable[fn], "%*c", w, c);
                     }
                     break;
    case 11/*rdi*/:
    case 72/*rdif*/: w = INT_MAX; fld = q == 72; if (fld) popint(&w);
                     popadr(&ad1); popadr(&ad); pshadr(ad);
                     valfil(ad); fn = store[ad]; readi(fn, &i, &w, fld);
                     putint(ad1, i);
                     break;
    case 37/*rib*/:
    case 71/*ribf*/: w = INT_MAX; fld = q == 71; popint(&mx); popint(&mn);
                    if (fld) popint(&w); popadr(&ad1); popadr(&ad);
                    pshadr(ad); valfil(ad); fn = store[ad];
                    readi(fn, &i, &w, fld);
                    if (i < mn || i > mx) errore(VALUEOUTOFRANGE);
                    putint(ad1, i);
                    break;
    case 12/*rdr*/:
    case 73/*rdrf*/: w = INT_MAX; fld = q == 73; if (fld) popint(&w);
                    popadr(&ad1); popadr(&ad); pshadr(ad);
                    valfil(ad); fn = store[ad];
                    readr(fn, &r, w, fld); putrel(ad1, r);
                    break;
    case 13/*rdc*/:
    case 75/*rdcf*/: w = INT_MAX; fld = q == 75; if (fld) popint(&w);
                    popadr(&ad1); popadr(&ad); pshadr(ad);
                    valfil(ad); fn = store[ad];
                    readc(fn, &c, w, fld); putchr(ad1, c);
                    break;
    case 38/*rcb*/:
    case 74/*rcbf*/: w = INT_MAX; fld = q == 74; popint(&mx); popint(&mn);
                     if (fld) popint(&w); popadr(&ad1); popadr(&ad);
                     pshadr(ad); valfil(ad);
                     fn = store[ad];
                     readc(fn, &c, w, fld);
                     if (c < mn || c > mx) errore(VALUEOUTOFRANGE);
                     putchr(ad1, c);
                     break;
    case 14/*sin*/: poprel(&r1); pshrel(sin(r1)); break;
    case 15/*cos*/: poprel(&r1); pshrel(cos(r1)); break;
    case 16/*exp*/: poprel(&r1); pshrel(exp(r1)); break;
    case 17/*log*/: poprel(&r1); if (r1 <= 0) errore(INVALIDARGUMENTTOLN);
                    pshrel(log(r1)); break;
    case 18/*sqt*/: poprel(&r1); if (r1 < 0) errore(INVALIDARGUMENTTOSQRT);
                    pshrel(sqrt(r1)); break;
    case 19/*atn*/: poprel(&r1); pshrel(atan(r1)); break;
    /* placeholder for "mark" */
    case 20/*sav*/: errorv(INVALIDSTANDARDPROCEDUREORFUNCTION);
    case 21/*pag*/: popadr(&ad); valfil(ad); fn = store[ad];
                    if (fn <= COMMANDFN) switch (fn) {
                         case OUTPUTFN: fprintf(stdout, "\f"); break;
                         case PRRFN: fprintf(filtable[PRRFN], "\f"); break;
                         case ERRORFN: fprintf(stderr, "\f"); break;
                         case LISTFN: fprintf(stdout, "\f"); break;
                         case PRDFN: case INPUTFN:
                         case COMMANDFN: errore(WRITEONREADONLYFILE); break;
                    } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         fprintf(filtable[fn], "\f");
                    }
                    break;
    case 22/*rsf*/: popadr(&ad); valfil(ad); fn = store[ad];
                    if (fn <= COMMANDFN) switch (fn) {
                         case PRDFN: resetfn(PRDFN, FALSE); break;
                         case PRRFN: errore(CANNOTRESETWRITEONLYFILE); break;
                         case OUTPUTFN: case ERRORFN: case LISTFN: case INPUTFN:
                         case COMMANDFN:
                           errore(CANNOTRESETORREWRITESTANDARDFILE); break;
                    } else resetfn(fn, FALSE);
                    break;
    case 23/*rwf*/: popadr(&ad); valfil(ad); fn = store[ad];
                    if (fn <= COMMANDFN) switch (fn) {
                         case PRRFN: rewritefn(PRRFN, FALSE); break;
                         case PRDFN: errore(CANNOTREWRITEREADONLYFILE); break;
                         case ERRORFN: case LISTFN: case OUTPUTFN: case INPUTFN:
                         case COMMANDFN:
                           errore(CANNOTRESETORREWRITESTANDARDFILE); break;
                    } else rewritefn(fn, FALSE);
                    break;
    case 24/*wrb*/: popint(&w); popint(&i); b = i != 0; popadr(&ad);
                     pshadr(ad); valfil(ad); fn = store[ad];
                     if (w < 1) errore(INVALIDFIELDSPECIFICATION);
                     if (fn <= COMMANDFN) switch (fn) {
                          case OUTPUTFN: writeb(stdout, b, w); break;
                          case PRRFN: writeb(filtable[PRRFN], b, w); break;
                          case ERRORFN: writeb(stderr, b, w); break;
                          case LISTFN: writeb(stdout, b, w); break;
                          case PRDFN: case INPUTFN:
                          case COMMANDFN: errore(WRITEONREADONLYFILE); break;
                     } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         writeb(filtable[fn], b, w);
                     }
                     break;
    case 25/*wrf*/: popint(&f); popint(&w); poprel(&r); popadr(&ad); pshadr(ad);
                     valfil(ad); fn = store[ad];
                     if (w < 1 && ISO7185) errore(INVALIDFIELDSPECIFICATION);
                     if (f < 1) errore(INVALIDFRACTIONSPECIFICATION);
                     if (fn <= COMMANDFN) switch (fn) {
                          case OUTPUTFN: fprintf(stdout, "%*.*f", w, f, r);
                                         break;
                          case PRRFN: fprintf(filtable[PRRFN], "%*.*f", w, f, r);
                                      break;
                          case ERRORFN: fprintf(stderr, "%*.*f", w, f, r);
                                        break;
                          case LISTFN: fprintf(stdout, "%*.*f", w, f, r); break;
                          case PRDFN: case INPUTFN:
                          case COMMANDFN: errore(WRITEONREADONLYFILE); break;
                     } else {
                         if (filstate[fn] != fswrite) errore(FILEMODEINCORRECT);
                         fprintf(filtable[fn], "%*.*f", w, f, r);
                     }
                     break;
    case 26/*dsp*/: popadr(&ad1); popadr(&ad); dspspc(ad1, ad); break;
    case 40/*dsl*/: popadr(&ad1); popint(&i); /* get size of record and n tags */
                    ad = getadr(sp+i*INTSIZE); /* get rec addr */
                    /* under us is either the number of tags or the length of the block, if it
                      was freed. Either way, it is >= adrsize if not free */
                    if (getint(ad-INTSIZE) <= ADRSIZE)
                        errorv(BLOCKALREADYFREED);
                    if (i != getint(ad-INTSIZE)-ADRSIZE-1)
                        errorv(NEWDISPOSETAGSMISMATCH);
                    ad = ad-INTSIZE; ad2 = sp;
                    /* ad = top of tags in dynamic, ad2 = top of tags in
                      stack */
                    k = i;
                    while (k > 0)
                    {
                        if (getint(ad) != getint(ad2))
                            errorv(NEWDISPOSETAGSMISMATCH);
                        ad = ad-INTSIZE; ad2 = ad2+INTSIZE; k = k-1;
                    }
                    dspspc(ad1+(i+1)*INTSIZE, ad+INTSIZE);
                    while (i > 0) { popint(&j); i = i-1; };
                    popadr(&ad);
                    break;
    case 27/*wbf*/: popint(&l); popadr(&ad1); popadr(&ad); pshadr(ad);
                    valfilwm(ad); fn = store[ad];
                    for (i = 0; i < l; i++) {
                       chkdef(ad1); fputc(store[ad1], filtable[fn]);
                       ad1 = ad1+1;
                    }
                    break;
    case 28/*wbi*/: popint(&i); popadr(&ad); pshadr(ad); pshint(i);
                     valfilwm(ad); fn = store[ad];
                     for (i = 0; i < INTSIZE; i++)
                        fputc(store[sp+i], filtable[fn]);
                     popint(&i);
                     break;
    case 45/*wbx*/: popint(&i); popadr(&ad); pshadr(ad); pshint(i);
                     valfilwm(ad); fn = store[ad];
                     fputc(store[sp], filtable[fn]); popint(&i);
                     break;
    case 29/*wbr*/: poprel(&r); popadr(&ad); pshadr(ad); pshrel(r);
                     valfilwm(ad); fn = store[ad];
                     for (i = 0; i < REALSIZE; i++)
                        fputc(store[sp+i], filtable[fn]);
                     poprel(&r);
                     break;
    case 30/*wbc*/: popint(&i); c = i; popadr(&ad); pshadr(ad); pshint(i);
                     valfilwm(ad); fn = store[ad];
                     for (i = 0; i < CHARSIZE; i++)
                        fputc(store[sp+i], filtable[fn]);
                     popint(&i);
                     break;
    case 31/*wbb*/: popint(&i); popadr(&ad); pshadr(ad); pshint(i);
                     valfilwm(ad); fn = store[ad];
                     for (i = 0; i < BOOLSIZE; i++)
                         fputc(store[sp+i-1], filtable[fn]);
                     popint(&i);
                     break;
    case 32/*rbf*/: popint(&l); popadr(&ad1); popadr(&ad); pshadr(ad);
                     valfilrm(ad); fn = store[ad];
                     if (filbuff[fn]) /* buffer data exists */
                       for (i = 0; i < l; i++) {
                         store[ad1+i] = store[ad+FILEIDSIZE+i];
                         putdef(ad1+i, TRUE);
                       }
                     else
                       for (i = 0; i < l; i++) {
                         if (eoffile(filtable[fn])) errore(ENDOFFILE);
                         store[ad1] = fgetc(filtable[fn]);
                         putdef(ad1, TRUE);
                         ad1 = ad1+1;
                       }
                     break;
    case 33/*rsb*/: popadr(&ad); valfil(ad); fn = store[ad]; resetfn(fn, TRUE);
                    break;
    case 34/*rwb*/: popadr(&ad); valfil(ad); fn = store[ad];
                    rewritefn(fn, TRUE); break;
    case 35/*gbf*/: popint(&i); popadr(&ad); valfilrm(ad);
                    fn = store[ad];
                    if (filbuff[fn]) filbuff[fn] = FALSE;
                    else
                      for (j = 0; j < i; j++)
                         store[ad+FILEIDSIZE+j] = fgetc(filtable[fn]);
                    break;
    case 36/*pbf*/: popint(&i); popadr(&ad); valfilwm(ad);
                 fn = store[ad];
                 if (!filbuff[fn]) errore(FILEBUFFERVARIABLEUNDEFINED);
                 for (j = 0; j < i; j++)
                   fputc(store[ad+FILEIDSIZE+j], filtable[fn]);
                 filbuff[fn] = FALSE;
                 break;
    case 43 /*fbv*/: popadr(&ad); pshadr(ad); valfil(ad);
                   fn = store[ad];
                   if (fn == INPUTFN) putchr(ad+FILEIDSIZE, buffn(INPUTFN));
                   else if (fn == PRDFN) putchr(ad+FILEIDSIZE, buffn(PRDFN));
                   else {
                     if (filstate[fn] == fsread)
                       putchr(ad+FILEIDSIZE, buffn(fn));
                   }
                   filbuff[fn] = TRUE;
                   break;
    case 44 /*fvb*/: popint(&i); popadr(&ad); pshadr(ad); valfil(ad);
                   fn = store[ad];
                   /* load buffer only if in read mode, and buffer is
                     empty */
                   if (filstate[fn] = fsread && !filbuff[fn]) {
                       for (j = 0; j < i; j++) {
                         read(filtable[fn], store[ad+FILEIDSIZE+j]);
                         putdef(ad+FILEIDSIZE+j, TRUE);
                       }
                   }
                   filbuff[fn] = TRUE;
                   break;
    /* extended Pascaline file handlers */
    case 46 /*asst*/:
    case 56 /*assb*/: popint(&i); popadr(&ad1); popadr(&ad); valfil(ad);
                  fn = store[ad];
                  for (j = 0; j < i; j++) {
                    if (j >= FILLEN) errore(FILENAMETOOLONG);
                    filnamtab[fn][j] = store[ad1+j];
                  }
                  if (j >= FILLEN) errore(FILENAMETOOLONG);
                  filnamtab[fn][j] = 0;
                  filanamtab[fn] = TRUE; /* set name assigned */
                  break;
    case 47 /*clst*/:
    case 57 /*clsb*/: popadr(&ad); valfil(ad); fn = store[ad];
                if (fclose(filtable[fn])) errorv(FILECLOSEFAIL);
                /* if the file is temp, remove now */
                if (!filanamtab[fn]) remove(filnamtab[fn]);
                filanamtab[fn] = FALSE; /* break any name association */
                break;
    case 48 /*pos*/: popint(&i); popadr(&ad); valfil(ad); fn = store[ad];
                if (i < 1) errore(INVALIDFILEPOSITION);
                if (fseek(filtable[fn], i-1, SEEK_SET)) errore(FILEPOSITIONFAIL);
                break;
    case 49 /*upd*/: popadr(&ad); valfil(ad); fn = store[ad];
                if (filstate[fn] == fsread) fseek(filtable[fn], 0, SEEK_SET);
                else {
                  if (fclose(filtable[fn])) errorv(FILECLOSEFAIL);
                  if (!fopen(filnamtab[fn], "wb")) errore(FILEOPENFAIL);
                }
                break;
    case 50 /*appt*/: popadr(&ad); valfil(ad); fn = store[ad];
                if (filstate[fn] == fswrite) fseek(filtable[fn], 0, SEEK_END);
                else {
                  if (fclose(filtable[fn])) errorv(FILECLOSEFAIL);
                  if (!fopen(filnamtab[fn], "w")) errore(FILEOPENFAIL);
                }
                break;
    case 58 /*appb*/: popadr(&ad); valfil(ad); fn = store[ad];
                if (filstate[fn] == fswrite) fseek(filtable[fn], 0, SEEK_END);
                else {
                  if (fclose(filtable[fn])) errorv(FILECLOSEFAIL);
                  if (!fopen(filnamtab[fn], "wb")) errore(FILEOPENFAIL);
                }
                break;
    case 51 /*del*/: popint(&i); popadr(&ad1);
                  for (j = 0; j < i; i++) fl1[j] = store[ad1+j]; fl1[j] = 0;
                  i = remove(fl1); if (i) errorv(FILEDELETEFAIL);
                  break;
    case 52 /*chg*/: popint(&i); popadr(&ad1); popint(&l); popadr(&ad);
                  for (j = 0; j < i; j++) fl1[j] = store[ad1+j]; fl1[j] = 0;
                  for (j = 0; j < l; j++) fl2[j] = store[ad+j]; fl2[j] = 0;
                  if (rename(fl1, fl2)) errorv(FILENAMECHANGEFAIL);
                  break;
    case 53 /*len*/: popadr(&ad); valfil(ad); fn = store[ad];
                pshint(filelength(filtable[fn]));
                break;
    case 54 /*loc*/: popadr(&ad); valfil(ad); fn = store[ad];
                  if (i = ftell(filtable[fn]) < 0) errorv(FILEPOSITIONFAIL);
                  pshint(i+1);
                  break;
    case 55 /*exs*/: popint(&i); popadr(&ad1);
                  for (j = 0; j < i; j++) fl1[j] = store[ad1+j]; fl1[j] = 0;
                  if (fp = fopen(fl1, "r")) fclose(fp);
                  pshint(!!fp);
                  break;
    case 59 /*hlt*/: finish(1); break;
    case 60 /*ast*/: popint(&i);
                  if (i == 0) errorv(PROGRAMCODEASSERTION);
                 break;
    case 61 /*asts*/: popint(&i); popadr(&ad); popint(&j);
                  if (j == 0) errors(ad, i);
                  break;
    case 70/*rds*/:
    case 76/*rdsf*/: w = INT_MAX; fld = q == 76; popint(&i);
                  if (fld) popint(&w); popadr(&ad1); popadr(&ad);
                  pshadr(ad); valfil(ad); fn = store[ad];
                  reads(fn, ad1, i, w, fld);
                  break;
    case 77/*rdsp*/: popint(&i); popadr(&ad1); popadr(&ad); pshadr(ad);
                  valfil(ad); fn = store[ad];
                  readsp(fn, ad1, i);
                  break;
    case 78/*aeft*/: popint(&i); popadr(&ad1); popadr(&ad); valfil(ad);
                  fn = store[ad]; clrfn(fl1);
                  for (j = 0; j < i; j++) fl1[j] = store[ad1+j];
                  assignexternal(fn, fl1);
                  break;
    case 79/*aefb*/: popint(&i); popadr(&ad1); popadr(&ad); valfil(ad);
                  fn = store[ad]; clrfn(fl1);
                  for (j = 0; j < i; j++) fl1[j] = store[ad1+j];
                  assignexternal(fn, fl1);
                  break;
    case 80/*rdie*/: w = INT_MAX; popint(&i); popadr(&ad1); popadr(&ad);
                  readi(COMMANDFN, &i, &w, FALSE); putint(ad, i);
                  break;
    case 81/*rdre*/: w = INT_MAX; popint(&i); popadr(&ad1); popadr(&ad);
                  readr(COMMANDFN, &r, w, FALSE); putrel(ad, r);
                  break;
    case 2/*thw*/: popadr(&ad1); mp = expmrk; sp = expstk;
                  pc = expadr; popadr(&ad2); pshadr(ad1);
                  ep = getadr(mp+MARKET); /* get the mark ep */
                  /* release to search vectors */
                  break;

    } /*case q*/
} /*callsp*/

void sinins()

{
    address ad,ad1,ad2,ad3; boolean b; int i,j,k,i1,i2; char c, c1; int i3,i4;
    float r1,r2; boolean b1,b2; settype s1,s2; address a1,a2,a3;

/*printf("sinins: pc: %08x sp: %08x mp: %02x @pc:%02x/%03d\n",
       pc, sp, mp, store[pc], store[pc]);*/
    if (pc >= pctop) errorv(PCOUTOFRANGE);

    /* fetch instruction from byte store */
    getop();

    /*execute*/

    switch (op) {

    case 0   /*lodi*/: getp(); getq(); pshint(getint(base(p) + q)); break;
    case 193 /*lodx*/: getp(); getq(); pshint(getbyt(base(p) + q)); break;
    case 105 /*loda*/: getp(); getq(); pshadr(getadr(base(p) + q)); break;
    case 106 /*lodr*/: getp(); getq(); pshrel(getrel(base(p) + q)); break;
    case 107 /*lods*/: getp(); getq(); getset(base(p) + q, s1); pshset(s1); break;
    case 108 /*lodb*/: getp(); getq(); pshint(getbol(base(p) + q)); break;
    case 109 /*lodc*/: getp(); getq(); pshint(getchr(base(p) + q)); break;

    case 1   /*ldoi*/: getq(); pshint(getint(q)); break;
    case 194 /*ldox*/: getq(); pshint(getbyt(q)); break;
    case 65  /*ldoa*/: getq(); pshadr(getadr(q)); break;
    case 66  /*ldor*/: getq(); pshrel(getrel(q)); break;
    case 67  /*ldos*/: getq(); getset(q, s1); pshset(s1); break;
    case 68  /*ldob*/: getq(); pshint(getbol(q)); break;
    case 69  /*ldoc*/: getq(); pshint(getchr(q)); break;

    case 2   /*stri*/: getp(); getq(); popint(&i); putint(base(p)+q, i); break;
    case 195 /*strx*/: getp(); getq(); popint(&i); putbyt(base(p)+q, i); break;
    case 70  /*stra*/: getp(); getq(); popadr(&ad); putadr(base(p)+q, ad); break;
    case 71  /*strr*/: getp(); getq(); poprel(&r1); putrel(base(p)+q, r1); break;
    case 72  /*strs*/: getp(); getq(); popset(s1); putset(base(p)+q, s1); break;
    case 73  /*strb*/: getp(); getq(); popint(&i1); b1 = i1 != 0;
                       putbol(base(p)+q, b1); break;
    case 74  /*strc*/: getp(); getq(); popint(&i1); c1 = i1;
                         putchr(base(p)+q, c1); break;

    case 3   /*sroi*/: getq(); popint(&i); putint(q, i); break;
    case 196 /*srox*/: getq(); popint(&i); putbyt(q, i); break;
    case 75  /*sroa*/: getq(); popadr(&ad); putadr(q, ad); break;
    case 76  /*sror*/: getq(); poprel(&r1); putrel(q, r1); break;
    case 77  /*sros*/: getq(); popset(s1); putset(q, s1); break;
    case 78  /*srob*/: getq(); popint(&i1); b1 = i1 != 0; putbol(q, b1); break;
    case 79  /*sroc*/: getq(); popint(&i1); c1 = i1; putchr(q, c1); break;

    case 4 /*lda*/: getp(); getq(); pshadr(base(p)+q); break;
    case 5 /*lao*/: getq(); pshadr(q); break;

    case 6   /*stoi*/: popint(&i); popadr(&ad); putint(ad, i); break;
    case 197 /*stox*/: popint(&i); popadr(&ad); putbyt(ad, i); break;
    case 80  /*stoa*/: popadr(&ad1); popadr(&ad); putadr(ad, ad1); break;
    case 81  /*stor*/: poprel(&r1); popadr(&ad); putrel(ad, r1); break;
    case 82  /*stos*/: popset(s1); popadr(&ad); putset(ad, s1); break;
    case 83  /*stob*/: popint(&i1); b1 = i1 != 0; popadr(&ad); putbol(ad, b1);
                       break;
    case 84  /*stoc*/: popint(&i1); c1 = i1; popadr(&ad); putchr(ad, c1);
                       break;

    case 127 /*ldcc*/: pshint(getchr(pc)); pc = pc+1; break;
    case 126 /*ldcb*/: pshint(getbol(pc)); pc = pc+1; break;
    case 123 /*ldci*/: i = getint(pc); pc = pc+INTSIZE; pshint(i); break;
    case 125 /*ldcn*/: pshadr(NILVAL); break; /* load nil */
    case 124 /*ldcr*/: getq(); pshrel(getrel(q)); break;
    case 7   /*ldcs*/: getq(); getset(q, s1); pshset(s1); break;

    case 9   /*indi*/: getq(); popadr(&ad); pshint(getint(ad+q)); break;
    case 198 /*indx*/: getq(); popadr(&ad); pshint(getbyt(ad+q)); break;
    case 85  /*inda*/: getq(); popadr(&ad); ad1 = getadr(ad+q); pshadr(ad1); break;
    case 86  /*indr*/: getq(); popadr(&ad); pshrel(getrel(ad+q)); break;
    case 87  /*inds*/: getq(); popadr(&ad); getset(ad+q, s1); pshset(s1); break;
    case 88  /*indb*/: getq(); popadr(&ad); pshint(getbol(ad+q)); break;
    case 89  /*indc*/: getq(); popadr(&ad); pshint(getchr(ad+q)); break;

    case 93 /*incb*/:
    case 94 /*incc*/:
    case 201 /*incx*/:
    case 10 /*inci*/: getq(); popint(&i1);
                   if (DOCHKOVF) if (i1<0 == q<0)
                      if (INT_MAX-abs(i1) < abs(q))
                        errore(INTEGERVALUEOVERFLOW);
                   pshint(i1+q);
                   break;
    case 90 /*inca*/: getq(); popadr(&a1); pshadr(a1+q); break;

    case 11 /*mst*/: /*p=level of calling procedure minus level of called
                       procedure + 1;  set dl and sl, decrement sp*/
                     /* then length of this element is
                        max(intsize,realsize,boolsize,charsize,ptrsize */
                 getp();
                 ad = sp; /* save mark base */
                 /* allocate mark */
                 for (j = 0; j < MARKSIZE/INTSIZE; j++) pshint(0);
                 putadr(ad+MARKSL, base(p)); /* sl */
                 /* the length of this element is ptrsize */
                 putadr(ad+MARKDL, mp); /* dl */
                 /* idem */
                 putadr(ad+MARKEP, ep); /* ep */
                 /* idem */
                 break;

    case 12 /*cup*/: /*p=no of locations for parameters, q=entry point*/
                 getp(); getq();
                 mp = sp+(p+MARKSIZE); /* mp to base of mark */
                 putadr(mp+MARKRA, pc); /* place ra */
                 pc = q;
                 break;

    case 13 /*ents*/: getq(); ad = mp+q; /*q = length of dataseg*/
                    if (ad <= np) errorv(STOREOVERFLOW);
                    /* clear allocated memory and set undefined */
                    while (sp > ad)
                      { sp = sp-1; store[sp] = 0; putdef(sp, FALSE); }
                    putadr(mp+MARKSB, sp); /* set bottom of stack */
                    break;

    case 173 /*ente*/:  getq(); ep = sp+q;
                    if (ep <= np) errorv(STOREOVERFLOW);
                    putadr(mp+MARKET, ep); /* place current ep */
                    break;
                    /*q = max space required on stack*/

    case 14  /*retp*/:
                   if (sp != getadr(mp+MARKSB)) errorv(STACKBALANCE);
                   sp = mp;
                   pc = getadr(mp+MARKRA); /* get ra */
                   ep = getadr(mp+MARKEP); /* get old ep */
                   mp = getadr(mp+MARKDL); /* get dl */
                   break;
    /* For characters and booleans, need to clean 8 bit results because
      only the lower 8 bits were stored to. */
    case 130 /*retc*/:
                   if (sp != getadr(mp+MARKSB)) errorv(STACKBALANCE);
                   putint(mp+MARKFV+MAXRESULT/2,
                          getchr(mp+MARKFV+MAXRESULT/2));
                   /* set stack below function result */
                   sp = mp+MARKFV+MAXRESULT / 2;
                   pc = getadr(mp+MARKRA);
                   ep = getadr(mp+MARKEP);
                   mp = getadr(mp+MARKDL);
                   break;
    case 131 /*retb*/:
                   if (sp != getadr(mp+MARKSB)) errorv(STACKBALANCE);
                   putint(mp+MARKFV+MAXRESULT/2,
                          getbol(mp+MARKFV+MAXRESULT/2));
                   /* set stack below function result */
                   sp = mp+MARKFV+MAXRESULT / 2;
                   pc = getadr(mp+MARKRA);
                   ep = getadr(mp+MARKEP);
                   mp = getadr(mp+MARKDL);
                   break;
    case 128 /*reti*/:
    case 204 /*retx*/:
                   if (sp != getadr(mp+MARKSB)) errorv(STACKBALANCE);
                   /* set stack below function result */
                   sp = mp+MARKFV+MAXRESULT / 2;
                   pc = getadr(mp+MARKRA);
                   ep = getadr(mp+MARKEP);
                   mp = getadr(mp+MARKDL);
                   break;
    case 129 /*retr*/:
                   if (sp != getadr(mp+MARKSB)) errorv(STACKBALANCE);
                   sp = mp+MARKFV; /* set stack below function result */
                   pc = getadr(mp+MARKRA);
                   ep = getadr(mp+MARKEP);
                   mp = getadr(mp+MARKDL);
                   break;
    case 132  /*reta*/:
                   if (sp != getadr(mp+MARKSB)) errorv(STACKBALANCE);
                   /* set stack below function result */
                   sp = mp+MARKFV+MAXRESULT / 2;
                   pc = getadr(mp+MARKRA);
                   ep = getadr(mp+MARKEP);
                   mp = getadr(mp+MARKDL);
                   break;

    case 15 /*csp*/: q = store[pc]; pc = pc+1; callsp(); break;

    case 16 /*ixa*/: getq(); popint(&i); popadr(&a1); pshadr(q*i+a1); break;

    case 17  /* equa */: popadr(&a2); popadr(&a1); pshint(a1==a2); break;
    case 139 /* equb */:
    case 141 /* equc */:
    case 137 /* equi */: popint(&i2); popint(&i1); pshint(i1==i2); break;
    case 138 /* equr */: poprel(&r2); poprel(&r1); pshint(r1==r2); break;
    case 140 /* equs */: popset(s2); popset(s1); pshint(s1==s2); break;
    case 142 /* equm */: getq(); popadr(&a2); popadr(&a1);
                         compare(&b, &a1, &a2); pshint(b); break;

    case 18  /* neqa */: popadr(&a2); popadr(&a1); pshint(a1!=a2); break;
    case 145 /* neqb */:
    case 147 /* neqc */:
    case 143 /* neqi */: popint(&i2); popint(&i1); pshint(i1!=i2); break;
    case 144 /* neqr */: poprel(&r2); poprel(&r1); pshint(r1!=r2); break;
    case 146 /* neqs */: popset(s2); popset(s1); pshint(s1!=s2); break;
    case 148 /* neqm */: getq(); popadr(&a2); popadr(&a1);
                         compare(&b, &a1, &a2); pshint(!b); break;

    case 151 /* geqb */:
    case 153 /* geqc */:
    case 149 /* geqi */: popint(&i2); popint(&i1); pshint(i1>=i2); break;
    case 150 /* geqr */: poprel(&r2); poprel(&r1); pshint(r1>=r2); break;
    case 152 /* geqs */: popset(s2); popset(s1); pshint(s1>=s2); break;
    case 154 /* geqm */: getq(); popadr(&a2); popadr(&a1);
                         compare(&b, &a1, &a2);
                         pshint(b || (store[a1] >= store[a2])); break;

    case 157 /* grtb */:
    case 159 /* grtc */:
    case 155 /* grti */: popint(&i2); popint(&i1); pshint(i1>i2); break;
    case 156 /* grtr */: poprel(&r2); poprel(&r1); pshint(r1>r2); break;
    case 158 /* grts */: errorv(SETINCLUSION); break;
    case 160 /* grtm */: getq(); popadr(&a2); popadr(&a1);
                         compare(&b, &a1, &a2);
                         pshint(!b && (store[a1] > store[a2])); break;

    case 163 /* leqb */:
    case 165 /* leqc */:
    case 161 /* leqi */: popint(&i2); popint(&i1); pshint(i1<=i2); break;
    case 162 /* leqr */: poprel(&r2); poprel(&r1); pshint(r1<=r2); break;
    case 164 /* leqs */: popset(s2); popset(s1); pshint(s1<=s2); break;
    case 166 /* leqm */: getq(); popadr(&a2); popadr(&a1);
                         compare(&b, &a1, &a2);
                         pshint(!b || (store[a1] <= store[a2])); break;

    case 169 /* lesb */:
    case 171 /* lesc */:
    case 167 /* lesi */: popint(&i2); popint(&i1); pshint(i1<i2); break;
    case 168 /* lesr */: poprel(&r2); poprel(&r1); pshint(r1<r2); break;
    case 170 /* less */: errorv(SETINCLUSION); break;
    case 172 /* lesm */: getq(); popadr(&a2); popadr(&a1);
                         compare(&b, &a1, &a2);
                         pshint(!b && (store[a1] < store[a2])); break;

    case 23 /*ujp*/: getq(); pc = q; break;
    case 24 /*fjp*/: getq(); popint(&i); if (i = 0) pc = q; break;
    case 25 /*xjp*/: getq(); popint(&i1); pc = i1*UJPLEN+q; break;

    case 95 /*chka*/:
    case 190 /*ckla*/: getq(); popadr(&a1); pshadr(a1);
                       /*     0 = assign pointer including nil
                         Not 0 = assign pointer from heap address */
                       if (a1 == 0)
                          /* if zero, but not nil, it's never been assigned */
                          errorv(UNINITIALIZEDPOINTER);
                       else if (q != 0 && a1 == NILVAL)
                          /* q != 0 means deref, and it was nil
                            (which is not zero) */
                          errorv(DEREFERENCEOFNILPOINTER);
                       else if ((a1 < gbtop || a1 >= np) &&
                               (a1 != NILVAL))
                          /* outside heap space (which could have
                            contracted!) */
                          errorv(BADPOINTERVALUE);
                       else if (DOCHKRPT && a1 != NILVAL) {
                         /* perform use of freed space check */
                         if (isfree(a1))
                           /* attempt to dereference or assign a freed
                             block */
                           errorv(POINTERUSEDAFTERDISPOSE);
                       }
                       break;
    case 97 /*chks*/: getq(); popset(s1); pshset(s1);
                      for (j = SETLOW; j <= getint(q)-1; j++)
                        if (sisin(j, s1)) errorv(SETELEMENTOUTOFRANGE);
                      for (j = getint(q+INTSIZE)+1; j <= SETHIGH; j++)
                        if (sisin(j, s1)) errorv(SETELEMENTOUTOFRANGE);
                      break;
    case 98 /*chkb*/:
    case 99 /*chkc*/:
    case 199 /* chkx */:
    case 26 /*chki*/: getq(); popint(&i1); pshint(i1);
                  if (i1 < getint(q) || i1 > getint(q+INTSIZE))
                    errore(VALUEOUTOFRANGE);
                  break;

    case 187 /* cks */: pshint(0);
    case 175 /* ckvi */:
    case 203 /* ckvx */:
    case 179 /* ckvb */:
    case 180 /* ckvc */: getq(); popint(&i2); popint(&i1);
                    pshint(i1); pshint(i1 = q || i2 != 0);
                    break;;
    case 188 /* cke */: popint(&i2); popint(&i1);
                    if (i2 == 0) errorv(VARIANTNOTACTIVE);
                    break;

    /* all the dups are defined, but not all used */
    case 185 /* dupb */:
    case 186 /* dupc */:
    case 181 /* dupi */: popint(&i1); pshint(i1); pshint(i1); break;
    case 182 /* dupa */: popadr(&a1); pshadr(a1); pshadr(a1); break;
    case 183 /* dupr */: poprel(&r1); pshrel(r1); pshrel(r1); break;
    case 184 /* dups */: popset(s1); pshset(s1); pshset(s1); break;

    case 189 /* inv */: popadr(&ad); putdef(ad, FALSE); break;

    case 28 /*adi*/: popint(&i2); popint(&i1);
                  if (DOCHKOVF) if (i1<0 == i2<0)
                    if (INT_MAX-abs(i1) < abs(i2)) errore(INTEGERVALUEOVERFLOW);
                  pshint(i1+i2); break;
    case 29 /*adr*/: poprel(&r2); poprel(&r1); pshrel(r1+r2); break;
    case 30 /*sbi*/: popint(&i2); popint(&i1);
                  if (DOCHKOVF) if (i1<0 != i2<0)
                    if (INT_MAX-abs(i1) < abs(i2)) errore(INTEGERVALUEOVERFLOW);
                  pshint(i1-i2); break;
    case 31 /*sbr*/: poprel(&r2); poprel(&r1); pshrel(r1-r2); break;
    case 32 /*sgs*/: popint(&i1); sset(s1, i); pshset(s1); break;
    case 33 /*flt*/: popint(&i1); pshrel(i1); break;

    /* note that flo implies the tos is float as well */
    case 34 /*flo*/: poprel(&r1); popint(&i1); pshrel(i1); pshrel(r1); break;

    case 35 /*trc*/: poprel(&r1);
                  if (DOCHKOVF) if (r1 < -INT_MAX || r1 > INT_MAX)
                    errore(REALARGUMENTTOOLARGE);
                  pshint(trunc(r1)); break;
    case 36 /*ngi*/: popint(&i1); pshint(-i1); break;
    case 37 /*ngr*/: poprel(&r1); pshrel(-r1); break;
    case 38 /*sqi*/: popint(&i1);
                if (DOCHKOVF) if (i1 != 0)
                  if (abs(i1) > INT_MAX % abs(i1)) errore(INTEGERVALUEOVERFLOW);
                pshint(i1*i1); break;
    case 39 /*sqr*/: poprel(&r1); pshrel(r1*r1); break;
    case 40 /*abi*/: popint(&i1); pshint(abs(i1)); break;
    case 41 /*abr*/: poprel(&r1); pshrel(abs(r1)); break;
    case 42 /*notb*/: popint(&i1); b1 = i1 != 0; pshint(!b1); break;
    case 205 /*noti*/: popint(&i1);
                      if (i1 < 0) errore(BOOLEANOPERATOROFNEGATIVE);
                      pshint(~i1); break;
    case 43 /*and*/: popint(&i2);
                      if (i2 < 0) errore(BOOLEANOPERATOROFNEGATIVE);
                      popint(&i1);
                      if (i1 < 0) errore(BOOLEANOPERATOROFNEGATIVE);
                      pshint(i1 & i2); break;
    case 44 /*ior*/: popint(&i2);
                      if (i2 < 0) errore(BOOLEANOPERATOROFNEGATIVE);
                      popint(&i1);
                      if (i1 < 0) errore(BOOLEANOPERATOROFNEGATIVE);
                      pshint(i1 | i2); break;
    case 206 /*xor*/: popint(&i2); b2 = i2 != 0;
                      if (i2 < 0) errore(BOOLEANOPERATOROFNEGATIVE);
                      popint(&i1); b1 = i1 != 0;
                      if (i1 < 0) errore(BOOLEANOPERATOROFNEGATIVE);
                      pshint(i1 ^ i2); break;
    case 45 /*dif*/: popset(s2); popset(s1); sdif(s1, s2); pshset(s1);
                     break;
    case 46 /*int*/: popset(s2); popset(s1); sint(s1, s2); pshset(s1);
                     break;
    case 47 /*uni*/: popset(s2); popset(s1); suni(s1, s2); pshset(s1);
                     break;
    case 48 /*inn*/: popset(s1); popint(&i1); pshint(sisin(i1, s1)); break;
    case 49 /*mod*/: popint(&i2); popint(&i1);
                  if (DOCHKOVF) if (i2 <= 0) errore(INVALIDDIVISORTOMOD);
                  pshint(i1 % i2); break;
    case 50 /*odd*/: popint(&i1); pshint(i1&1); break;
    case 51 /*mpi*/: popint(&i2); popint(&i1);
                  if (DOCHKOVF) if (i1 != 0 && i2 != 0)
                    if (abs(i1) > INT_MAX / abs(i2))
                      errore(INTEGERVALUEOVERFLOW);
                  pshint(i1*i2); break;
    case 52 /*mpr*/: poprel(&r2); poprel(&r1); pshrel(r1*r2); break;
    case 53 /*dvi*/: popint(&i2); popint(&i1); break;
                      if (DOCHKOVF) if (i2 == 0) errore(ZERODIVIDE);
                      pshint(i1/i2); break;
    case 54 /*dvr*/: poprel(&r2); poprel(&r1);
                      if (DOCHKOVF) if (r2 == 0.0) errore(ZERODIVIDE);
                      pshrel(r1/r2); break;
    case 55 /*mov*/: getq(); popint(&i2); popint(&i1);
                 for (i3 = 0; i3 <= q-1; i3++)
                   { store[i1+i3] = store[i2+i3];
                         putdef(i1+i3, getdef(i2+i3)); };
                 /* q is a number of storage units */
                 break;
    case 56 /*lca*/: getq(); pshadr(q); break;

    case 103 /*decb*/:
    case 104 /*decc*/:
    case 202 /*decx*/:
    case 57  /*deci*/: getq(); popint(&i1);
                    if (DOCHKOVF) if (i1<0 != q<0)
                      if (INT_MAX-abs(i1) < abs(q))
                        errore(INTEGERVALUEOVERFLOW);
                    pshint(i1-q); break;

    case 58 /*stp*/: stopins = TRUE; break;

    case 134 /*ordb*/:
    case 136 /*ordc*/:
    case 200 /*ordx*/:
    case 59  /*ordi*/: break; /* ord is a no-op */

    case 60 /*chr*/: break; /* chr is a no-op */

    case 61 /*ujc*/: errorv(INVALIDCASE); break;
    case 62 /*rnd*/: poprel(&r1);
                  if (DOCHKOVF) if (r1 < -(INT_MAX+0.5) || r1 > INT_MAX+0.5)
                    errore(REALARGUMENTTOOLARGE);
                  pshint(round(r1)); break;
    case 63 /*pck*/: getq(); getq1(); popadr(&a3); popadr(&a2); popadr(&a1);
                 if (a2+q > q1) errore(PACKELEMENTSOUTOFBOUNDS);
                 for (i4 = 0; i4 <= q-1; i4++) { chkdef(a1+a2);
                    store[a3+i4] = store[a1+a2];
                    putdef(a3+i4, getdef(a1+a2));
                    a2 = a2+1;
                 }
                 break;
    case 64 /*upk*/: getq(); getq1(); popadr(&a3); popadr(&a2); popadr(&a1);
                 if (a3+q > q1) errore(UNPACKELEMENTSOUTOFBOUNDS);
                 for (i4 = 0; i4 <= q-1; i4++) { chkdef(a1+i4);
                    store[a2+a3] = store[a1+i4];
                    putdef(a2+a3, getdef(a1+i4));
                    a3 = a3+1;
                 } break;

    case 110 /*rgs*/: popint(&i2); popint(&i1); rset(s1, i1, i2); pshset(s1);
                      break;
    case 112 /*ipj*/: getp(); getq(); pc = q;
                 mp = base(p); /* index the mark to restore */
                 /* restore marks until we reach the destination level */
                 sp = getadr(mp+MARKSB); /* get the stack bottom */
                 ep = getadr(mp+MARKET); /* get the mark ep */
                 break;
    case 113 /*cip*/: getp(); popadr(&ad);
                mp = sp+(p+MARKSIZE);
                /* replace next link mp with the one for the target */
                putadr(mp+MARKSL, getadr(ad+1*PTRSIZE));
                putadr(mp+MARKRA, pc);
                pc = getadr(ad);
                break;
    case 114 /*lpa*/: getp(); getq(); /* place procedure address on stack */
                pshadr(base(p));
                pshadr(q);
                break;
    case 117 /*dmp*/: getq(); sp = sp+q; break; /* remove top of stack */

    case 118 /*swp*/: getq(); swpstk(q); break;

    case 119 /*tjp*/: getq(); popint(&i); if (i == 0) pc = q; break;

    case 120 /*lip*/: getp(); getq(); ad = base(p) + q;
                   ad1 = getadr(ad); ad2 = getadr(ad+1*PTRSIZE);
                   pshadr(ad2); pshadr(ad1);
                   break;

    case 191 /*cta*/: getq(); getq1(); popint(&i); popadr(&ad); pshadr(ad);
                       pshint(i); ad = ad-q-INTSIZE; ad1 = getadr(ad);
                       if (ad1 < INTSIZE)
                         errorv(SYSTEMERROR);
                       ad1 = ad1-ADRSIZE-1;
                       if (ad1 >= q1) {
                         ad = ad-ad1*INTSIZE;
                         if (getadr(ad+(q1-1)*INTSIZE) != i)
                           errorv(CHANGETOALLOCATEDTAGFIELD);
                       }
                      break;

    /* ivt has issues */
    case 192 /*ivt*/: getq(); getq1(); popint(&i); popadr(&ad);
                      pshadr(ad); pshint(i);
                      if (FALSE && DOCHKDEF) {
                        b = getdef(ad);
                        if (b) b = i == getint(ad);
                        if (b) {
                          ad = ad+q;
                          for (j = 1; j <= q1; j++)
                            { putdef(ad, FALSE); ad = ad+1; }
                        }
                      }
                      break;

    case 174 /*mrkl*/: getq(); srclin = q; break;

    case 207 /*bge*/: getq();
                   /* save current exception framing */
                   pshadr(expadr); pshadr(expstk); pshadr(expmrk);
                   pshadr(0); /* place dummy vector */
                   /* place new exception frame */
                   expadr = q; expstk = sp; expmrk = mp;
                   break;
    case 208 /*ede*/: popadr(&a1); /* dispose vector */
                   /* restore previous exception frame */
                   popadr(&expmrk); popadr(&expstk); popadr(&expadr);
                   break;
    case 209 /*mse*/: popadr(&a1);
                   /* restore previous exception frame */
                   popadr(&expmrk); popadr(&expstk); popadr(&expadr);
                   /* if there is no surrounding frame, handle fixed */
                   if (expadr == 0) errorm(a1);
                   else { /* throw to new frame */
                     mp = expmrk; sp = expstk; pc = expadr;
                     popadr(&a2); pshadr(a1);
                     ep = getadr(mp+MARKET); /* get the mark ep */
                     /* release to search vectors */
                   }
                   break;
    case 8 /*cjp*/: getq(); getq1(); popint(&i1); pshint(i1);
                  if (i1 >= getint(q) && i1 <= getint(q+INTSIZE))
                    { pc = q1; popint(&i1); }
                  break;
    case 20 /*lnp*/: getq(); np = q; gbtop = np; ad = pctop;
                  /* clear global memory and set undefined */
                  while (np > ad)
                    { store[ad] = 0; putdef(ad, FALSE); ad = ad+1; }
                  break;
    case 21 /*cal*/: getq(); pshadr(pc); pc = q; break;
    case 22 /*ret*/: popadr(&pc); break;
    case 19 /*brk*/: break; /* breaks are no-ops here */

    /* illegal instructions */
    /* 27,  91,  92,  96, 100, 101, 102, 111, 115, 116, 121, 122, 133, 135,
      176, 177, 178, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
      221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234,
      235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248,
      249, 250, 251, 252, 253, 254, 255*/
    default: errorv(INVALIDINSTRUCTION); break;

  }
}

void main (int argc, char *argv[])

{
    FILE* fp;

    printf("P6 Pascal mach interpreter vs. %d.%d", MAJORVER, MINORVER);
    if (EXPERIMENT) printf(".x");
    printf("\n");
    printf("\n");

    argc--; argv++; /* discard the program parameter */

    /* initialize file state */
    for (i = 1; i <= MAXFIL; i++) {
        filstate[i] = fsclosed; /* closed */
        filanamtab[i] = FALSE; /* no name assigned */
    }

    /* construct bit equivalence table */
    i = 1;
    for (bai = 0; bai <= 7; bai++) { bitmsk[bai] = i; i = i*2; }

    for (sdi = 0; sdi <= MAXDEF; sdi++)
      storedef[sdi] = 0; /* clear storage defined flags */

#ifdef GPC
    /*
     * For GPC, we open the PRD and PRR files in advance. In Pascaline mode, we
     * treat all header files the same, which is to open as parameters from the
     * command line.
     */
    filtable[PRDFN] = fopen("prd", "r");
    if (!filtable[PRDFN]) {
        printf("*** Cannot open file prd\n");
        finish(1);
    }
    filstate[PRDFN] = fsread;

    filtable[PRRFN] = fopen("prr", "w");
    if (!filtable[PRRFN]) {
        printf("*** Cannot open file prr\n");
        finish(1);
    }
    filstate[PRRFN] = fswrite;

    fp = filtable[PRDFN]; /* set load file as prd */
#else
    if (*argc < 2) {
        printf("*** Usage: pmach <codefile> [<params>]...\n");
        finish(1);
    }
    fp = fopen(**argv, "r");
    if (!fp) {
        printf("*** Cannot open file %s\n", **argv);
        finish(1);
    }
    *argv++; *argc--; /* skip that parameter */
#endif
    printf("loading program\n");
    load(fp); /* assembles and stores code */
#ifndef GPC
    fclose(fp);
#endif

    /* get the command line */
    getcommandline(argc, argv, cmdlin, &cmdlen);
    cmdpos = 1;

    /* prep for the run */
    pc = 0; sp = MAXTOP; np = -1; mp = MAXTOP; ep = 5; srclin = 1;
    expadr = 0; expstk = 0; expmrk = 0;

    printf("Running program\n");
    printf("\n");
    do {
        stopins = FALSE; /* set no stop flag */
        sinins();
    } while (!stopins); /* until stop instruction is seen */

    finish(0); /* exit program with good status */

}