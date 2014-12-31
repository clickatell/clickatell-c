/*
 * test_clickatell_sms.c
 *
 * Contains sample code demonstrating how to utilize Clickatell HTTP and REST APIs.
 * This file calls public functions from the Clickatell SMS library.
 * This file executes common API Calls for the following API types:
 * - HTTP API using Username+Password as authentication
 * - REST API using API Key as authentication
 *
 *  Martin Beyers <martin.beyers@clickatell.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clickatell_sms/clickatell_debug.h"
#include "clickatell_sms/clickatell_string.h"
#include "clickatell_sms/clickatell_sms.h"

/* ----------------------------------------------------------------------------- *
 * Input configuration values                                                    *
 * NOTE: Please modify these values and replace them with your own credentials.  *
 * ----------------------------------------------------------------------------- */

// insert your HTTP API credentials here
#define CFG_HTTP_USERNAME           "myusernamehere" // insert your Clickatell account username here
#define CFG_HTTP_PASSWORD           "mypasswordhere" // insert your Clickatell account password here
#define CFG_HTTP_APIID              "3518209"        // insert your Clickatell HTTP API ID here

// insert your REST API credentials here
#define CFG_REST_APIKEY             "uJqYpaWlUNPUhEDsuptRJCk5nGZD.Fwx8vHQOUjoTXTdFghXERUsZDvoK1SiF" // insert your Clickatell REST API Key here
#define CFG_REST_APIID              "2517153" // insert your Clickatell REST API ID here

// insert your destination addresses here
#define CFG_SAMPLE_MSISDN1          "2991000000" // insert your first desired destination mobile number here
#define CFG_SAMPLE_MSISDN2          "2991000001" // insert your second desired destination mobile number here
#define CFG_SAMPLE_MSISDN3          "2991000002" // insert your third desired destination mobile number here
#define CFG_SAMPLE_COVERAGE_MSISDN  "2991000000" // insert your coverage destination mobile number here

// insert your SMS message sText here
#define CFG_SAMPLE_MSG_TEXT         "This is example SMS message text; -> insert your own text here."

// timeout values - these can be modified or left as is
#define CFG_APICALL_TIMEOUT         5 // Config: Maximum time in seconds (long value) for API call to take
#define CFG_APICALL_CONNECT_TIMEOUT 2 // Config: maximum time in seconds (long value) that API call takes to connect to Clickatell server

/* ----------------------------------------------------------------------------- *
 * Fixed Macros/Types                                                            *
 * ----------------------------------------------------------------------------- */

// common print functions
#define PRINT_MAIN_TEST_SEPARATOR   { click_debug_print("\n===============================================================================================\n"); }
#define PRINT_SUB_TEST_SEPARATOR    { click_debug_print("\n\n"); }

/* ----------------------------------------------------------------------------- *
 * Forward declarations                                                          *
 * ----------------------------------------------------------------------------- */

static void run_common_tests(eClickApi eApiType);
static void run_common_api_calls(eClickApi eApiType, ClickSmsHandle *oClickSms);

/* ----------------------------------------------------------------------------- *
 * Local function definitions                                                    *
 * ----------------------------------------------------------------------------- */

/*
 * Function:  run_common_tests
 * Info:      Runs series of API calls.
 * Inputs:    API type
 * Return:    void
 */
static void run_common_tests(eClickApi eApiType)
{
    ClickSmsHandle *oClickSms = NULL;

    PRINT_MAIN_TEST_SEPARATOR

    switch (eApiType) {
        case CLICK_API_HTTP:
            click_debug_print("Executing HTTP API Tests with Username+Password as authentication method\n\n");

            ClickSmsString *sHttpUser = click_string_create(CFG_HTTP_USERNAME);
            ClickSmsString *sHttpPassword = click_string_create(CFG_HTTP_PASSWORD);
            ClickSmsString *sHttpApiId = click_string_create(CFG_HTTP_APIID);

            oClickSms = clickatell_sms_handle_init(eApiType,
                                                   sHttpUser,
                                                   sHttpPassword,
                                                   NULL,
                                                   sHttpApiId,
                                                   CFG_APICALL_TIMEOUT,
                                                   CFG_APICALL_CONNECT_TIMEOUT);
            if (oClickSms == NULL)
                click_debug_print("ERROR: Clickatell SMS Module Initialization failed\n");
            else
                run_common_api_calls(eApiType, oClickSms);

            clickatell_sms_handle_shutdown(oClickSms);

            click_string_destroy(sHttpUser);
            click_string_destroy(sHttpPassword);
            click_string_destroy(sHttpApiId);
            break;

        case CLICK_API_REST:
            click_debug_print("Executing REST API Tests with sApiKey as authentication method\n\n");

            ClickSmsString *sRestApiToken = click_string_create(CFG_REST_APIKEY);
            ClickSmsString *sRestApiId  = click_string_create(CFG_REST_APIID);

            oClickSms = clickatell_sms_handle_init(eApiType,
                                                   NULL,
                                                   NULL,
                                                   sRestApiToken,
                                                   sRestApiId,
                                                   CFG_APICALL_TIMEOUT,
                                                   CFG_APICALL_CONNECT_TIMEOUT);
            if (oClickSms == NULL)
                click_debug_print("ERROR: Clickatell SMS Module Initialization failed\n");
            else
                run_common_api_calls(eApiType, oClickSms);

            clickatell_sms_handle_shutdown(oClickSms);

            click_string_destroy(sRestApiToken);
            click_string_destroy(sRestApiId);
            break;

        default:
            click_debug_print("ERROR: Invalid API type selected!\n");
            break;
    }
}

