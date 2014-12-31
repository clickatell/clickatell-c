/*
 * clickatell_debug.c
 *
 *  Simple debug module used by the Clickatell SMS library.
 *
 *  Martin Beyers <martin.beyers@clickatell.com>
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "clickatell_debug.h"

/* ----------------------------------------------------------------------------- *
 * Local variables                                                               *
 * ----------------------------------------------------------------------------- */

static eClickDebugOption eLocalDebugOpt = CLICK_DEBUG_OFF;
static int iDebugInitialized = 0;

/* ----------------------------------------------------------------------------- *
 * Public function definitions                                                   *
 * ----------------------------------------------------------------------------- */

/*
 * Function:  click_debug_init
 * Info:      Initialize the debug module.
 * Inputs:    eDebugOption - Option to turn debug on/off
 * Outputs:   None
 * Return:    void
 */
void click_debug_init(eClickDebugOption eDebugOption)
{
    if (eDebugOption >= 0 && eDebugOption < CLICK_DEBUG_COUNT) {
        eLocalDebugOpt = eDebugOption;
        iDebugInitialized = 1;
    }
}

/*
 * Function:  click_debug_print
 * Info:      Formats and prints out a variable argument aDests. At present this function's
 *            functionality is similar to a printf call except that the function will not sOutput
 *            debug if debug was disabled.
 * Inputs:    chFormat - Name of the last parameter before the variable argument aDests.
 *            args   - variable number of args
 * Outputs:   None
 * Return:    void
 */
void click_debug_print(const char *chFormat, ...)
{
    if (iDebugInitialized == 0 || chFormat == NULL || eLocalDebugOpt != CLICK_DEBUG_ON)
        return;

    va_list argList;

    va_start(argList, chFormat);
    vprintf(chFormat, argList);  // vprintf takes a va_list as input (rather than a variable no. of args which printf does)

    va_end(argList);
}
