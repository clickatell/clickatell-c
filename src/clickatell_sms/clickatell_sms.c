/*
 * clickatell_sms.c
 *
 *  Clickatell SMS Library
 *
 *  This library allows a developer to integrate their application with both the
 *  Clickatell REST API and Clickatell HTTP API. The Clickatell REST API does support XML format
 *  for requests and responses, but note that in this library we transmit REST requests in JSON
 *  format and receive corresponding responses in JSON format.
 *  The following public functions are provided for each API type:
 *   -  send MT message(s)
 *   -  get user’s credit balance
 *   -  get message status
 *   -  get message charge
 *   -  get coverage
 *   -  stop a message
 *
 *  The Clickatell SMS library integrates with libcurl (free client-side URL transfer library).
 *  Libcurl is cross-platform, and therefore if running a windows application,
 *  one would just need to download the relevant libcurl resource from
 *  http://curlHandle.haxx.se/download.html. If working with a Linux OS, the "curlHandle-devel" package
 *  should be installed prior to building this library with a Makefile.
 *
 *   Martin Beyers <martin.beyers@clickatell.com>
 */

#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include "curl/curl.h"

#include "clickatell_debug.h"
#include "clickatell_string.h"
#include "clickatell_sms.h"

/* ----------------------------------------------------------------------------- *
 * Types/Macros                                                                  *
 * ----------------------------------------------------------------------------- */

// HTTP API Username+Password container
typedef struct ClickLoginUserPass {
    ClickSmsString *sUsername;
    ClickSmsString *sPassword;
} ClickLoginUserPass;

// REST API_Key container
typedef struct ClickLoginApiKey {
    ClickSmsString *sKey;
} ClickLoginApiKey;

// Key/Value pair container
typedef struct ClickKeyVal {
    ClickSmsString *sKey; // sKey string
    ClickSmsString *sVal; // value string
} ClickKeyVal;

// container to hold all Key/Value pairs for an API call
typedef struct ClickArrayKeyVal {
    int iNum;                   // count of Key/Value pairs
    ClickKeyVal **aKeyValues;   // array of pointers to Key/Value structures
} ClickArrayKeyVal;

// internal structure (hidden from public access) for processing Clickatell API calls
struct ClickSmsHandle {
    // API type
    eClickApi eApiType;

    // input configs
    ClickSmsString *sApiId; // user's Clickatell API ID when user creates a new API in Clickatell central.

    // union for storing either apikey or Username+Password as they are mutually exclusive
    union uLoginDetails {
        ClickLoginUserPass userpass;
        ClickLoginApiKey apikey;
    } uLoginDetails;

    // output data
    ClickSmsString *sResponse;

    // cURL-request related fields
    struct curl_slist *curlHeaders; // cURL header data
    long     curlHttpStatus;        // HTTP status code
    CURL    *curlHandle;            // libcurl handle
    CURLcode curlCode;              // return code from recent curlHandle request
};

typedef enum eClickCurlRequestType{
    CLICK_CURL_GET,   // REST or HTTP
    CLICK_CURL_POST,  // REST or HTTP
    CLICK_CURL_DELETE // REST API only
} eClickCurlRequestType;

// default cURL request timeout values
#define CLICK_SMS_DEFAULT_APICALL_TIMEOUT          5  // max time allowed for API call to Clickatell
#define CLICK_SMS_DEFAULT_APICALL_CONNECT_TIMEOUT  5  // max connection time allowed for API call to Clickatell

// macro to validate API type
#define VALIDATE_API_TYPE(api)           ((api) >= CLICK_API_HTTP &&  (api) < CLICK_API_COUNT)
// macro to validate user-provided input parameters
#define VALIDATE_AUTH_PARAMS(api, user, pass, apikey) (((api) == CLICK_API_HTTP && !CLICK_STR_INVALID((user)) && !CLICK_STR_INVALID((pass))) || \
                                                       ((api) == CLICK_API_REST && !CLICK_STR_INVALID((apikey))))
// macro to validate a ClickMsisdn container
#define CLICK_MSISDN_INVALID(cm)         ((cm) == NULL || ((cm)->iNum) < 1 || (cm)->aDests == NULL)
// macro to validate a ClickArrayKeyVal container
#define CLICK_KEYVAL_ARRAY_INVALID(ckva) ((ckva) != NULL && (((ckva)->iNum) < 1 || (ckva)->aKeyValues == NULL))

// Clickatell Messaging base URL
static char chLocalBaseUrl[] = "https://api.clickatell.com/";

/* ----------------------------------------------------------------------------- *
 * Forward declarations                                                          *
 * ----------------------------------------------------------------------------- */

static ClickArrayKeyVal *local_click_keyval_array_create(int iNumKeyPairs);
static void local_sms_reset(ClickSmsHandle *oClickSms);
static size_t local_sms_curl_response_cb(void *buffer, size_t iSize, size_t iMemLen, void *sResponse);
static void local_sms_curl_config(ClickSmsHandle *oClickSms, long iTimeout, long iConnectTimeout);
static void local_sms_curl_execute(ClickSmsHandle *oClickSms,
                                   ClickSmsString *sFullUrl,
                                   eClickCurlRequestType eReqType,
                                   ClickSmsString *sPostData);
static ClickSmsString *local_api_command_execute(ClickSmsHandle *oClickSms,
                                                 const ClickSmsString *sPath,
                                                 eClickCurlRequestType eRequestType,
                                                 const ClickArrayKeyVal *oKeyVals,
                                                 const ClickMsisdn *aMsisdns);

