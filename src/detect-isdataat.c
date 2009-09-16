/**
 * \file
 * \author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 *
 * ISDATAAT part of the detection engine.
 */

#include <pcre.h>

#include "eidps-common.h"
#include "debug.h"
#include "decode.h"
#include "detect.h"

#include "util-unittest.h"

#include "detect-isdataat.h"
#include "detect-content.h"

#include "flow.h"
#include "flow-var.h"


/**
 * \brief Regex for parsing our isdataat options
 */
#define PARSE_REGEX  "^\\s*([0-9]{1,10})\\s*(,\\s*relative)?\\s*(,\\s*rawbytes\\s*)?\\s*$"

static pcre *parse_regex;
static pcre_extra *parse_regex_study;

int DetectIsdataatMatch (ThreadVars *, DetectEngineThreadCtx *, Packet *, Signature *, SigMatch *);
int DetectIsdataatSetup (DetectEngineCtx *, Signature *, SigMatch *, char *);
void DetectIsdataatRegisterTests(void);
void DetectIsdataatFree(void *);

/**
 * \brief Registration function for isdataat: keyword
 */
void DetectIsdataatRegister (void) {
    sigmatch_table[DETECT_ISDATAAT].name = "isdataat";
    sigmatch_table[DETECT_ISDATAAT].Match = DetectIsdataatMatch;
    sigmatch_table[DETECT_ISDATAAT].Setup = DetectIsdataatSetup;
    sigmatch_table[DETECT_ISDATAAT].Free  = DetectIsdataatFree;
    sigmatch_table[DETECT_ISDATAAT].RegisterTests = DetectIsdataatRegisterTests;

    const char *eb;
    int eo;
    int opts = 0;

    parse_regex = pcre_compile(PARSE_REGEX, opts, &eb, &eo, NULL);
    if(parse_regex == NULL)
    {
        printf("pcre compile of \"%s\" failed at offset %" PRId32 ": %s\n", PARSE_REGEX, eo, eb);
        goto error;
    }

    parse_regex_study = pcre_study(parse_regex, 0, &eb);
    if(eb != NULL)
    {
        printf("pcre study failed: %s\n", eb);
        goto error;
    }
    return;

error:
    /* XXX */
    return;
}

/**
 * \brief This function is used to match isdataat on a packet
 * \todo We need to add support for rawbytes
 *
 * \param t pointer to thread vars
 * \param det_ctx pointer to the pattern matcher thread
 * \param p pointer to the current packet
 * \param m pointer to the sigmatch that we will cast into DetectIsdataatData
 *
 * \retval 0 no match
 * \retval 1 match
 */
int DetectIsdataatMatch (ThreadVars *t, DetectEngineThreadCtx *det_ctx, Packet *p, Signature *s, SigMatch *m)
{
    DetectIsdataatData *idad = (DetectIsdataatData *)m->ctx;
    int ret=0;

    #ifdef DEBUG
    printf("detect-isdataat: payload len : %u , dataat? %u ; relative? %u ...\n", p->payload_len,idad->dataat,idad->flags &ISDATAAT_RELATIVE);
    #endif

    if(idad->flags & ISDATAAT_RELATIVE)
    {
        /* Relative to the last matched content, is not performed here */
        #ifdef DEBUG
        printf("detect-isdataat: Nothing now, this is checked in detect-content.c!\n");
        #endif
    }
    else
        if( !(idad->flags & ISDATAAT_RELATIVE) && p->payload_len >= idad->dataat) {
            ret=1; /* its not relative and we have more data in the packet than the offset of isdataat */
            #ifdef DEBUG
            printf("detect-isdataat: matched with payload len : %u , dataat? %u ; relative? %u ...\n", p->payload_len,idad->dataat,idad->flags &ISDATAAT_RELATIVE);
            #endif
        }

    return ret;
}

/**
 * \brief This function is used to parse isdataat options passed via isdataat: keyword
 *
 * \param isdataatstr Pointer to the user provided isdataat options
 *
 * \retval idad pointer to DetectIsdataatData on success
 * \retval NULL on failure
 */
