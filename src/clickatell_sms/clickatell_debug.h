#ifndef CLICKATELL_DEBUG_H
#define CLICKATELL_DEBUG_H

/*
 * clickatell_debug.h
 *
 *  Simple debug module used by the Clickatell SMS library.
 *
 *  Martin Beyers <martin.beyers@clickatell.com>
 */

// Enumeration of debug options
typedef enum eClickDebugOption {
    CLICK_DEBUG_ON,     // Turn debug On
    CLICK_DEBUG_OFF,    // Turn debug Off
    CLICK_DEBUG_COUNT   // count of options
} eClickDebugOption;

void click_debug_init(eClickDebugOption click_debug_opt);
void click_debug_print(const char *chFormat, ...);

#endif // CLICKATELL_DEBUG_H
