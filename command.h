#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>

typedef enum{
    ERR_CMD_OK           =  0,
    ERR_UNKNOWN          = -1,
    ERR_UNKNOWN_CMD      = -2,
    ERR_INSUFFICENT_ARGS = -3,
    ERR_TOO_MANY_ARGS    = -4,
    ERR_ADDRESS          = -5,
    ERR_NO_CMD           = -6,
    ERR_TOO_LARGE_ARG    = -7,
    ERR_OUT_OF_HEAP      = -8,
    ERR_BAD_ARG          = -9,
    ERR_LAST_ERROR       = -10
} cmd_err_t;


typedef cmd_err_t (*command_function_t)(int argc, char *argv[]);
typedef cmd_err_t (*help_example_function_t)( char* command_name, uint32_t eg_select );


typedef struct
{
    char* name;                             /* The command name matched at the command line. */
    command_function_t command;             /* Function that runs the command. */
    int arg_count;                          /* Minimum number of arguments. */
    const char* delimit;                    /* Custom string of characters that may delimit the arguments for this command - NULL value will use the default for the console. */

    /*
     * These three elements are only used by the help, not the console dispatching code.
     * The default help function will not produce a help entry if both format and brief elements
     * are set to NULL (good for adding synonym or short form commands).
     */
    help_example_function_t help_example;   /* Command specific help function. Generally set to NULL. */
    char *format;                           /* String describing argument format used by the generic help generator function. */
    char *brief;                            /* Brief description of the command used by the generic help generator function. */
} command_t;


#define CMD_TABLE_END      { NULL, NULL, 0, NULL, NULL, NULL, NULL }


// int            console_add_cmd_table ( const command_t *commands );
void init_cmd(const command_t* commands);
cmd_err_t console_parse_cmd(const char* line);
int console_printf(const char *format, ...);    // if anything should be printed in the user-defined command, use this function 
void command_console_task();

#endif