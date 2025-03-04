#include <Arduino.h>
#include <cstddef>
#include <cstring>
#include "command.h"
#include "config.h"

static cmd_err_t help_command(int argc, char* argv[]);
static cmd_err_t version_command(int argc, char *argv[]);
static cmd_err_t uptime_command(int argc, char *argv[]);

#define MAX_NUM_COMMANDS 64
static command_t console_command_table_array[MAX_NUM_COMMANDS+1] = {
  { "help",             help_command,        0, NULL, NULL, NULL,    "help [command name]"},
  { "version",          version_command,     0, NULL, NULL, NULL,    "get FW version"},
  { "uptime",           uptime_command,      0, NULL, NULL, NULL,    "running time since reboot"},
  { NULL,               NULL,                0, NULL, NULL, NULL,    NULL}
};

// output of each command and/or error message is saved in this array
#define COMMAND_OUTPUT_MAX_LEN 128
static char console_printf_buffer[COMMAND_OUTPUT_MAX_LEN];
// static int console_printf_buffer_len = 0;

#define DELIMIT_STR  " "  // space only
#define DELIMIT_CHAR ' '  // space only
#define MAX_PARAMS 3      // max number of arguments = 3-1 = 2

#define MAX_COMMAND_LEN 128
static char console_buffer[MAX_COMMAND_LEN];
static uint8_t console_cursor_position = 0;


/* default error strings */
static const char* const console_error_strings[] =
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
    const command_t* cmd_ptr_to_add = commands;

    int num_commands_total = 0;   // total number of commands in the command look up table
    while (NULL != console_command_table_array[num_commands_total].command)
      num_commands_total++;
    
    while ((cmd_ptr_to_add->name != NULL) && (num_commands_total<MAX_NUM_COMMANDS)){ 
        console_command_table_array[num_commands_total] = *cmd_ptr_to_add;
        cmd_ptr_to_add++;
        num_commands_total++;
    }
    console_command_table_array[num_commands_total].command = NULL;  //End of table marker

    Serial.printf("Serial terminal command line interface.  Left arrow: ctrl-b, Right arrow: ctrl-f, Home: ctrl-a, End: ctrl-e.\r\n");
    Serial.printf("Need to set Flow control to None in putty.\r\n>");
    Serial.flush();
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

    console_printf_buffer[0] = '\0';

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
            if ((err == ERR_CMD_OK) && (cmd_ptr->command != NULL)){
                err = (cmd_err_t) cmd_ptr->command(param_cnt, params);                
            }
        }

        // process errors
        if (err != ERR_CMD_OK){  
            if ((err <= 0) && (err > ERR_LAST_ERROR)){
                console_printf("ERROR: %s", console_error_strings[-err]);                
            }
        }
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
      // buffer is not empty and it's not just delimit characters      
      Serial.print("\r\n");

      err = console_parse_cmd(console_buffer);
      if (strlen(console_printf_buffer)){
        // print the output message
        Serial.print(console_printf_buffer); 
      }

    }
    /* prepare for a new line of entry */
    console_buffer[0] = 0;
    console_cursor_position = 0;    
    Serial.print("\r\n>");

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
  while (Serial.available() > 0){    
    // need to set Flow control to None in putty
    console_process_char(Serial.read());
  }
}

int console_printf(const char *format, ...) {
  // print message into a buffer for processing (print to serial console, or sent over internet) layer
  // return: the number of characters written in the buffer
  int len;
  va_list args;
  va_start(args, format);
  len = vsnprintf(console_printf_buffer + strlen(console_printf_buffer), COMMAND_OUTPUT_MAX_LEN - strlen(console_printf_buffer), format, args);
  va_end(args);
  return len;
}


const char* get_command_output(){
  return console_printf_buffer;
}


/* help function */
static cmd_err_t help_command(int argc, char* argv[]){
    const command_t* cmd_ptr;
    cmd_err_t err = ERR_CMD_OK;
    uint32_t eg_sel = 0;
    int table_index;

    switch (argc){
        case 0:
        case 1:
            console_printf("Console Commands:\r\n");
            for ( cmd_ptr = console_command_table_array; cmd_ptr->name != NULL; cmd_ptr++ ){                
              console_printf( "    %s\r\n", cmd_ptr->name);                
            }
            break;

        case 2:
            err = ERR_UNKNOWN_CMD;
            if ((cmd_ptr = lookup_command(argv[1])) != NULL){
                if ( cmd_ptr->help_example != NULL ){
                    err = cmd_ptr->help_example(argv[1], eg_sel);
                }else if (cmd_ptr->brief != NULL){
                    err = ERR_CMD_OK;
                    console_printf("%s\n", cmd_ptr->brief);
                }
                else{
                    err = ERR_CMD_OK;
                    console_printf("No example available for %s\n\n", argv[1]);
                }
            }
            break;
        default:
            console_printf("\t help [command name]\n\r");
            err=ERR_BAD_ARG;
            break;
    }
    return err;
}

static cmd_err_t version_command(int argc, char *argv[])
{
  console_printf("Version %s", VERSION_STRING);
  return ERR_CMD_OK;
}

static cmd_err_t uptime_command(int argc, char *argv[])
{
  int t_sec = esp_timer_get_time() / 1000000;
  console_printf("Up %d days %02d:%02d:%02d since boot",
                  t_sec / 3600 / 24,
                  (t_sec / 3600) % 24,
                  (t_sec / 60) % 60,
                  t_sec % 60
                  );
  return ERR_CMD_OK;
}