/* ----------------------------------------------------------------------------- *
 * Local function definitions                                                    *
 * ----------------------------------------------------------------------------- */

/*
 * Function:  local_sms_reset
 * Info:      Resets output fields of ClickSmsHandle structure.
 * Inputs:    ClickSmsHandle
 * Outputs:   None
 * Return:    void
 */
static void local_sms_reset(ClickSmsHandle *oClickSms)
{
    if (oClickSms != NULL) {
        if (oClickSms->sResponse != NULL) {
            click_string_destroy(oClickSms->sResponse); // deallocate memory
            oClickSms->sResponse = NULL;                // avoid dangling pointers
        }
    }
}

/*
 * Function:  local_click_keyval_array_create
 * Info:      Allocate memory for a ClickArrayKeyVal structure.
 *            The ClickArrayKeyVal structure stores an array of pointers to ClickKeyVal
 *            Key/Value structures. Each ClickKeyVal pointer will point to a newly allocated
 *            ClickKeyVal memory structure.
 * Inputs:    iNumKeyPairs - count of Key/Value pairs excluding destination address ("to") parameter
 * Return:    pointer to a new ClickArrayKeyVal structure
 */
static ClickArrayKeyVal *local_click_keyval_array_create(int iNumKeyPairs)
{
    ClickArrayKeyVal *oKeyVals = NULL;

    if (iNumKeyPairs < 1 || ((oKeyVals = (ClickArrayKeyVal *)calloc(1, sizeof(ClickArrayKeyVal))) == NULL))
        return NULL;

    if ((oKeyVals->aKeyValues = calloc(iNumKeyPairs, sizeof(ClickKeyVal *))) == NULL) {
        free(oKeyVals);
        return NULL;
    }

    int i = 0;
    for (i = 0; i < iNumKeyPairs; i++) {
        if ((oKeyVals->aKeyValues[i] = (ClickKeyVal *)calloc(1, sizeof(ClickKeyVal))) == NULL) {
            free(oKeyVals);
            return NULL;
        }
    }

    oKeyVals->iNum = iNumKeyPairs;

    return oKeyVals;
}

/*
 * Function:  local_click_keyval_array_destroy
 * Info:      Destroy a ClickArrayKeyVal structure. The function will:
 *            - destroy each ClickKeyVal key string, then
 *            - destroy each ClickKeyVal value string, then
 *            - destroy each ClickKeyVal key value structure, then
 *            - destroy the ClickKeyVal pointers, and lastly
 *            - destroy the single ClickArrayKeyVal
 * Inputs:    oKeyVals - structure to be destroyed
 * Return:    void
 */
static void local_click_keyval_array_destroy(ClickArrayKeyVal *oKeyVals)
{
    if (oKeyVals == NULL)
        return;

    if (oKeyVals->aKeyValues != NULL) {
        int i = 0;
        for (i = 0; i < oKeyVals->iNum; i++) {
            click_string_destroy(oKeyVals->aKeyValues[i]->sKey);
            click_string_destroy(oKeyVals->aKeyValues[i]->sVal);
            free(oKeyVals->aKeyValues[i]);
        }

        free(oKeyVals->aKeyValues);
    }

    free(oKeyVals);
}

/*
 * Function:  local_sms_curl_response_cb
 * Info:      Callback which is set in function local_sms_curl_config() when initializing
 *            the cURL CURLOPT_WRITEFUNCTION option.
 *            The 'sResponse' parameter passed back here was set in function
 *            local_sms_curl_execute() when configuring the cURL CURLOPT_WRITEDATA option.
 *            This callback function reads a curlHandle request's response data.
 *            In here we allocate the response data to the corresponding
 *            ClickSmsHandle's response field.
 * Return:    Size of response data buffer
 */
static size_t local_sms_curl_response_cb(void *buffer, size_t iSize, size_t iMemLen, void *sResponse)
{
    int iTotalSize = iMemLen * iSize;

    if (iTotalSize > 0) {
        ClickSmsHandle *oClickSms = (ClickSmsHandle *)sResponse;
        char *chTempData = NULL;

        if (oClickSms->sResponse != NULL) { // should not reach this logic since local_sms_reset() called before each eRequestType
            click_string_destroy(oClickSms->sResponse);
            oClickSms->sResponse = NULL;
            click_debug_print("%s WARNING: Had to clear response data which should be NULL\n", __func__);
        }

        if (oClickSms == NULL) // this handle should never be NULL, but cater for the scenario in any case
            click_debug_print("%s ERROR: cURL reponse data invalid!\n", __func__);
        else if ((chTempData = calloc(iTotalSize + 1, sizeof(char))) != NULL) {
            memcpy(chTempData, buffer, iTotalSize);
            chTempData[iTotalSize] = '\0';

            oClickSms->sResponse = click_string_create(chTempData);

            free(chTempData);
        }
        else
            click_debug_print("%s ERROR: Failed to allocate memory for response!\n", __func__);
    }

    return (iTotalSize);
}

/*
 * Function:  local_sms_curl_config
 * Info:      Initializes CURL handle using libcurl library functions.
 *            This function applies standard cURL configs. For REST/HTTP-specific
 *            cURL configuration logic, please see function local_sms_curl_execute().
 * Inputs:    oClickSms       - Handle required when calling clickatell_sms_### functions
 *            iTimeout        - Maximum duration for cURL request to Clickatell server
 *            iConnectTimeout - Maximum timeout for cURL connection to Clickatell server
 * Return:    void
 */