/*
 * Function:  run_common_api_calls
 * Info:      Runs common API calls.
 *            Ensure function clickatell_sms_handle_init() has been called prior to this
 *            function.
 * Inputs:    eApiType  - API call type
 *            oClickSms - ClickSmsHandle handle returned from clickatell_sms_handle_init() call
 * Return:    void
 */
static void run_common_api_calls(eClickApi eApiType, ClickSmsHandle *oClickSms)
{
    int i = 0;
    ClickSmsString *sResponse = NULL;
    ClickSmsString *sMsgText = click_string_create(CFG_SAMPLE_MSG_TEXT);

    // ----------------------------------------------------------------------------------------
    // send a message to multiple mobile handsets
    // uncomment this if you wish to send a message to multiple handsets.
    // ----------------------------------------------------------------------------------------
/*
    click_debug_print("[%s: Send multiple SMSes]\n\n", (eApiType == CLICK_API_HTTP ? "HTTP" : "REST"));
    ClickMsisdn *aMsisdnsMulti = (ClickMsisdn *)calloc(1, sizeof(ClickMsisdn));
    aMsisdnsMulti->aDests = calloc(3, sizeof(ClickSmsString *));
    aMsisdnsMulti->iNum = 3;        // send to 2 mobile numbers in this sample code

    aMsisdnsMulti->aDests[0] = (ClickSmsString *)click_string_create(CFG_SAMPLE_MSISDN1);
    aMsisdnsMulti->aDests[1] = (ClickSmsString *)click_string_create(CFG_SAMPLE_MSISDN2);
    aMsisdnsMulti->aDests[2] = (ClickSmsString *)click_string_create(CFG_SAMPLE_MSISDN3);

    ClickSmsString *sMsgIds = clickatell_sms_message_send(oClickSms, sMsgText, aMsisdnsMulti);
    PRINT_SUB_TEST_SEPARATOR

    for (i = 0; i < aMsisdnsMulti->iNum; i++) // free memory allocated for destination addresses
        click_string_destroy((ClickSmsString *)(aMsisdnsMulti->aDests[i]));
    free(aMsisdnsMulti->aDests);
    free(aMsisdnsMulti);  // free container
*/
    // ----------------------------------------------------------------------------------------
    // send a message to one handset
    // ----------------------------------------------------------------------------------------
    click_debug_print("[%s: Send SMS]\n\n", (eApiType == CLICK_API_HTTP ? "HTTP" : "REST"));
    ClickMsisdn *aMsisdnsSingle = (ClickMsisdn *)calloc(1, sizeof(ClickMsisdn));
    aMsisdnsSingle->aDests = calloc(1, sizeof(ClickSmsString *));
    aMsisdnsSingle->iNum = 1;        // send to 1 mobile number
    aMsisdnsSingle->aDests[0] = (ClickSmsString *)click_string_create(CFG_SAMPLE_MSISDN1);

    ClickSmsString *sMsgIdResponse = clickatell_sms_message_send(oClickSms, sMsgText, aMsisdnsSingle);
    PRINT_SUB_TEST_SEPARATOR

    click_string_destroy((ClickSmsString *)(aMsisdnsSingle->aDests[0]));
    free(aMsisdnsSingle->aDests);
    free(aMsisdnsSingle);

    // retrieve apiMessageId field from response
    ClickSmsString *sMsgId = NULL;
    if (eApiType == CLICK_API_HTTP) {
        /* A successful response should look like this:   ID: 205e85d0578314037a96175249fc6a2b
         * which means we need to remove the 'ID:' prefix text and space character from the response
         */
        click_string_trim_prefix(sMsgIdResponse, 4);
        sMsgId = click_string_duplicate(sMsgIdResponse);
    }
    else { // REST
        int iPosStart = -1, iPosEnd = -1;

        /* A successful JSON response should look similar to this:
         *     {"data":{"message":[{"accepted":true,"to":"2771000000","apiMessageId":"77a4a70428f984d9741001e6f17d02b4"}]}}
         * We need to search for the apiMessageId field, before retrieving its value.
         */
        if ((iPosStart = click_string_find_cstr(sMsgIdResponse, "apiMessageId", 0)) > -1) {        // find start position of message ID key
            if ((iPosEnd = click_string_find_cstr(sMsgIdResponse, "\"", (iPosStart + 14))) > -1) { // find end position of message ID
                int iMsgIdSize = iPosEnd - 1 - (iPosStart + 14); // iPosEnd - terminating quotes character - (iPosStart +  apiMessageId":"  )
                char *chBuf = calloc(iMsgIdSize + 1, sizeof(char));

                strncpy(chBuf, (char *)((sMsgIdResponse->data) + iPosStart + 14), iMsgIdSize);
                chBuf[iMsgIdSize] = '\0';
                sMsgId = click_string_create(chBuf);

                free(chBuf);
            }
            else
                sMsgId = click_string_create("MSG NOT FOUND");
        }
        else
            sMsgId = click_string_create("MSG NOT FOUND");
    }

    // ----------------------------------------------------------------------------------------
    // get sms status (using message id received from 'send message' call)
    // ----------------------------------------------------------------------------------------
    click_debug_print("[%s: Get SMS status]\n\n", (eApiType == CLICK_API_HTTP ? "HTTP" : "REST"));
    sResponse = clickatell_sms_status_get(oClickSms, sMsgId);
    click_string_destroy(sResponse);
    PRINT_SUB_TEST_SEPARATOR

    // ----------------------------------------------------------------------------------------
    // get user account balance
    // ----------------------------------------------------------------------------------------
    click_debug_print("[%s: Get account balance]\n\n", (eApiType == CLICK_API_HTTP ? "HTTP" : "REST"));
    sResponse = clickatell_sms_balance_get(oClickSms);
    click_string_destroy(sResponse);
    PRINT_SUB_TEST_SEPARATOR

    // ----------------------------------------------------------------------------------------
    // get sms charge (using message id received from 'send message' call)
    // ----------------------------------------------------------------------------------------
    click_debug_print("[%s: Get SMS charge]\n\n", (eApiType == CLICK_API_HTTP ? "HTTP" : "REST"));
    sResponse = clickatell_sms_charge_get(oClickSms, sMsgId);
    click_string_destroy(sResponse);
    PRINT_SUB_TEST_SEPARATOR

    // ----------------------------------------------------------------------------------------
    // get coverage of route or number (using message id received from 'send message' call)
    // ----------------------------------------------------------------------------------------
    click_debug_print("[%s: Get coverage]\n\n", (eApiType == CLICK_API_HTTP ? "HTTP" : "REST"));
    ClickSmsString *coverage_msisdn = click_string_create(CFG_SAMPLE_COVERAGE_MSISDN);
    sResponse = clickatell_sms_coverage_get(oClickSms, coverage_msisdn);
    click_string_destroy(sResponse);
    click_string_destroy(coverage_msisdn);
    PRINT_SUB_TEST_SEPARATOR

    // ----------------------------------------------------------------------------------------
    // stop delivery of a message coverage (using message id received from 'send message' call)
    // ----------------------------------------------------------------------------------------
    click_debug_print("[%s: Stop an SMS]\n\n", (eApiType == CLICK_API_HTTP ? "HTTP" : "REST"));
    sResponse = clickatell_sms_message_stop(oClickSms, sMsgId);
    click_string_destroy(sResponse);
    PRINT_SUB_TEST_SEPARATOR

    click_string_destroy(sMsgIdResponse);
    click_string_destroy(sMsgIds);
    click_string_destroy(sMsgId);
    click_string_destroy(sMsgText);
}

/* ----------------------------------------------------------------------------- *
 * Main function which tests the Clickatell SMS library                          *
 * ----------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    // start using Clickatell library
    clickatell_sms_init();

    click_debug_print("========= Clickatell SMS module test application =========\n");

    // run Clickatell HTTP common API calls (with Username+Password as authentication)
    run_common_tests(CLICK_API_HTTP);

    // run Clickatell REST common API calls (with API Key as authentication)
    run_common_tests(CLICK_API_REST);

    // finished using Clickatell library
    clickatell_sms_shutdown();

    return 0;
}
