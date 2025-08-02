
#ifndef __MT5728SHA_H__
#define __MT5728SHA_H__

#include <stdbool.h>
/*
* If you do not have the ISO standard stdint.h header file, then you
* must typdef the following:
* name meaning
* unsigned int unsigned 32 bit integer
* unsigned char unsigned 8 bit integer (i.e., unsigned char)
* int_least16_t integer of >= 16 bits
*
*/
#ifndef _SHA_enum_
#define _SHA_enum_
enum
{
    shaSuccess = 0,
    shaNull,         /* Null pointer parameter */
    shaInputTooLong, /* input data too long */
    shaStateError    /* called Input after Result */
};
#endif

typedef enum {
    SHA_VERIFY_MODE   = 0,
    SHA_GENERATE_MODE = 1,
} sha_mode_t;

#define SHA1DIGEST_SIZE 20
/* This structure will hold context information for the SHA-1 hashing operation */
typedef struct SHA1Context
{
    unsigned int Intermediate_Hash[SHA1DIGEST_SIZE/4];     /* Message Digest */
    unsigned int Length_Low;                            /* Message length in bits */
    unsigned int Length_High;                           /* Message length in bits */

    /* Index into message block array */
    unsigned int /* _least16_t */ Message_Block_Index;
    unsigned char Message_Block[64];                           /* 512-bit message blocks */
    int Computed;                                   /* Is the digest computed? */
    int Corrupted;                                  /* Is the message digest corrupted? */
} SHA1Context;


#define PRIVATEKEY_SIZE 128
typedef struct Sha1Pkt {
    unsigned short key_start    : 7;
    unsigned short key_len      : 4;
    unsigned short digest_start : 5;
    
    unsigned char digest[3]; 
} __attribute__ ((packed)) Sha1Pkt;

/*
* Function Prototypes
*/
 
int SHA1Reset( SHA1Context *);
int SHA1Input( SHA1Context *,const unsigned char *,unsigned int);
int SHA1Result( SHA1Context *,unsigned char Message_Digest[SHA1DIGEST_SIZE]);
bool sha1_verify(unsigned char *msg, unsigned char mode);

#endif
