/*
 * clickatell_string.c
 *
 *  Basic String functions.
 *
 *  Martin Beyers <martin.beyers@clickatell.com>
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "clickatell_debug.h"
#include "clickatell_string.h"

/* ----------------------------------------------------------------------------- *
 * Macros/Types                                                                  *
 * ----------------------------------------------------------------------------- */

// character array designating a URL-safe character range
#define URL_ENCODE_SAFE_CHAR(c) (((c) >= '0' && (c) <= '9') || \
                                    ((c) >= 'A' && (c) <= 'Z') || \
                                    ((c) >= 'a' && (c) <= 'z') || \
                                    (c) == '-' || (c) == '_' || (c) == '.' || (c) == '~')

/* ----------------------------------------------------------------------------- *
 * Forward declarations                                                          *
 * ----------------------------------------------------------------------------- */

static ClickSmsString *local_string_create(const char *chStr, int iLenBuffer);

/* ----------------------------------------------------------------------------- *
 * Local functions                                                               *
 * ----------------------------------------------------------------------------- */

/*
 * Function:  local_string_create
 * Info:      Local common function to create a new ClickSmsString.
 * Inputs:    sBuf       - pointer to buffer that acts as sSource data for our ClickSmsString.
 *            iLenBuffer   - length of string.
 * Return:    new ClickSmsString if successful, else NULL if failed to allocate
 *            new ClickSmsString.
 */
static ClickSmsString *local_string_create(const char *chStr, int iLenBuffer)
{
    ClickSmsString *sOutput = (ClickSmsString *)malloc(sizeof(ClickSmsString));
    if (sOutput != NULL) {
        sOutput->data = malloc(iLenBuffer + 1);

        if (sOutput != NULL) {
            strncpy(sOutput->data, chStr, iLenBuffer); // copy sSource buffer (including '\0') to destination string
            sOutput->data[iLenBuffer] = '\0';         // ensure string terminator exists if it did not in the 'chStr' parameter
        }
        else
            click_debug_print("%s ERROR: Failed to allocate memory for ClickSmsString data!\n", __func__);
    }
    else
        click_debug_print("%s ERROR: Failed to allocate memory for ClickSmsString!\n", __func__);

    return sOutput;
}

/* ----------------------------------------------------------------------------- *
 * Public functions                                                              *
 * ----------------------------------------------------------------------------- */

/*
 * Function:  click_string_append
 * Info:      Appends a sSource string to a destination string
 *            If 'sSource' is not NULL, then attempt to append ClickSmsString 'sSource'.
 *            If 'sSource' is NULL, then attempt to append char string 'chSource'.
 *            If append eRequestType is successful then ClickSmsString 'sDest' structure returned
 *            will hold the reallocated buffer.
 *            If append eRequestType is unsuccessful ClickSmsString 'sDest' structure remains
 *            unchanged.
 * Inputs:    sDest        - target string to append to
 *            sSource      - string to append
 *            chSource - string to append if 'sSource' argument is NULL
 * Return:    void
 */
void click_string_append(ClickSmsString *sDest, const ClickSmsString *sSource, const char *chSource)
{
    if (CLICK_STR_INVALID(sDest) ||
        (sSource == NULL && chSource == NULL) ||
        (sSource != NULL && (CLICK_STR_INVALID(sSource) || strlen(sDest->data) < 1)) ||
        (sSource == NULL && chSource != NULL && strlen(chSource) < 1))
    {
        click_debug_print("%s ERROR: Invalid parameter!\n", __func__);
        return;
    }

    char *chReallocatedStr = NULL;
    int iAppendLen = strlen((sSource != NULL ? sSource->data : chSource));
    int iNewLen    = strlen(sDest->data) + iAppendLen + 1;

    // reallocate destination data memory buffer and then concatenate strings
    if ((chReallocatedStr = realloc((void *)(sDest->data), (size_t)iNewLen)) != NULL) {
        sDest->data = chReallocatedStr;
        strcat(sDest->data, (sSource != NULL ? sSource->data : chSource));
    }
    else
        click_debug_print("%s ERROR: Failed to allocate memory for appended string!\n", __func__);
}

/*
 * Function:  click_string_append_formatted_cstr
 * Info:      Appends a formatted sSource string to a destination ClickSmsString
 *            If append eRequestType is unsuccessful ClickSmsString 'sDest' structure remains
 *            unchanged.
 * Inputs:    sDest             - target string to append to
 *            formatted string - string with variable aDests of args to append
 * Return:    void
 */