DetectIsdataatData *DetectIsdataatParse (char *isdataatstr)
{
    DetectIsdataatData *idad = NULL;
    char *args[3] = {NULL,NULL,NULL};
#define MAX_SUBSTRINGS 30
    int ret = 0, res = 0;
    int ov[MAX_SUBSTRINGS];
    int i=0;

    ret = pcre_exec(parse_regex, parse_regex_study, isdataatstr, strlen(isdataatstr), 0, 0, ov, MAX_SUBSTRINGS);
    if (ret < 1 || ret > 4) {
        goto error;
    }

    if (ret > 1) {
        const char *str_ptr;
        res = pcre_get_substring((char *)isdataatstr, ov, MAX_SUBSTRINGS, 1, &str_ptr);
        if (res < 0) {
            printf("DetectIsdataatParse: pcre_get_substring failed\n");
            goto error;
        }
        args[0] = (char *)str_ptr;


        if (ret > 2) {
            res = pcre_get_substring((char *)isdataatstr, ov, MAX_SUBSTRINGS, 2, &str_ptr);
            if (res < 0) {
                printf("DetectIsdataatParse: pcre_get_substring failed\n");
                goto error;
            }
            args[1] = (char *)str_ptr;
        }
        if (ret > 3) {
            res = pcre_get_substring((char *)isdataatstr, ov, MAX_SUBSTRINGS, 3, &str_ptr);
            if (res < 0) {
                printf("DetectIsdataatParse: pcre_get_substring failed\n");
                goto error;
            }
            args[2] = (char *)str_ptr;
        }

        idad = malloc(sizeof(DetectIsdataatData));
        if (idad == NULL) {
            printf("DetectIsdataatParse malloc failed\n");
            goto error;
        }

        idad->flags = 0;
        idad->dataat= 0;

        if(args[0] != NULL)
            idad->dataat=atoi(args[0]);

        if(idad->dataat < ISDATAAT_MIN || idad->dataat > ISDATAAT_MAX) {
            printf("detect-isdataat: DetectIsdataatParse: isdataat out of range\n");
            free(idad);
            idad=NULL;
            goto error;
        }

        if(args[1] !=NULL)
        {
            idad->flags |= ISDATAAT_RELATIVE;

            if(args[2] !=NULL)
                idad->flags |= ISDATAAT_RAWBYTES;
        }

        for (i = 0; i < (ret -1); i++){
            if (args[i] != NULL) free(args[i]);
        }

        return idad;

    }

error:

    for (i = 0; i < (ret -1); i++){
        if (args[i] != NULL) free(args[i]);
    }

    if (idad != NULL) DetectIsdataatFree(idad);
    return NULL;

}

/**
 * \brief this function is used to add the parsed isdataatdata into the current signature
 *
 * \param de_ctx pointer to the Detection Engine Context
 * \param s pointer to the Current Signature
 * \param m pointer to the Current SigMatch
 * \param isdataatstr pointer to the user provided isdataat options
 *
 * \retval 0 on Success
 * \retval -1 on Failure
 */
int DetectIsdataatSetup (DetectEngineCtx *de_ctx, Signature *s, SigMatch *m, char *isdataatstr)
{
    DetectIsdataatData *idad = NULL;
    SigMatch *sm = NULL;
    SigMatch *search_sm_content = NULL;
    DetectContentData *cd = NULL;


    idad = DetectIsdataatParse(isdataatstr);
    if (idad == NULL) goto error;

    if(idad->flags & ISDATAAT_RELATIVE)
    {
        /// Set it in the last parsed contet because it is relative to that content match
        #ifdef DEBUG
        printf("detect-isdataat: Set it in the last parsed contet because it is relative to that content match\n");
        #endif

        if( m == NULL )
        {
            printf("detect-isdataat: No previous content, the flag 'relative' cant be used without content\n");
            goto  error;
        }
        else
        {
            search_sm_content=m;
            /// Searching last content
            uint8_t found=0;
            while(search_sm_content != NULL && !found)
            {
                if(search_sm_content->type== DETECT_CONTENT) //Found content
                    found=1;
                else
                    search_sm_content=search_sm_content->prev;
            }

            if(search_sm_content != NULL)
            {
                /* Found */
                cd=(DetectContentData*)search_sm_content->ctx;
                if(cd != NULL)
                {
                    cd->flags |= DETECT_CONTENT_ISDATAAT_RELATIVE;
                    cd->isdataat=idad->dataat;
                }
                else
                {
                    printf("detect-isdataat: No content data found in a SigMatch of DETECT_CONTENT type\n");
                    goto error;
                }
            }
            else
            {
                printf("detect-isdataat: No previous content, the flag 'relative' cant be used without content\n");
                goto  error;
            }

        }
    }
    else
    {
        #ifdef DEBUG
        printf("detect-isdataat: Set it as a normal SigMatch\n");
        #endif
        /// else Set it as a normal SigMatch
        sm = SigMatchAlloc();
        if (sm == NULL)
            goto error;

        sm->type = DETECT_ISDATAAT;
        sm->ctx = (void *)idad;

        SigMatchAppend(s,m,sm);
    }

    return 0;

error:
    if (idad != NULL) DetectIsdataatFree(idad);
    if (sm != NULL) free(sm);
    return -1;

}