static void local_sms_curl_config(ClickSmsHandle *oClickSms, long iTimeout, long iConnectTimeout)
{
    if (oClickSms == NULL)
        return;

    // set this to 1 for detailed curlHandle debug
    curl_easy_setopt(oClickSms->curlHandle, CURLOPT_VERBOSE, 0);

    // curlHandle version set
    curl_easy_setopt(oClickSms->curlHandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    // here set maximum timeouts for libcurl transfer eRequestType
    curl_easy_setopt(oClickSms->curlHandle,
                     CURLOPT_TIMEOUT,
                     (iTimeout <= 0 ? CLICK_SMS_DEFAULT_APICALL_TIMEOUT : iTimeout));
    curl_easy_setopt(oClickSms->curlHandle,
                     CURLOPT_CONNECTTIMEOUT,
                     (iConnectTimeout <= 0 ? CLICK_SMS_DEFAULT_APICALL_CONNECT_TIMEOUT : iConnectTimeout));

    // Clickatell will write the response data to this write function callback (instead of to stdout)
    curl_easy_setopt(oClickSms->curlHandle, CURLOPT_WRITEFUNCTION, local_sms_curl_response_cb);
}

/*
 * Function:  local_sms_curl_execute
 * Info:      Executes a cURL request using libcurl.
 *            The result of the cURL eRequestType (cURL return code) will be set in the
 *            ClickSmsHandle's 'curl_result' field.
 *            The cURL eRequestType's response data will be set in the 'sResponse'
 *            field of the ClickSmsHandle.
 * Input:     oClickSms - Handle required when calling clickatell_sms_### functions
 *            eReqType  - Type of curl handle request
 *            sFullUrl  - Full URL for API call (excluding parameters)
 *            sPostData - cURL 'POST request' data
 * Output:    oClickSms - ClickSmsHandle 'sResponse' field will contain the API call's
 *                        response received from Clickatell.
 *            oClickSms - ClickSmsHandle 'curlCode' field will contain the cURL
 *                        response code. This will be set as the return value for the
 *                        cURL curl_easy_perform function call.
 *            oClickSms - ClickSmsHandle 'curlHttpStatus' field will contain the HTTP
 *                        status code returned.
 * Return:    void
 */
static void local_sms_curl_execute(ClickSmsHandle *oClickSms, ClickSmsString *sFullUrl,
                                   eClickCurlRequestType eReqType, ClickSmsString *sPostData)
{
    if (oClickSms == NULL || CLICK_STR_INVALID(sFullUrl)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return;
    }

    // add curlHeaders if applicable
    if (oClickSms->curlHeaders != NULL)
        curl_easy_setopt(oClickSms->curlHandle, CURLOPT_HTTPHEADER, oClickSms->curlHeaders);
    else // remove curlHeaders
        curl_easy_setopt(oClickSms->curlHandle, CURLOPT_HTTPHEADER, NULL);

    // set full URL for curl handle request and data to pass back to response callback
    curl_easy_setopt(oClickSms->curlHandle, CURLOPT_URL, sFullUrl->data);
    curl_easy_setopt(oClickSms->curlHandle, CURLOPT_WRITEDATA, oClickSms);

    switch (eReqType) {
        case CLICK_CURL_POST:
            // set cURL 'POST request' data if requested and if the post data exists
            if (!CLICK_STR_INVALID(sPostData) && strlen(sPostData->data) > 0) {
                curl_easy_setopt(oClickSms->curlHandle, CURLOPT_POST, 1);
                curl_easy_setopt(oClickSms->curlHandle, CURLOPT_POSTFIELDS, sPostData->data);
                curl_easy_setopt(oClickSms->curlHandle, CURLOPT_POSTFIELDSIZE, strlen(sPostData->data));

                click_debug_print("Curl post data:\n%s\n", sPostData->data);
            }
            break;

        case CLICK_CURL_DELETE:
            curl_easy_setopt(oClickSms->curlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;

        case CLICK_CURL_GET:
        default:
            curl_easy_setopt(oClickSms->curlHandle, CURLOPT_HTTPGET, 1);
            break;
    }

    // execute curl handle request
    oClickSms->curlCode = curl_easy_perform(oClickSms->curlHandle);

    // obtain response data
    if (oClickSms->curlCode == CURLE_OK)
        oClickSms->curlCode = curl_easy_getinfo(oClickSms->curlHandle, CURLINFO_RESPONSE_CODE, &(oClickSms->curlHttpStatus));

    // output debug information
    click_debug_print("Curl %s-Request URL:\n%s\n", (eReqType == CLICK_CURL_POST ? "POST" : (eReqType == CLICK_CURL_GET ? "GET" : "DELETE")),
                                                    (sFullUrl == NULL ? "" : sFullUrl->data));
    click_debug_print("Curl HTTP sResponse code:\n%d\n", oClickSms->curlHttpStatus);
    click_debug_print("Curl sResponse:\n%s\n", (oClickSms->sResponse == NULL ? "" : oClickSms->sResponse->data));
}

/* ----------------------------------------------------------------------------- *
 * Public function definitions                                                   *
 * ----------------------------------------------------------------------------- */

/*
 * Function:  clickatell_sms_init
 * Info:      Initializes Clickatell SMS library.
 *            This function MUST be called before calling any other functions in
 *            this library.
 * Inputs:    none
 * Return:    none
 */
void clickatell_sms_init(void)
{
    // initialize debug module
    click_debug_init(CLICK_DEBUG_ON);

    // initialize cURL
    curl_global_init(CURL_GLOBAL_ALL);
}

/*
 * Function:  clickatell_sms_shutdown
 * Info:      Shutdown the Clickatell SMS library.
 *            This function must be called when the Clickatell SMS library
 *            must be shutdown.
 * Inputs:    none
 * Return:    none
 */
void clickatell_sms_shutdown(void)
{
    // shutdown cURL
    curl_global_cleanup();
}

/*
 * Function:  clickatell_sms_handle_init
 * Info:      Initializes Clickatell SMS API handle.
 * Inputs:    sUsername       - HTTP API Username from Clickatell account
 *            sPassword       - HTTP API Password from Clickatell account
 *            sApiKey         - REST API Key from Clickatell account
 *            sApiId          - HTTP or REST API number from Clickatell account
 *            iTimeout        - Maximum timeout for API call to take
 *            iConnectTimeout - Maximum timeout for API call connection to take
 * Return:    ClickSmsHandle handle required when calling any clickatell_sms_###
 *            functions, else NULL if failed to create ClickSmsHandle.
 */
ClickSmsHandle *clickatell_sms_handle_init(eClickApi eApiType,
                                           const ClickSmsString *sUsername,
                                           const ClickSmsString *sPassword,
                                           const ClickSmsString *sApiKey,
                                           const ClickSmsString *sApiId,
                                           long iTimeout, long iConnectTimeout)
{
    if (!VALIDATE_API_TYPE(eApiType) || CLICK_STR_INVALID(sApiId) || !VALIDATE_AUTH_PARAMS(eApiType, sUsername, sPassword, sApiKey)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    ClickSmsHandle *oClickSms = (ClickSmsHandle *)calloc(1, sizeof(ClickSmsHandle));
    oClickSms->eApiType = eApiType;

    if ((oClickSms->curlHandle = curl_easy_init()) == NULL) {
        clickatell_sms_handle_shutdown(oClickSms);
        oClickSms = NULL;
    }
    else {
        local_sms_curl_config(oClickSms, iTimeout, iConnectTimeout);

        // REST requires API Key only and other APIs (ie HTTP) require username+password for authentication
        if (eApiType == CLICK_API_REST) {
            oClickSms->uLoginDetails.apikey.sKey = click_string_duplicate(sApiKey);

            // configure default curlHeaders - always ensure first slist append call has NULL curl headers argument
            oClickSms->curlHeaders = curl_slist_append(NULL, "X-Version: 1");
            oClickSms->curlHeaders = curl_slist_append(oClickSms->curlHeaders, "Content-Type: application/json");
            oClickSms->curlHeaders = curl_slist_append(oClickSms->curlHeaders, "Accept: application/json");

            // the REST API Key will be used as the authorization token
            ClickSmsString *sBuf = click_string_create("Authorization: Bearer ");
            click_string_append(sBuf, NULL, oClickSms->uLoginDetails.apikey.sKey->data);
            oClickSms->curlHeaders = curl_slist_append(oClickSms->curlHeaders, sBuf->data);
            click_string_destroy(sBuf);

            // set default curl headers - can replace them if necessary
            curl_easy_setopt(oClickSms->curlHandle, CURLOPT_HTTPHEADER, oClickSms->curlHeaders);
        }
        else {
            oClickSms->uLoginDetails.userpass.sUsername = click_string_duplicate(sUsername);
            oClickSms->uLoginDetails.userpass.sPassword = click_string_duplicate(sPassword);

            // configure default curlHeaders - always ensure first slist append call has NULL curl headers argument
            oClickSms->curlHeaders = curl_slist_append(NULL, "Connection:keep-alive");
            oClickSms->curlHeaders = curl_slist_append(oClickSms->curlHeaders, "Cache-Control:max-age=0");
            oClickSms->curlHeaders = curl_slist_append(oClickSms->curlHeaders, "Origin:null");

            // set default curlHeaders - can replace them if necessary
            curl_easy_setopt(oClickSms->curlHandle, CURLOPT_HTTPHEADER, oClickSms->curlHeaders);
        }

        oClickSms->sApiId = click_string_duplicate(sApiId);
    }

    return oClickSms;
}

/*
 * Function:  local_api_command_execute
 * Info:      Common function to execute a Clickatell API call.
 *            This function assumes ALL input parameters are valid.
 *            If providing HTTP Key/Values, then the sizes of both the
 *            'param_keys' and the 'param_vals' should match.
 * Inputs:    oClickSms        - ClickSmsHandle API handle
 *            sPath            - local path to resource which will be appended to base URL
 *            eRequestType     - Type of cURL request type (ie POST, GET, DELETE)
 *            oKeyVals         - array of Key/Value pairs. Set this to NULL if no Key/Value pairs
 *                               will be used.
 *            aMsisdns         - Array of destination addresses (for send message call only)
 *                               This function expects this parameter to be validated by the calling
 *                               function.
 *                               If not performing a send message call, then this parameter
 *                               should be set to NULL.
 * Return:    ClickSmsString containing the curlHandle request's response from Clickatell.
 *            The calling function must destroy said ClickSmsString.
 */
static ClickSmsString *local_api_command_execute(ClickSmsHandle *oClickSms,
                                                 const ClickSmsString *sPath,
                                                 eClickCurlRequestType eRequestType,
                                                 const ClickArrayKeyVal *oKeyVals,
                                                 const ClickMsisdn *aMsisdns)
{
    if (oClickSms == NULL || CLICK_STR_INVALID(sPath) || CLICK_KEYVAL_ARRAY_INVALID(oKeyVals)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    int i = 0;
    ClickSmsString *sResponse = NULL, *sPostData = NULL, *sUrl = NULL, *sApiParams = NULL;

    // format URL Key/Value parameters
    if (oKeyVals != NULL) {
       if (oClickSms->eApiType == CLICK_API_HTTP) {
            sApiParams = click_string_create("?");

            // append all non-"to" parameters first
            for (i = 0; i < oKeyVals->iNum; i++) {
                click_string_append_formatted_cstr(sApiParams, "%s%s=%s", (i == 0 ? "" : "&"),
                                                (char *)(oKeyVals->aKeyValues[i]->sKey->data),
                                                (char *)(oKeyVals->aKeyValues[i]->sVal->data));
            }

            // For send message API calls only: append "to" parameter, example:  &to=2799900001,2799900002
            if (aMsisdns != NULL) {
                click_string_append_formatted_cstr(sApiParams, "&to=");

                for (i = 0; i < aMsisdns->iNum; i++) {
                    click_string_append_formatted_cstr(sApiParams, "%s%s", (i == 0 ? "" : ","), aMsisdns->aDests[i]->data);
                }
            }
        }
        else { // REST
            sApiParams = click_string_create("{"); // the JSON data is enclosed in opening/closing braces

            // append all non-"to" parameters first, example:  {"sText":"Test Message","callback":"7"}
            for (i = 0; i < oKeyVals->iNum; i++)
                click_string_append_formatted_cstr(sApiParams, "%s\"%s\":\"%s\"", (i == 0 ? "" : ","),
                        (char *)(oKeyVals->aKeyValues[i]->sKey->data),
                        (char *)(oKeyVals->aKeyValues[i]->sVal->data));

            // For send message API calls only: append "to" parameter, example:  "to":["2799900001","2799900002"]}'
            if (aMsisdns != NULL) {
                click_string_append_formatted_cstr(sApiParams, ",\"to\":[");

                for (i = 0; i < aMsisdns->iNum; i++)
                    click_string_append_formatted_cstr(sApiParams, "%s\"%s\"", (i == 0 ? "" : ","), aMsisdns->aDests[i]->data);

                click_string_append_formatted_cstr(sApiParams, "]");
            }
            click_string_append_formatted_cstr(sApiParams, "}"); // the JSON data is enclosed in opening/closing braces
        }
    }

    // format full URL by combining 1. Clickatell base URL 2. API call script or resource path and 3. Key/Value parameters
    sUrl = click_string_create(chLocalBaseUrl);
    if (sUrl == NULL) {
        click_debug_print("%s ERROR: failed to allocate memory for URL!\n", __func__);
        goto exit;
    }

    click_string_append(sUrl, sPath, NULL); // append API call script (HTTP) / resource path (REST)

    // cURL eRequestType-type specific logic
    if (oKeyVals != NULL) {
        switch (eRequestType) {
            case CLICK_CURL_POST:
                sPostData = click_string_duplicate(sApiParams);
                break;

            case CLICK_CURL_DELETE:
                click_string_append(sUrl, sApiParams, NULL);
                break;

            case CLICK_CURL_GET:
            default:
                click_string_append(sUrl, sApiParams, NULL);
                break;
        }
    }

    // execute curl handle request
    local_sms_curl_execute(oClickSms, sUrl, eRequestType, sPostData);

    // set response string (memory must be deallocated by calling function)
    sResponse = click_string_duplicate(oClickSms->sResponse);

exit:
    click_string_destroy(sUrl);
    click_string_destroy(sPostData);
    click_string_destroy(sApiParams);

    return sResponse;
}

/*
 * Function:  clickatell_sms_message_send
 * Info:      Sends SMSes.
 *            This function will set the URL / post data params as follows:
 *               For REST, we need at least 2 Key/Value pairs -> "text" "to"
 *               For HTTP, we need at least 5 Key/Value pairs -> "user" "password" "api_id" "text" "to"
 *            The calling function must free memory allocated to the returned string.
 * Inputs:    oClickSms  - Handle returned from clickatell_sms_init() function call
 *            sText      - Message Text (Latin1 format supported in this library)
 *            aMsisdns   - Array of destination mobile numbers
 * Return:    API Message ID or error code if eRequestType unsuccessful or NULL if invalid parameter
 */
ClickSmsString *clickatell_sms_message_send(ClickSmsHandle *oClickSms, const ClickSmsString *sText, ClickMsisdn *aMsisdns)
{
    if (oClickSms == NULL || CLICK_STR_INVALID(sText) || CLICK_MSISDN_INVALID(aMsisdns)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    int i = 0;
    ClickSmsString *sResponse  = NULL;
    ClickSmsString *sPath      = NULL; // API call script file / resource path designator
    ClickArrayKeyVal *oKeyVals = NULL; // array of Key/Value structures, excluding "to" field
    eClickCurlRequestType eReqType = (oClickSms->eApiType == CLICK_API_HTTP ? CLICK_CURL_GET : CLICK_CURL_POST);

    local_sms_reset(oClickSms); // clear any old memory allocations

    if (oClickSms->eApiType == CLICK_API_HTTP) {
        sPath = click_string_create("http/sendmsg.php");

        // set URL Key/Value pairs
        if ((oKeyVals = local_click_keyval_array_create(4)) == NULL)
            return NULL;
        oKeyVals->aKeyValues[0]->sKey = click_string_create("user");
        oKeyVals->aKeyValues[0]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sUsername);
        oKeyVals->aKeyValues[1]->sKey = click_string_create("password");
        oKeyVals->aKeyValues[1]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sPassword);
        oKeyVals->aKeyValues[2]->sKey = click_string_create("api_id");
        oKeyVals->aKeyValues[2]->sVal = click_string_duplicate(oClickSms->sApiId);
        oKeyVals->aKeyValues[3]->sKey = click_string_create("text");
        oKeyVals->aKeyValues[3]->sVal = click_string_duplicate(sText);

        // URL-encode the URL values
        for (i = 0; i < oKeyVals->iNum; i++)
            click_string_url_encode(oKeyVals->aKeyValues[i]->sVal);
    }
    else { // REST
        sPath = click_string_create("rest/message");

        // set post data Key/Value pairs
        if ((oKeyVals = local_click_keyval_array_create(1)) == NULL)
            return NULL;
        oKeyVals->aKeyValues[0]->sKey = click_string_create("text");
        oKeyVals->aKeyValues[0]->sVal = click_string_duplicate(sText);
    }

    // performs formatting of API call and then executes the request
    sResponse = local_api_command_execute(oClickSms, sPath, eReqType, oKeyVals, aMsisdns);

    // free allocated memory
    local_click_keyval_array_destroy(oKeyVals);
    click_string_destroy(sPath);

    return sResponse;
}

/*
 * Function:  clickatell_sms_status_get
 * Info:      Obtain current status of an SMS message.
 *            Authentication: This function uses Username+Password to authenticate for the
 *                            HTTP API, however it is also possible in a session to use a session ID to
 *                            authenticate with the HTTP API. See the Clickatell API docs at
 *                            www.clickatell.com for more details.
 *            URL Encoding: For the HTTP API, The URL parameter values are URL-encoded in
 *                          this function.
 *            The calling function must free memory allocated to the returned string.
 * Inputs:    oClickSms      - Handle returned from clickatell_sms_init() function call
 *            API Message ID - SMS ID assigned by Clickatell
 * Return:    Status of API message
 */
ClickSmsString *clickatell_sms_status_get(ClickSmsHandle *oClickSms, const ClickSmsString *sMsgId)
{
    if (oClickSms == NULL || CLICK_STR_INVALID(sMsgId)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    int i = 0;
    ClickSmsString *sResponse  = NULL;
    ClickSmsString *sPath      = NULL; // api call path designator
    ClickArrayKeyVal *oKeyVals = NULL; // array of Key/Value structures

    local_sms_reset(oClickSms); // clear any old memory allocations

    if (oClickSms->eApiType == CLICK_API_HTTP) {
        sPath = click_string_create("http/querymsg.php");

        // set URL Key/Value pairs
        if ((oKeyVals = local_click_keyval_array_create(4)) == NULL)
            return NULL;
        oKeyVals->aKeyValues[0]->sKey = click_string_create("user");
        oKeyVals->aKeyValues[0]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sUsername);
        oKeyVals->aKeyValues[1]->sKey = click_string_create("password");
        oKeyVals->aKeyValues[1]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sPassword);
        oKeyVals->aKeyValues[2]->sKey = click_string_create("api_id");
        oKeyVals->aKeyValues[2]->sVal = click_string_duplicate(oClickSms->sApiId);
        oKeyVals->aKeyValues[3]->sKey = click_string_create("apimsgid");
        oKeyVals->aKeyValues[3]->sVal = click_string_duplicate(sMsgId);

        // URL-encode URL values
        for (i = 0; i < oKeyVals->iNum; i++)
            click_string_url_encode(oKeyVals->aKeyValues[i]->sVal);
    }
    else { // REST
        // example URL:  http://api.clickatell.com/rest/message/47584bae0165fbec57b18bf47895fece
        sPath = click_string_create("rest/message/");
        click_string_append(sPath, sMsgId, NULL);
    }

    // performs formatting of API call and then executes the request
    sResponse = local_api_command_execute(oClickSms, sPath, CLICK_CURL_GET, oKeyVals, NULL);

    // free allocated memory
    local_click_keyval_array_destroy(oKeyVals);
    click_string_destroy(sPath);

    return sResponse;
}

/*
 * Function:  clickatell_sms_balance_get
 * Info:      Obtain user's credit balance.
 *            Authentication: This function uses Username+Password to authenticate for the
 *                            HTTP API, however it is also possible in a session to use a session ID to
 *                            authenticate with the HTTP API. See the Clickatell API docs at
 *                            www.clickatell.com for more details.
 *            URL Encoding: For the HTTP API, The URL parameter values are URL-encoded in
 *                          this function.
 *            The calling function must free memory allocated to the returned string.
 * Inputs:    oClickSms      - Handle returned from clickatell_sms_init() function call
 * Return:    User's current balance.
 */
ClickSmsString *clickatell_sms_balance_get(ClickSmsHandle *oClickSms)
{
    if (oClickSms == NULL) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    int i = 0;
    ClickSmsString *sResponse  = NULL;
    ClickSmsString *sPath      = NULL; // api call path designator
    ClickArrayKeyVal *oKeyVals = NULL; // array of Key/Value structures

    local_sms_reset(oClickSms); // clear any old memory allocations

    if (oClickSms->eApiType == CLICK_API_HTTP) {
        sPath = click_string_create("http/getbalance.php");

        // set URL Key/Value pairs
        if ((oKeyVals = local_click_keyval_array_create(3)) == NULL)
            return NULL;
        oKeyVals->aKeyValues[0]->sKey = click_string_create("user");
        oKeyVals->aKeyValues[0]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sUsername);
        oKeyVals->aKeyValues[1]->sKey = click_string_create("password");
        oKeyVals->aKeyValues[1]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sPassword);
        oKeyVals->aKeyValues[2]->sKey = click_string_create("api_id");
        oKeyVals->aKeyValues[2]->sVal = click_string_duplicate(oClickSms->sApiId);

        // URL-encode URL values
        for (i = 0; i < oKeyVals->iNum; i++)
            click_string_url_encode(oKeyVals->aKeyValues[i]->sVal);
    }
    else { // REST
        // example URL:  https://api.clickatell.com/rest/account/balance
        sPath = click_string_create("rest/account/balance");
    }

    // performs formatting of API call and then executes the request
    sResponse = local_api_command_execute(oClickSms, sPath, CLICK_CURL_GET, oKeyVals, NULL);

    // free allocated memory
    local_click_keyval_array_destroy(oKeyVals);
    click_string_destroy(sPath);

    return sResponse;
}

