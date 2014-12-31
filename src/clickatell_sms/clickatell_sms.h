#ifndef CLICKATELL_SMS_H
#define CLICKATELL_SMS_H

/*
 * clickatell_sms.h
 *
 * Clickatell SMS Library:
 *
 * This library allows a developer to integrate their application
 * with the both the Clickatell REST API and Clickatell HTTP API.
 * The following public functions are provided.
 *   -  send MT message(s)
 *   -  get user’s credit balance
 *   -  get message status
 *   -  get message ClickSmsStringge
 *   -  get coverage
 *   -  stop message
 *
 *  Martin Beyers <martin.beyers@clickatell.com>
 */

/*
 * Structure that acts as a handle when calling API functions.
 * It is returned during a successful initialization call.
 */
typedef struct ClickSmsHandle ClickSmsHandle;

// Enumeration Clickatell APIs supported in this library
typedef enum eClickApi {
    CLICK_API_HTTP, // HTTP API using Username+Password to authenticate
    CLICK_API_REST, // REST API using sApiKey (an auth token) to authenticate
    CLICK_API_COUNT // count of supported APIs
} eClickApi;

// destination address container (used for send message API call only)
typedef struct ClickMsisdn {
    int iNum;                 // number of destination ("to") addresses
    ClickSmsString **aDests;   // array of pointers to destination addresses
} ClickMsisdn;

// Public function declarations
void clickatell_sms_init(void);
void clickatell_sms_shutdown(void);
ClickSmsHandle *clickatell_sms_handle_init(eClickApi eApiType, const ClickSmsString *sUsername, const ClickSmsString *sPassword,
                                           const ClickSmsString *sApiKey, const ClickSmsString *sApiId, long iTimeout, long iConnectTimeout);
void clickatell_sms_handle_shutdown(ClickSmsHandle *oClickSms);
ClickSmsString *clickatell_sms_message_send(ClickSmsHandle *oClickSms, const ClickSmsString *sText, ClickMsisdn *aMsisdns);
ClickSmsString *clickatell_sms_status_get(ClickSmsHandle *oClickSms, const ClickSmsString *sMsgId);
ClickSmsString *clickatell_sms_balance_get(ClickSmsHandle *oClickSms);
ClickSmsString *clickatell_sms_charge_get(ClickSmsHandle *oClickSms, const ClickSmsString *sMsgId);
ClickSmsString *clickatell_sms_coverage_get(ClickSmsHandle *oClickSms, const ClickSmsString *msisdn);
ClickSmsString *clickatell_sms_message_stop(ClickSmsHandle *oClickSms, const ClickSmsString *sMsgId);

#endif // CLICKATELL_SMS_H