/**
 * \brief this function will free memory associated with DetectIsdataatData
 *
 * \param idad pointer to DetectIsdataatData
 */
void DetectIsdataatFree(void *ptr) {
    DetectIsdataatData *idad = (DetectIsdataatData *)ptr;
    free(idad);
}


#ifdef UNITTESTS

/**
 * \test DetectIsdataatTestParse01 is a test to make sure that we return a correct IsdataatData structure
 *  when given valid isdataat opt
 */
int DetectIsdataatTestParse01 (void) {
    int result = 0;
    DetectIsdataatData *idad = NULL;
    idad = DetectIsdataatParse("30 ");
    if (idad != NULL) {
        DetectIsdataatFree(idad);
        result = 1;
    }

    return result;
}

/**
 * \test DetectIsdataatTestParse02 is a test to make sure that we return a correct IsdataatData structure
 *  when given valid isdataat opt
 */
int DetectIsdataatTestParse02 (void) {
    int result = 0;
    DetectIsdataatData *idad = NULL;
    idad = DetectIsdataatParse("30 , relative");
    if (idad != NULL && idad->flags & ISDATAAT_RELATIVE && !(idad->flags & ISDATAAT_RAWBYTES)) {
        DetectIsdataatFree(idad);
        result = 1;
    }

    return result;
}

/**
 * \test DetectIsdataatTestParse03 is a test to make sure that we return a correct IsdataatData structure
 *  when given valid isdataat opt
 */
int DetectIsdataatTestParse03 (void) {
    int result = 0;
    DetectIsdataatData *idad = NULL;
    idad = DetectIsdataatParse("30,relative, rawbytes ");
    if (idad != NULL && idad->flags & ISDATAAT_RELATIVE && idad->flags & ISDATAAT_RAWBYTES) {
        DetectIsdataatFree(idad);
        result = 1;
    }

    return result;
}

/**
 * \test DetectIsdataatTestPacket01 is a test to check if the packet has data at 50 bytes offset non relative
 *
 */