/*
 * Function:  clickatell_sms_charge_get
 * Info:      Obtain charge of an SMS message.
 *            Authentication: This function uses Username+Password to authenticate for the
 *                            HTTP API, however it is also possible in a session to use a session ID to
 *                            authenticate with the HTTP API. See the Clickatell API docs at
 *                            www.clickatell.com for more details.
 *            URL Encoding: For the HTTP API, The URL parameter values are URL-encoded in
 *                          this function.
 *            The calling function must free memory allocated to the returned string.
 * Inputs:    oClickSms      - Handle returned from clickatell_sms_init() function call
 *            API Message ID - SMS ID assigned by Clickatell
 * Return:    Charge of SMS message.
 */
ClickSmsString *clickatell_sms_charge_get(ClickSmsHandle *oClickSms, const ClickSmsString *sMsgId)
{
    if (oClickSms == NULL || CLICK_STR_INVALID(sMsgId)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    int i = 0;
    ClickSmsString *sResponse  = NULL;
    ClickSmsString *sPath      = NULL; // api call path designator
    ClickArrayKeyVal *oKeyVals = NULL; // array of Key/Value structures

    local_sms_reset(oClickSms); // clear any old memory allocations

    if (oClickSms->eApiType == CLICK_API_HTTP) {
        sPath = click_string_create("http/getmsgcharge.php");

        // set URL Key/Value pairs
        if ((oKeyVals = local_click_keyval_array_create(4)) == NULL)
            return NULL;
        oKeyVals->aKeyValues[0]->sKey = click_string_create("user");
        oKeyVals->aKeyValues[0]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sUsername);
        oKeyVals->aKeyValues[1]->sKey = click_string_create("password");
        oKeyVals->aKeyValues[1]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sPassword);
        oKeyVals->aKeyValues[2]->sKey = click_string_create("api_id");
        oKeyVals->aKeyValues[2]->sVal = click_string_duplicate(oClickSms->sApiId);
        oKeyVals->aKeyValues[3]->sKey = click_string_create("apimsgid");
        oKeyVals->aKeyValues[3]->sVal = click_string_duplicate(sMsgId);

        // URL-encode URL values
        for (i = 0; i < oKeyVals->iNum; i++)
            click_string_url_encode(oKeyVals->aKeyValues[i]->sVal);
    }
    else { // REST
        // example URL:  http://api.clickatell.com/rest/message/47584bae0165fbec57b18bf47895fece
        sPath = click_string_create("rest/message/");
        click_string_append(sPath, sMsgId, NULL);
    }

    // performs formatting of API call and then executes the request
    sResponse = local_api_command_execute(oClickSms, sPath, CLICK_CURL_GET, oKeyVals, NULL);

    // free allocated memory
    local_click_keyval_array_destroy(oKeyVals);
    click_string_destroy(sPath);

    return sResponse;
}