void click_string_append_formatted_cstr(ClickSmsString *sDest, const char *chFormat, ...)
{
    if (CLICK_STR_INVALID(sDest)) {
        click_debug_print("%s ERROR: Invalid parameter!\n", __func__);
        return;
    }

    char *tmp_cstr = NULL;
    char *chReallocatedStr = NULL;
    int iNewLen = 0;
    int iOldLen = strlen(sDest->data);
    va_list argList, arg_list_copy;

    va_start(argList, chFormat);

    // can only pass va_list once to vsnprintf(), so make a copy for the 2nd vsnprintf() call
    va_copy(arg_list_copy, argList);
    /* obtain length of formatted characters for dynamic malloc eRequestType
     *  note that 2nd arg of vsnprintf() here is zero to ensure vsnprintf() does not write to 1st arg */
    iNewLen = vsnprintf(NULL,
                        0,
                        chFormat,
                        argList);
    // now we know how large the appended string will be, allocate memory for it
    tmp_cstr = malloc(iNewLen + 1);
    // chFormat the string
    vsnprintf(tmp_cstr, iNewLen + 1, chFormat, arg_list_copy);

    va_end(argList);

    // reallocate destination data memory buffer and then concatenate strings
    chReallocatedStr = realloc((void *)(sDest->data), (size_t)(iOldLen + iNewLen + 1));
    if (chReallocatedStr != NULL) {
        sDest->data = chReallocatedStr;
        strcat(sDest->data, tmp_cstr);
    }
    else
        click_debug_print("%s ERROR: Failed to allocate memory for appended string!\n", __func__);

    free(tmp_cstr);
}

/*
 * Function:  click_string_trim_prefix
 * Info:      Remove prefix string from a ClickSmsString.
 *            If prefix string length is the same or longer than the ClickSmsString
 *            length, then the ClickSmsString's data field will be freed and said
 *            ClickSmsString will be destroyed.
 * Inputs:    sBuf   - ClickSmsString containing prefix to remove
 *            iLen   - length of prefix data to remove
 * Return:    void
 */
void click_string_trim_prefix(ClickSmsString *sBuf, unsigned int iLen)
{
    if (CLICK_STR_INVALID(sBuf) || iLen == 0) {
        click_debug_print("%s ERROR: Invalid parameter!\n", __func__);
        return;
    }

    int iNewLen = strlen(sBuf->data) - (int)iLen;
    char *chReallocatedStr = NULL;
    char *chCopiedStr      = NULL;

    if (iNewLen < 1)
        click_string_destroy(sBuf);
    else {
        if ((chReallocatedStr = malloc(iNewLen + 1)) != NULL) {
            if ((chCopiedStr = strncpy(chReallocatedStr, sBuf->data + iLen, iNewLen)) != NULL) {
                chReallocatedStr[iNewLen] = '\0'; // set null terminator
                free(sBuf->data);                 // free old string
                sBuf->data = chReallocatedStr;     // now use new buffer
            }
            else
                click_debug_print("%s ERROR: Failed to copy string for ClickSmsString!\n", __func__);
        }
        else
            click_debug_print("%s ERROR: Failed to allocate memory for ClickSmsString!\n", __func__);
    }
}

/*
 * Function:  click_string_find_cstr
 * Info:      Search for a substring within a ClickSmsString.
 *            The substring iSize should be less or equal to the
 *            sHaystack string, else the function will return -1.
 * Inputs:    sHaystack  - string that could contain the substring we are
 *                        looking for
 *            chNeedle    - substring to search for
 *            iStartPos - character position in ClickSmsString where to start
 *                        substring search. Unsigned to disallow negative inputs.
 * Return:    search position in sHaystack, else -1 if substring not bFound.
 */
int click_string_find_cstr(const ClickSmsString *sHaystack, char *chNeedle, unsigned int iStartPos)
{
    int iNeedleLen   = strlen(chNeedle);
    int iHaystackLen = strlen(sHaystack->data);

    if (CLICK_STR_INVALID(sHaystack) || chNeedle == NULL || iHaystackLen < iNeedleLen || (int)iStartPos > iHaystackLen) {
        click_debug_print("%s ERROR: Invalid parameter!\n", __func__);
        return -1;
    }

    char *pHaystack = sHaystack->data;
    int iSearchLen   = (int)iStartPos;
    int bFound        = 0;
    pHaystack      += (int)iStartPos;

    while ((iSearchLen++ + iNeedleLen) < iHaystackLen) {
        // traverse until chNeedle bFound or we reach end of sHaystack
        if ((bFound = strncmp(pHaystack, chNeedle, iNeedleLen)) == 0)
            break;
        else
            pHaystack++;
    }

    return (bFound == 0 ? iSearchLen : -1);
}

/*
 * Function:  click_string_create
 * Info:      Creates a new ClickSmsString.
 * Inputs:    chStr - pointer to buffer that acts as sSource data for our ClickSmsString.
 * Return:    new ClickSmsString if successful, else NULL if failed to allocate memory for
 *            new ClickSmsString or if 'chStr' parameter is invalid.
 */
ClickSmsString *click_string_create(const char *chStr)
{
    int iLenBuffer = 0;

    if (chStr == NULL || ((iLenBuffer = strlen(chStr)) < 1))
        return NULL;

    return local_string_create(chStr, iLenBuffer);
}

