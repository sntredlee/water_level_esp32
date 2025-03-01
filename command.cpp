#include <Arduino.h>
#include <cstddef>
#include <cstring>
#include "command.h"

static const command_t* console_command_table_array=NULL;

#define DELIMIT_STR  " "  // space only
#define DELIMIT_CHAR ' '  // space only
#define MAX_PARAMS 3   // max number of arguments = 3-1 = 2

#define MAX_COMMAND_LEN 128
static char console_buffer[MAX_COMMAND_LEN];
static uint8_t console_cursor_position = 0;


/* default error strings */
static const char* const console_default_error_strings[] =
{
    (char*) "OK",                      /* ERR_CMD_OK */
    (char*) "Unknown Error",           /* ERR_UNKNOWN */
    (char*) "Unknown Command",         /* ERR_UNKNOWN_CMD */
    (char*) "Insufficient Arguments",  /* ERR_INSUFFICENT_ARGS */
    (char*) "Too Many Arguments",      /* ERR_TOO_MANY_ARGS */
    (char*) "Bad Address Value",       /* ERR_ADDRESS */
    (char*) "No Command Entered",      /* ERR_NO_CMD */
    (char*) "Argument too large",      /* ERR_TOO_LARGE_ARG */
    (char*) "Out of heap space",       /* ERR_OUT_OF_HEAP */
    (char*) "Bad argument provided"    /* ERR_BAD_ARG */
};


void init_cmd(const command_t* commands)
{
    console_command_table_array = commands;    
}


const command_t* lookup_command(const char *command_name){
    const command_t* cmd_ptr;
    for (cmd_ptr = console_command_table_array; cmd_ptr->name != NULL; cmd_ptr++){
        if (strcmp(command_name, cmd_ptr->name ) == 0){
            return cmd_ptr;
        }
    }
    return NULL;
}

cmd_err_t console_parse_cmd(const char* line){
    // no re-entrant
    cmd_err_t err = ERR_CMD_OK;
    const command_t* cmd_ptr = NULL;
    char* params[MAX_PARAMS + 1]; // last param is always NULL as marker of end
    uint8_t param_cnt = 0;
    char* saveptr = NULL;
    char copy[strlen(line) + 1];
    
    /* Copy original buffer into local buffer, as tokenize will change the original string */
    strcpy(copy, line);

    /* First call to strtok. */
    params[param_cnt++] = strtok_r(copy, DELIMIT_STR, &saveptr);

    if (params[0] == NULL){
        /* no command entered */
        err = ERR_NO_CMD;
    }else{
        /* find the command */
        if ((cmd_ptr = lookup_command(params[0])) == NULL){
            err = ERR_UNKNOWN_CMD;
        }else{
            uint32_t i = 0;   

            /* parse arguments */
            while (saveptr != NULL && saveptr[0] != '\0'){                
                /* Check for delimiters first */
                while ((saveptr[0] != '\0') && (saveptr[0] == DELIMIT_CHAR)){
                    saveptr++;                            
                }

                params[param_cnt] = strtok_r(NULL, DELIMIT_STR, &saveptr);                
                if (params[param_cnt] != NULL){
                    param_cnt++;
                    if (param_cnt > MAX_PARAMS){
                        err = ERR_TOO_MANY_ARGS;
                        break;
                    }
                }
            }
            if (err == ERR_CMD_OK){
                params[param_cnt] = NULL;
            }

            /* check arguments */
            if ((param_cnt - 1) < cmd_ptr->arg_count){
                err = ERR_INSUFFICENT_ARGS;
            }

            /* run command */
            if ( (err == ERR_CMD_OK) && (cmd_ptr->command != NULL) ){                
                err = (cmd_err_t) cmd_ptr->command(param_cnt, params);                
            }
        }

#if DEBUG
        /* process errors */
        if (err != ERR_CMD_OK){
            if ((err <= 0) && (err > ERR_LAST_ERROR)){
                Serial.printf("ERROR: %s\r\n", console_error_strings[-err]);
            }
            if ( err != ERR_UNKNOWN_CMD ){
                Serial.printf("Usage: %s %s\r\n",cmd_ptr->name, cmd_ptr->format);
            }
        }
#endif        
    }
    return err;
}