/*
 * Function:  clickatell_sms_coverage_get
 * Info:      Enables users to check Clickatell coverage of a network/number, without sending
 *            a message to that number
 *            Authentication: This function uses Username+Password to authenticate for the
 *                            HTTP API, however it is also possible in a session to use a session ID to
 *                            authenticate with the HTTP API. See the Clickatell API docs at
 *                            www.clickatell.com for more details.
 *            URL Encoding: For the HTTP API, The URL parameter values are URL-encoded in
 *                          this function.
 *            The calling function must free memory allocated to the returned string.
 * Inputs:    oClickSms - Handle returned from clickatell_sms_init() function call
 *            msisdn    - single msisdn for which Clickatell will verify has supported coverage
 * Return:    Prefix is currently supported or prefix is not supported by Clickatell.
 */
ClickSmsString *clickatell_sms_coverage_get(ClickSmsHandle *oClickSms, const ClickSmsString *msisdn)
{
    if (oClickSms == NULL || CLICK_STR_INVALID(msisdn)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    int i = 0;
    ClickSmsString *sResponse  = NULL;
    ClickSmsString *sPath      = NULL; // api call path designator
    ClickArrayKeyVal *oKeyVals = NULL; // array of Key/Value structures

    local_sms_reset(oClickSms); // clear any old memory allocations

    if (oClickSms->eApiType == CLICK_API_HTTP) {
        sPath = click_string_create("utils/routecoverage.php");

        // set URL Key/Value pairs
        if ((oKeyVals = local_click_keyval_array_create(4)) == NULL)
            return NULL;
        oKeyVals->aKeyValues[0]->sKey = click_string_create("user");
        oKeyVals->aKeyValues[0]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sUsername);
        oKeyVals->aKeyValues[1]->sKey = click_string_create("password");
        oKeyVals->aKeyValues[1]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sPassword);
        oKeyVals->aKeyValues[2]->sKey = click_string_create("api_id");
        oKeyVals->aKeyValues[2]->sVal = click_string_duplicate(oClickSms->sApiId);
        oKeyVals->aKeyValues[3]->sKey = click_string_create("msisdn");
        oKeyVals->aKeyValues[3]->sVal = click_string_duplicate(msisdn);

        // URL-encode URL values
        for (i = 0; i < oKeyVals->iNum; i++)
            click_string_url_encode(oKeyVals->aKeyValues[i]->sVal);
    }
    else { // REST
        // example URL:  https://api.clickatell.com/rest/coverage/27999123456
        sPath = click_string_create("rest/coverage/");
        click_string_append(sPath, msisdn, NULL);
    }

    // performs formatting of API call and then executes the request
    sResponse = local_api_command_execute(oClickSms, sPath, CLICK_CURL_GET, oKeyVals, NULL);

    // free allocated memory
    local_click_keyval_array_destroy(oKeyVals);
    click_string_destroy(sPath);

    return sResponse;
}