int DetectIsdataatTestPacket01 (void) {
    int result = 0;
    DetectIsdataatData *idad = NULL;

    /// Parse Isdataat Data: if packet data len is greater or equal than 50 byte it should match
    /// The packet has 190 bytes of data so it must match
    idad = DetectIsdataatParse("50");
    if (idad == NULL)
    {
        printf("DetectIsdataatTestPacket01: expected a DetectIsdataatData pointer (got NULL)\n");
        return 0;
    }
    /* Buid and decode the packet */
    uint8_t raw_eth [] = {
    0x00,0x25,0x00,0x9e,0xfa,0xfe,0x00,0x02,0xcf,0x74,0xfe,0xe1,0x08,0x00,0x45,0x00
	,0x01,0xcc,0xcb,0x91,0x00,0x00,0x34,0x06,0xdf,0xa8,0xd1,0x55,0xe3,0x67,0xc0,0xa8
	,0x64,0x8c,0x00,0x50,0xc0,0xb7,0xd1,0x11,0xed,0x63,0x81,0xa9,0x9a,0x05,0x80,0x18
	,0x00,0x75,0x0a,0xdd,0x00,0x00,0x01,0x01,0x08,0x0a,0x09,0x8a,0x06,0xd0,0x12,0x21
	,0x2a,0x3b,0x48,0x54,0x54,0x50,0x2f,0x31,0x2e,0x31,0x20,0x33,0x30,0x32,0x20,0x46
	,0x6f,0x75,0x6e,0x64,0x0d,0x0a,0x4c,0x6f,0x63,0x61,0x74,0x69,0x6f,0x6e,0x3a,0x20
	,0x68,0x74,0x74,0x70,0x3a,0x2f,0x2f,0x77,0x77,0x77,0x2e,0x67,0x6f,0x6f,0x67,0x6c
	,0x65,0x2e,0x65,0x73,0x2f,0x0d,0x0a,0x43,0x61,0x63,0x68,0x65,0x2d,0x43,0x6f,0x6e
	,0x74,0x72,0x6f,0x6c,0x3a,0x20,0x70,0x72,0x69,0x76,0x61,0x74,0x65,0x0d,0x0a,0x43
	,0x6f,0x6e,0x74,0x65,0x6e,0x74,0x2d,0x54,0x79,0x70,0x65,0x3a,0x20,0x74,0x65,0x78
	,0x74,0x2f,0x68,0x74,0x6d,0x6c,0x3b,0x20,0x63,0x68,0x61,0x72,0x73,0x65,0x74,0x3d
	,0x55,0x54,0x46,0x2d,0x38,0x0d,0x0a,0x44,0x61,0x74,0x65,0x3a,0x20,0x4d,0x6f,0x6e
	,0x2c,0x20,0x31,0x34,0x20,0x53,0x65,0x70,0x20,0x32,0x30,0x30,0x39,0x20,0x30,0x38
	,0x3a,0x34,0x38,0x3a,0x33,0x31,0x20,0x47,0x4d,0x54,0x0d,0x0a,0x53,0x65,0x72,0x76
	,0x65,0x72,0x3a,0x20,0x67,0x77,0x73,0x0d,0x0a,0x43,0x6f,0x6e,0x74,0x65,0x6e,0x74
	,0x2d,0x4c,0x65,0x6e,0x67,0x74,0x68,0x3a,0x20,0x32,0x31,0x38,0x0d,0x0a,0x0d,0x0a
	,0x3c,0x48,0x54,0x4d,0x4c,0x3e,0x3c,0x48,0x45,0x41,0x44,0x3e,0x3c,0x6d,0x65,0x74
	,0x61,0x20,0x68,0x74,0x74,0x70,0x2d,0x65,0x71,0x75,0x69,0x76,0x3d,0x22,0x63,0x6f
	,0x6e,0x74,0x65,0x6e,0x74,0x2d,0x74,0x79,0x70,0x65,0x22,0x20,0x63,0x6f,0x6e,0x74
	,0x65,0x6e,0x74,0x3d,0x22,0x74,0x65,0x78,0x74,0x2f,0x68,0x74,0x6d,0x6c,0x3b,0x63
	,0x68,0x61,0x72,0x73,0x65,0x74,0x3d,0x75,0x74,0x66,0x2d,0x38,0x22,0x3e,0x0a,0x3c
	,0x54,0x49,0x54,0x4c,0x45,0x3e,0x33,0x30,0x32,0x20,0x4d,0x6f,0x76,0x65,0x64,0x3c
	,0x2f,0x54,0x49,0x54,0x4c,0x45,0x3e,0x3c,0x2f,0x48,0x45,0x41,0x44,0x3e,0x3c,0x42
	,0x4f,0x44,0x59,0x3e,0x0a,0x3c,0x48,0x31,0x3e,0x33,0x30,0x32,0x20,0x4d,0x6f,0x76
	,0x65,0x64,0x3c,0x2f,0x48,0x31,0x3e,0x0a,0x54,0x68,0x65,0x20,0x64,0x6f,0x63,0x75
	,0x6d,0x65,0x6e,0x74,0x20,0x68,0x61,0x73,0x20,0x6d,0x6f,0x76,0x65,0x64,0x0a,0x3c
	,0x41,0x20,0x48,0x52,0x45,0x46,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,0x2f,0x77
	,0x77,0x77,0x2e,0x67,0x6f,0x6f,0x67,0x6c,0x65,0x2e,0x65,0x73,0x2f,0x22,0x3e,0x68
	,0x65,0x72,0x65,0x3c,0x2f,0x41,0x3e,0x2e,0x0d,0x0a,0x3c,0x2f,0x42,0x4f,0x44,0x59
	,0x3e,0x3c,0x2f,0x48,0x54,0x4d,0x4c,0x3e,0x0d,0x0a };

    Packet q;
    ThreadVars tv;
    DecodeThreadVars dtv;

    memset(&tv, 0, sizeof(ThreadVars));
    memset(&q, 0, sizeof(Packet));
    memset(&dtv, 0, sizeof(DecodeThreadVars));

    FlowInitConfig(FLOW_QUIET);
    DecodeEthernet(&tv, &dtv, &q, raw_eth, sizeof(raw_eth), NULL);
    FlowShutdown();

    Packet *p=&q;

    if (!(PKT_IS_TCP(p))) {
        printf("detect-window: TestPacket01: Packet is not TCP\n");
        return 0;
    }


    /* We dont need DetectEngineThreadCtx inside DetectIsdataatMatch, its just to pass it to
     the function, since this is a test for this option
     Also a Signature is not really needed
    */

    DetectEngineThreadCtx *det_ctx=NULL;
    Signature *s=NULL;

    /* The data of DetectIsdataatData is retrieved inside DetectIsdataatMatch
     from a SigMatch struct, so creating a temporal SigMatch
    */
    SigMatch m;
    m.ctx=idad;

    /* Now that we have what we need, just try to Match! */
    result=DetectIsdataatMatch (&tv, det_ctx, p, s, &m);

    return result;
}


