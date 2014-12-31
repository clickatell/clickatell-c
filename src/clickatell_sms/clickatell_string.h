#ifndef CLICKATELL_STRING_H
#define CLICKATELL_STRING_H

/*
 * Clickatell SMS string function declarations
 *
 * This is a simple string module used by the Clickatell SMS library.
 *
 * Martin Beyers <martin.beyers@clickatell.com>
 */

 /*
 * Structure that holds a string. Used because reallocating a string can cause
 * its address to change. So rather pass a structure as a parameter to a function
 * since the structure's address will not change if we modify the address of the string
 * field inside it.
 */
typedef struct ClickSmsString{
    char *data;
} ClickSmsString;

// macro to validate a ClickSmsString
#define CLICK_STR_INVALID(sBuf)  ((sBuf) == NULL || (sBuf)->data == NULL)

// function declarations
ClickSmsString *click_string_create(const char *chStr);
void click_string_destroy(ClickSmsString *sBuf);
void click_string_trim_prefix(ClickSmsString *sBuf, unsigned int iLen);
int click_string_find_cstr(const ClickSmsString *sHaystack, char *chNeedle, unsigned int iStartPos);
ClickSmsString *click_string_duplicate(const ClickSmsString *sBuf);
char *click_string_retrieve_cstr(const ClickSmsString *sBuf);
void click_string_append(ClickSmsString *sDest, const ClickSmsString *sSource, const char *chSource);
void click_string_append_formatted_cstr(ClickSmsString *sDest, const char *chFormat, ...);
void click_string_url_encode(ClickSmsString *sBuf);

#endif // CLICKATELL_STRING_H