/*
 * Function:  clickatell_sms_message_stop
 * Info:      Attempt to stop the delivery of an SMS message. This command can only stop messages
 *            which may be queued within the Clickatell system and not messages which have already
 *            been delivered to an SMSC.
 *            Authentication: This function uses Username+Password to authenticate for the
 *                            HTTP API, however it is also possible in a session to use a session ID to
 *                            authenticate with the HTTP API. See the Clickatell API docs at
 *                            www.clickatell.com for more details.
 *            URL Encoding: For the HTTP API, The URL parameter values are URL-encoded in
 *                          this function.
 *            The calling function must free memory allocated to the returned string.
 * Inputs:    oClickSms      - Handle returned from clickatell_sms_init() function call
 *            API Message ID - SMS ID assigned by Clickatell
 * Return:    ID with status or an error number with error description.
 */
ClickSmsString *clickatell_sms_message_stop(ClickSmsHandle *oClickSms, const ClickSmsString *sMsgId)
{
    if (oClickSms == NULL || CLICK_STR_INVALID(sMsgId)) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return NULL;
    }

    int i = 0;
    ClickSmsString *sResponse  = NULL;
    ClickSmsString *sPath      = NULL; // api call path designator
    ClickArrayKeyVal *oKeyVals = NULL; // array of Key/Value structures
    eClickCurlRequestType eReqType  = (oClickSms->eApiType == CLICK_API_HTTP ? CLICK_CURL_GET : CLICK_CURL_DELETE);

    local_sms_reset(oClickSms); // clear any old memory allocations

    if (oClickSms->eApiType == CLICK_API_HTTP) {
        sPath = click_string_create("http/delmsg.php");

        // set URL Key/Value pairs
        if ((oKeyVals = local_click_keyval_array_create(4)) == NULL)
            return NULL;
        oKeyVals->aKeyValues[0]->sKey = click_string_create("user");
        oKeyVals->aKeyValues[0]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sUsername);
        oKeyVals->aKeyValues[1]->sKey = click_string_create("password");
        oKeyVals->aKeyValues[1]->sVal = click_string_duplicate(oClickSms->uLoginDetails.userpass.sPassword);
        oKeyVals->aKeyValues[2]->sKey = click_string_create("api_id");
        oKeyVals->aKeyValues[2]->sVal = click_string_duplicate(oClickSms->sApiId);
        oKeyVals->aKeyValues[3]->sKey = click_string_create("apimsgid");
        oKeyVals->aKeyValues[3]->sVal = click_string_duplicate(sMsgId);

        // URL-encode URL values
        for (i = 0; i < oKeyVals->iNum; i++)
            click_string_url_encode(oKeyVals->aKeyValues[i]->sVal);
    }
    else { // REST
        // example URL:  https://api.clickatell.com/rest/message/47584bae0165fbec57b18bf47895fece
        sPath = click_string_create("rest/message/");
        click_string_append(sPath, sMsgId, NULL);
    }

    // performs formatting of API call and then executes the request
    sResponse = local_api_command_execute(oClickSms, sPath, eReqType, oKeyVals, NULL);

    // free allocated memory
    local_click_keyval_array_destroy(oKeyVals);
    click_string_destroy(sPath);

    return sResponse;
}

/*
 * Function:  clickatell_sms_handle_shutdown
 * Info:      Shutdown library descriptor handle, freeing up any memory used by the descriptor.
 * Inputs:    oClickSms - Handle returned from clickatell_sms_init() function call
 * Return:    void
 */
void clickatell_sms_handle_shutdown(ClickSmsHandle *oClickSms)
{
    if (oClickSms == NULL) {
        click_debug_print("%s ERROR: invalid parameter!\n", __func__);
        return;
    }

    local_sms_reset(oClickSms);

    click_string_destroy(oClickSms->sApiId);

    if (oClickSms->eApiType == CLICK_API_REST)
        click_string_destroy(oClickSms->uLoginDetails.apikey.sKey);
    else {
        click_string_destroy(oClickSms->uLoginDetails.userpass.sUsername);
        click_string_destroy(oClickSms->uLoginDetails.userpass.sPassword);
    }

    if (oClickSms->curlHeaders != NULL)
        curl_slist_free_all(oClickSms->curlHeaders);

    if (oClickSms->curlHandle != NULL)
        curl_easy_cleanup(oClickSms->curlHandle);

    free(oClickSms);
}