/**
 * \test DetectIsdataatTestPacket02 is a test to check if the packet match 6000 bytes offset non relative (it wont)
 *
 */

int DetectIsdataatTestPacket02 (void) {
    int result = 0;
    DetectIsdataatData *idad = NULL;

    /// Parse Isdataat Data: if packet data len is greater or equal than 6000 byte it should match
    /// The packet has 190 bytes of data so it must not match
    idad = DetectIsdataatParse("6000");
    if (idad == NULL)
    {
        printf("DetectIsdataatTestPacket01: expected a DetectIsdataatData pointer (got NULL)\n");
        return 0;
    }
    /* Buid and decode the packet */
    uint8_t raw_eth [] = {
    0x00,0x25,0x00,0x9e,0xfa,0xfe,0x00,0x02,0xcf,0x74,0xfe,0xe1,0x08,0x00,0x45,0x00
	,0x01,0xcc,0xcb,0x91,0x00,0x00,0x34,0x06,0xdf,0xa8,0xd1,0x55,0xe3,0x67,0xc0,0xa8
	,0x64,0x8c,0x00,0x50,0xc0,0xb7,0xd1,0x11,0xed,0x63,0x81,0xa9,0x9a,0x05,0x80,0x18
	,0x00,0x75,0x0a,0xdd,0x00,0x00,0x01,0x01,0x08,0x0a,0x09,0x8a,0x06,0xd0,0x12,0x21
	,0x2a,0x3b,0x48,0x54,0x54,0x50,0x2f,0x31,0x2e,0x31,0x20,0x33,0x30,0x32,0x20,0x46
	,0x6f,0x75,0x6e,0x64,0x0d,0x0a,0x4c,0x6f,0x63,0x61,0x74,0x69,0x6f,0x6e,0x3a,0x20
	,0x68,0x74,0x74,0x70,0x3a,0x2f,0x2f,0x77,0x77,0x77,0x2e,0x67,0x6f,0x6f,0x67,0x6c
	,0x65,0x2e,0x65,0x73,0x2f,0x0d,0x0a,0x43,0x61,0x63,0x68,0x65,0x2d,0x43,0x6f,0x6e
	,0x74,0x72,0x6f,0x6c,0x3a,0x20,0x70,0x72,0x69,0x76,0x61,0x74,0x65,0x0d,0x0a,0x43
	,0x6f,0x6e,0x74,0x65,0x6e,0x74,0x2d,0x54,0x79,0x70,0x65,0x3a,0x20,0x74,0x65,0x78
	,0x74,0x2f,0x68,0x74,0x6d,0x6c,0x3b,0x20,0x63,0x68,0x61,0x72,0x73,0x65,0x74,0x3d
	,0x55,0x54,0x46,0x2d,0x38,0x0d,0x0a,0x44,0x61,0x74,0x65,0x3a,0x20,0x4d,0x6f,0x6e
	,0x2c,0x20,0x31,0x34,0x20,0x53,0x65,0x70,0x20,0x32,0x30,0x30,0x39,0x20,0x30,0x38
	,0x3a,0x34,0x38,0x3a,0x33,0x31,0x20,0x47,0x4d,0x54,0x0d,0x0a,0x53,0x65,0x72,0x76
	,0x65,0x72,0x3a,0x20,0x67,0x77,0x73,0x0d,0x0a,0x43,0x6f,0x6e,0x74,0x65,0x6e,0x74
	,0x2d,0x4c,0x65,0x6e,0x67,0x74,0x68,0x3a,0x20,0x32,0x31,0x38,0x0d,0x0a,0x0d,0x0a
	,0x3c,0x48,0x54,0x4d,0x4c,0x3e,0x3c,0x48,0x45,0x41,0x44,0x3e,0x3c,0x6d,0x65,0x74
	,0x61,0x20,0x68,0x74,0x74,0x70,0x2d,0x65,0x71,0x75,0x69,0x76,0x3d,0x22,0x63,0x6f
	,0x6e,0x74,0x65,0x6e,0x74,0x2d,0x74,0x79,0x70,0x65,0x22,0x20,0x63,0x6f,0x6e,0x74
	,0x65,0x6e,0x74,0x3d,0x22,0x74,0x65,0x78,0x74,0x2f,0x68,0x74,0x6d,0x6c,0x3b,0x63
	,0x68,0x61,0x72,0x73,0x65,0x74,0x3d,0x75,0x74,0x66,0x2d,0x38,0x22,0x3e,0x0a,0x3c
	,0x54,0x49,0x54,0x4c,0x45,0x3e,0x33,0x30,0x32,0x20,0x4d,0x6f,0x76,0x65,0x64,0x3c
	,0x2f,0x54,0x49,0x54,0x4c,0x45,0x3e,0x3c,0x2f,0x48,0x45,0x41,0x44,0x3e,0x3c,0x42
	,0x4f,0x44,0x59,0x3e,0x0a,0x3c,0x48,0x31,0x3e,0x33,0x30,0x32,0x20,0x4d,0x6f,0x76
	,0x65,0x64,0x3c,0x2f,0x48,0x31,0x3e,0x0a,0x54,0x68,0x65,0x20,0x64,0x6f,0x63,0x75
	,0x6d,0x65,0x6e,0x74,0x20,0x68,0x61,0x73,0x20,0x6d,0x6f,0x76,0x65,0x64,0x0a,0x3c
	,0x41,0x20,0x48,0x52,0x45,0x46,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,0x2f,0x77
	,0x77,0x77,0x2e,0x67,0x6f,0x6f,0x67,0x6c,0x65,0x2e,0x65,0x73,0x2f,0x22,0x3e,0x68
	,0x65,0x72,0x65,0x3c,0x2f,0x41,0x3e,0x2e,0x0d,0x0a,0x3c,0x2f,0x42,0x4f,0x44,0x59
	,0x3e,0x3c,0x2f,0x48,0x54,0x4d,0x4c,0x3e,0x0d,0x0a };

    Packet q;
    ThreadVars tv;
    DecodeThreadVars dtv;

    memset(&tv, 0, sizeof(ThreadVars));
    memset(&q, 0, sizeof(Packet));
    memset(&dtv, 0, sizeof(DecodeThreadVars));

    FlowInitConfig(FLOW_QUIET);
    DecodeEthernet(&tv, &dtv, &q, raw_eth, sizeof(raw_eth), NULL);
    FlowShutdown();

    Packet *p=&q;

    if (!(PKT_IS_TCP(p))) {
        printf("detect-window: TestPacket02: Packet is not TCP\n");
        return 0;
    }


    /* We dont need DetectEngineThreadCtx inside DetectIsdataatMatch, its just to pass it to
     the function, since this is a test for this option
     Also a Signature is not really needed
    */

    DetectEngineThreadCtx *det_ctx=NULL;
    Signature *s=NULL;

    /* The data of DetectIsdataatData is retrieved inside DetectIsdataatMatch
     from a SigMatch struct, so creating a temporal SigMatch
    */

    SigMatch m;

    m.ctx=idad;

    /* Now that we have what we need, just try to Match! */
    result=DetectIsdataatMatch (&tv, det_ctx, p, s, &m);

    /* Invert it, we dont want this packet to match */
    result=!result;
    if(result==0)
        printf("detect-isdataat: It has matched with isdataat 6000, expecting not to match\n");

    return result;
}

#endif

/**
 * \brief this function registers unit tests for DetectIsdataat
 */
void DetectIsdataatRegisterTests(void) {
    #ifdef UNITTESTS
    UtRegisterTest("DetectIsdataatTestParse01", DetectIsdataatTestParse01, 1);
    UtRegisterTest("DetectIsdataatTestParse02", DetectIsdataatTestParse02, 1);
    UtRegisterTest("DetectIsdataatTestParse03", DetectIsdataatTestParse03, 1);
    UtRegisterTest("DetectIsdataatTestPacket01", DetectIsdataatTestPacket01, 1);
    UtRegisterTest("DetectIsdataatTestPacket02", DetectIsdataatTestPacket02, 1);
    #endif
}