/*
 * Function:  click_string_duplicate
 * Info:      Allocates memory for a new duplicated ClickSmsString.
 *            Note that the calling function must destroy the returned ClickSmsString.
 * Inputs:    sBuf - pointer to ClickSmsString that acts as sSource data for our new string.
 * Return:    copied ClickSmsString
 */
ClickSmsString *click_string_duplicate(const ClickSmsString *sBuf)
{
    int iLenBuffer = 0;

    if (CLICK_STR_INVALID(sBuf) || ((iLenBuffer = strlen(sBuf->data)) < 1))
        return NULL;

    return local_string_create(sBuf->data, iLenBuffer);
}

/*
 * Function:  click_string_retrieve_cstr
 * Info:      Allocates memory for a new duplicated character string.
 *            Note that the calling function must destroy the returned char string.
 * Inputs:    sBuf - pointer to ClickSmsString that acts as sSource data for our new string
 * Return:    new sOutput string if successful, else NULL
 */
char *click_string_retrieve_cstr(const ClickSmsString *sBuf)
{
    if (CLICK_STR_INVALID(sBuf)) {
        click_debug_print("%s ERROR: Invalid parameter!\n", __func__);
        return NULL;
    }

    char *sOutput = malloc(strlen(sBuf->data) + 1);

    if (sOutput != NULL)
        strcpy(sOutput, sBuf->data); // copy sSource buffer (including '\0') to destination

    return sOutput;
}

/*
 * Function:  click_string_destroy
 * Info:      Destroys a ClickSmsString.
 * Inputs:    sBuf - pointer to ClickSmsString to be destroyed.
 * Return:    void.
 */
void click_string_destroy(ClickSmsString *sBuf)
{
    if (sBuf == NULL)
        return;

    if (sBuf->data != NULL)
        free(sBuf->data);

    free(sBuf);
}

/*
 * Function:  click_string_url_encode
 * Info:      URL-encodes a ClickSmsString.
 * Inputs:    sBuf  - ClickSmsString to URL-encode
 * Return:    void
 */
void click_string_url_encode(ClickSmsString *sBuf)
{
    if (CLICK_STR_INVALID(sBuf)) {
        click_debug_print("%s ERROR: Invalid parameter!\n", __func__);
        return;
    }

    char *chEncodedStr = NULL; // pointer to temporary url-encoded buffer
    char *sReturn  = NULL; // pointer to sOutput string

    // allocate memory: worst case is that all characters are unsafe (then 3 chars required for every char, ie: ! becomes %21)
    if ((chEncodedStr = (char *)malloc((int)strlen(sBuf->data) * 3 + 1)) == NULL) {
        click_debug_print("%s ERROR: Failed to alloc encoded string!\n", __func__);
        return;
    }

    char *pBuf = sBuf->data;           // pointer to original string
    char *pEncodedStr = chEncodedStr;  // pointer to new encoded string
    int iCharCount = 0;
    unsigned int iDecimalVal = 0;
    char chBuf[3];

    // traverse string searching for characters to URL encode
    while(*pBuf != '\0') {
        if (URL_ENCODE_SAFE_CHAR(*pBuf)) {
            *pEncodedStr = *pBuf;    // safe characters remain as is
            iCharCount++;
        }
        else {
            /* http://www.w3.org/Addressing/URL/uri-spec.html#z5 states:
             * "Within the query string, the plus sign is reserved as shorthand notation for a space."
             * note this also saves space in the SMS message instead of using 3 characters "%20" per space
             * we instead utilize just one "+"
             */
            if (*pBuf == ' ') {
                *pEncodedStr = '+'; // use + instead of %20
                iCharCount++;
            }
            else {
                // add URL-encoded 3-character replacement, i.e. +  becomes %2B
                *pEncodedStr = '%';

                iDecimalVal = ((*pBuf >> 4) & 0xf);
                sprintf(chBuf, "%x", iDecimalVal); // convert upper nibble to lowercase hex char
                pEncodedStr++;
                *pEncodedStr = chBuf[0];

                iDecimalVal = ((*pBuf) & 0xf);
                sprintf(chBuf, "%x", iDecimalVal); // convert lower nibble to lowercase hex char
                pEncodedStr++;
                *pEncodedStr = chBuf[0];

                iCharCount+=3;
            }
        }

        pBuf++;
        pEncodedStr++;
    }

    if ((sReturn = malloc(iCharCount + 1)) != NULL) {
        memcpy(sReturn, chEncodedStr, iCharCount);
        sReturn[iCharCount] = '\0';
    }
    else
        click_debug_print("%s ERROR: Failed to alloc return string for ClickSmsString data!\n", __func__);

    free(chEncodedStr);

    if (sReturn != NULL) {
        free(sBuf->data);
        sBuf->data = sReturn;
    }
}