static void console_insert_char(char c){
    uint32_t i;
    uint32_t len = strlen(console_buffer);

    /* move the end of the line out to make space */
    for (i=len+1; i > console_cursor_position; i--){
        console_buffer[i] = console_buffer[i - 1];
    }

    /* insert the character */
    len++;
    console_buffer[console_cursor_position] = c;

    /* print out the modified part of the ConsoleBuffer */
    Serial.print(&console_buffer[console_cursor_position]);

    /* move the cursor back to where it's supposed to be */
    console_cursor_position++;
    for (i = len; i > console_cursor_position; i-- ){
        Serial.print( '\b' );
    }
}

static void console_remove_char(){
    uint32_t i;
    uint32_t len = strlen( console_buffer );

    /* back the rest of the line up a character */
    for ( i = console_cursor_position; i < len; i++ ){
        console_buffer[i] = console_buffer[i + 1];
    }
    len--;

    /* print out the modified part of the ConsoleBuffer */
    Serial.print( &console_buffer[console_cursor_position] );

    /* overwrite the extra character at the end */
    Serial.print(" \b");

    /* move the cursor back to where it's supposed to be */
    for (i = len; i > console_cursor_position; i--){
        Serial.print( '\b' );
    }
}


static void console_do_home(){
    console_cursor_position = 0;
    Serial.print('\r');
    Serial.print('>');
}


static void console_do_end(){
    Serial.print(&console_buffer[console_cursor_position]);
    console_cursor_position = strlen(console_buffer);
}


void console_do_left(){
    if (console_cursor_position > 0){
        Serial.print('\b');
        console_cursor_position--;
    }
    else{
        Serial.print('\a');  // console_bell_string
    }
}

void console_do_right(){
    if (console_cursor_position < strlen(console_buffer)){
        Serial.print(console_buffer[console_cursor_position]);
        console_cursor_position++;
    }else{
        Serial.print('\a');  // console_bell_string
    }
}

void console_do_delete(){
    if (console_cursor_position < strlen(console_buffer)){        
        console_remove_char( );
    }else{
        Serial.print('\a');  // console_bell_string
    }
}

void console_do_backspace(){
    if (console_cursor_position > 0){        
        console_cursor_position--;
        Serial.print('\b');
        console_remove_char( );
    }else{
        Serial.print('\a');  // console_bell_string
    }
}

cmd_err_t console_do_enter(){
    cmd_err_t err = ERR_CMD_OK;
    if (strlen(console_buffer) && (strlen(console_buffer) > strspn(console_buffer, DELIMIT_STR))){        
        /* buffer is not empty and it's not just delimit characters */
        err = console_parse_cmd(console_buffer);
    }
    
    /* prepare for a new line of entry */
    console_buffer[0] = 0;
    console_cursor_position = 0;
    Serial.print('>');

    return err;
}

cmd_err_t console_process_char(char c){
    cmd_err_t err = ERR_CMD_OK;        
    switch (c){        
        case '\r': /* newline */
            err = console_do_enter( );            
            break;
        case '\b': /* backspace */
        case '\x7F': /* backspace */
            console_do_backspace( );
            break;
        case 2: /* ctrl-b */
            console_do_left( );
            break;
        case 6: /* ctrl-f */
            console_do_right( );
            break;
        case 1: /* ctrl-a */
            console_do_home( );
            break;
        case 5: /* ctrl-e */
            console_do_end( );
            break;
        case 4: /* ctrl-d */
            console_do_delete( );
            break;
              
        default:
            if ( (c > 31) && (c < 127) ){ /* limit to printables */
                if (strlen(console_buffer) + 1 < MAX_COMMAND_LEN){                    
                    console_insert_char( c );
                }
                else{
                    Serial.print("\a" );  // console_bell_string
                }
            }else{
                Serial.print("\a" );  // console_bell_string
            }
            break;
    }    
    return err;
}

void command_console_task(){   
  char c;   
    while (Serial.available() > 0){
      c = Serial.read();
#if DEBUG
      Serial.printf("Got %c\n", c);
#endif
      console_process_char(c);
    }     
}
