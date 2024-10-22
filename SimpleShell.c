#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <errno.h>
#include "shell.h"

int main(){
	//Find the user home directory from the environment (3)
        //Set current working directory to user home directory (3)
        char* home = getenv("HOME");
        chdir(home);
	
        //Save the current path (3)
        char* path = getenv("PATH");
        
        //Create array to store history and initialise counter
        int history_count = 0;
	history* history_arr = load_history(&history_count);
  
        //Create array to store history and initialse counter
        int alias_count = 0;        
        alias* alias_arr = load_aliases(&alias_count);

	// Delimiter characters for tokenizers
        char delim[] = " \n\t;&<>|";

	// Create size for input buffer and token array
	char input_buffer[512];
	char** tokens = malloc(sizeof(char*) * 50);
        while(1) {
                display_prompt();

		// Reset any values that may be in input buffer/token array
                strcpy(input_buffer, "");
		clear_tokens(tokens);
		
		// CTRL+D exit case + get input from user
		if(fgets(input_buffer, 512, stdin) == NULL) {
		  printf("\n");
		  break;
		}

                // Remove newline from input buffer
		input_buffer[strcspn(input_buffer, "\n")] = 0;
		if(strcmp(input_buffer, "") == 0) {
		  continue;
		} else if (strcmp(input_buffer, "exit") == 0) {
		  break;
		}
              
                // If command isn't history invoc, store to history
                if(input_buffer[0] != '!') {
                  history_arr = store_history(input_buffer, history_arr, &(history_count));
                } 
                
                //Tokenize input
		tokens = tokenizer(input_buffer, tokens, delim);
	        
	        //Tokens length == 0 when all input was delim characters
	        if(length(tokens) == 0) {
	          continue;
	        }
	        
	        // Check input for aliases
	        replace_aliases(input_buffer, tokens, alias_arr, delim);
		
		// History invocation check
		if(tokens[0][0] == '!') {
		  // command == empty string if no history invocation found
		  char* command = get_history_invoc(tokens, history_arr, history_count);
		  if(strcmp(command, "") != 0) {
		      printf("[shell] found history invocation '%s', replacing with '%s'\n", tokens[0], command); 

		      // get_history_invoc checks token arr length, only need
		      // to replace first token with found command
		      strcpy(tokens[0], command);

		      // retokenize incase history invocation was multi-argument;
		      tokens = tokenizer(tokens[0], tokens, delim);

		      // Only re-alias after history if it isn't
		      // another history invocation -- doesn't do
		      // a full alias check so doesn't always work
		      alias* check = find_alias(alias_arr, tokens[0]);
		      if(check && check->command[0] != '!') {
			replace_aliases(input_buffer, tokens, alias_arr, delim);
		      }
		  } else {
		    // get_history_invoc prints error message, continue to next input
		    continue;
		  }
		}

		// Check which command we're running
		if(strcmp(tokens[0], "cd") == 0) {
		  cd(tokens, home);
		} else if (strcmp(tokens[0], "getpath") == 0) {
		  getpath(tokens);
		} else if (strcmp(tokens[0], "setpath") == 0) {
		  setpath(tokens);
		} else if (strcmp(tokens[0], "history") == 0) {
		  print_history(tokens, history_arr, history_count);
		} else if (strcmp(tokens[0], "alias") == 0) {
		  run_alias(tokens, alias_arr, &alias_count);
		} else if (strcmp(tokens[0], "unalias") == 0) {
		  unalias(tokens, alias_arr, &alias_count);
		} else {
		  // If no built-in found, try run as an external program
		  ext_p(tokens);
		}
	}
	
	// Move to home dir to save alias/history files to right place
	chdir(home);
        save_history(history_arr);
        save_aliases(alias_arr);

	// Restore original path
        setenv("PATH", path, 1);
	printf("[shell] restored original path: %s\n", getenv("PATH"));
	printf("[shell] exiting..\n");

	// Properly free all allocated memory for arrays
	free_history(history_arr);
	free_aliases(alias_arr);
	free(tokens);
	return 0;
}

 /*
	Displays the shell prompt. Should make prompt constant?
	inputs: n/a
	outputs: n/a
	side effects: prints prompt to stdout
	written by Andrew, Fred and John
 */
void display_prompt(){
  char dir[PATH_MAX];
  printf("\033[1;32m");
  printf("%s ", getcwd(dir, sizeof(dir)));
  printf("\033[0m");
  printf("$ ");
}

// Input tokenizer - takes a string input and breaks it
// into character pointer array based on delims passed
// (takes "ls -la SimpleShell" -> ["ls", "-la", "SimpleShell", NULL])
char** tokenizer(char* input, char** dest, char delim[]) {
  char* token;
  int i = 0;
  token = strtok(input, delim);
  while(token != NULL) {
    dest[i] = token;
    i++;
    token = strtok(NULL, delim);
  }
  dest[i] = NULL;
  return dest;
}

// Reset token array
// Added instead of mallocing/freeing a new
// tokens array for each input
void clear_tokens(char* tokens[]) {
  for(int i=0; i<50; i++) {
    tokens[i] = NULL;
  }
}

// Finds+replaces aliases in tokens, edits token array in place
// Returns 1 if alias's have been found + replaced, 0 otherwise
void replace_aliases(char* input_buffer, char* tokens[], alias* alias_arr, char delim[]) {
  int recursion_count = 0;
  int replaced = 1;
  // Recursion counter to avoid infinite alias loops
  while(recursion_count < ALIAS_RECURSION_LIMIT) {
    // Don't do alias replacement if we're running alias or unalias
    if(strcmp(tokens[0], "alias") == 0 || strcmp(tokens[0], "unalias") == 0) {
      break;
    }

    // Loop through each token and check for an alias replacement
    for(int i=0; i<length(tokens); i++) {
      alias* a = find_alias(alias_arr, tokens[i]);
      if(a != NULL) {
	printf("[shell] found alias '%s', replacing with '%s'\n", a->alias, a->command);
	tokens[i] = a->command;
	replaced = 1;
	recursion_count++;
	// Extra check for aliases that can grow exponentially 
	// without making it back to the first recursion check
	if(recursion_count > ALIAS_RECURSION_LIMIT) {
	  break;
	}
      }
    }
                  
    // If commands have been replaced, we need to reconstruct 
    // the input string to tokenize it again
    if(replaced == 1) {
      replaced = 0;
      char* new_inp = tokens_to_string(tokens);
      strcpy(input_buffer, new_inp);
      tokens = tokenizer(input_buffer, tokens, delim);
      free(new_inp);
    } else {
      // Reached bottom of alias chain
      break;
    }    
  }
  if(recursion_count >= ALIAS_RECURSION_LIMIT) {
    printf("[shell] reached alias recursion limit\n");
  }
}

// Concatenates token array back into string
char* tokens_to_string(char* tokens[]) {
  char* new_inp = malloc(sizeof(char) * 512);
  strcpy(new_inp, "");
  int i =0;
  while(tokens[i]) {
    strcat(new_inp, tokens[i]);
    strcat(new_inp, " ");
    i++;
  }
  return new_inp;
}

// cd built-in
void cd(char* args[], char* home) {
  if(length(args) > 2) {
    printf("cd: too many arguments supplied (max 1)\n");
    return;
  } else if(!args[1]) {
    chdir(home);
    return;
  }
  
  if(chdir(args[1]) == -1) {
    if(errno == 2) {
      printf("cd: %s: No such file or directory\n", args[1]);
    } else if (errno == 20) {
      printf("cd: %s: Not a directory\n", args[1]);
    } else if (errno == 13) {
      printf("cd: %s: Permission denied\n", args[1]);
    } else {
      // shouldn't ever get to this line
      printf("cd: %s: Unknown error\n", args[1]);
    }
  }
}

// getpath built-in
void getpath(char* args[]) {
  if(length(args) > 1) {
    printf("getpath: too many arguments supplied (max 0)\n");
  } else {
    printf("Path: %s\n", getenv("PATH"));
  }
}

// setpath built-in
void setpath(char* args[]) {
  if(!args[1]) {
    printf("setpath: no directory supplied (usage: setpath <dir>)\n");
  } else if (length(args) > 2) {
    printf("setpath: too many arguments supplied (max 1)\n");
  } else {
    setenv("PATH", args[1], 1);
    printf("setpath: %s\n", args[1]);
  }
}

// alias built-in
void run_alias(char* args[], alias* alias_arr, int* alias_count) {
  if(length(args) == 1) {
    print_aliases(alias_arr);
  } else if (length(args) == 2) {
    printf("alias: not enough arguments supplied (usage: alias <name> <command>)\n");
  } else {
    create_alias(alias_arr, alias_count, args);
  }
}

// unalias built-in
void unalias(char* args[], alias* alias_arr, int* alias_count) {
  if(length(args) > 2) {
    printf("unalias: too many arguments specified (max 1)\n");
  } else if (length(args) == 1) {
    printf("unalias: no arguments specified (usage: unalias <alias>)\n");
  } else if (find_alias(alias_arr, args[1]) == NULL) {
    printf("unalias: alias %s not found\n", args[1]);
  } else {
    alias* current = find_alias(alias_arr, args[1]);
    alias* next_current = alias_arr; 
    printf("unalias: alias %s deleted\n", current->alias);
    while(strcmp(next_current->next->alias, current->alias) != 0) {
      next_current = next_current->next;
    }
    next_current->next = current->next;
    free(current);
    (*alias_count)--;  
  }
}

// run command as external process
void ext_p(char* args[]) {
  pid_t pid = fork();
  if(pid < 0) {
    fprintf(stderr, "Fork Failed\n");
    exit(1);
  } else if (pid == 0) {
    execvp(args[0], args);
    perror(args[0]);
    exit(1);
  } else {
    wait(NULL);
  }
}

// length helper
// used to check number of arguments in token array
int length(char* args[]) {
  int length = 0;
  while(args[length]) {
    length++;
  }
  return length;
}

// Find a history invocation from input of form "!.."
char* get_history_invoc(char* tokens[], history* arr, int count) {
  if(length(tokens) > 1) {
    printf("!: too many arguments specified (usage !! | !<num> | !-<num>)\n");
    return "";
  } else if (count == 0) {
    printf("!: no history entrys to invoc\n");
    return "";
  } else {
    signed int offset = -999;
    char* rtest = malloc(sizeof(char) * 50);
    strcpy(rtest, "");
    sscanf(tokens[0], "!%i%s", &offset, rtest);
    
    // !! case
    if(tokens[0][1] == '!' && strlen(tokens[0]) == 2) { offset = count; }

    // rtest being blank + offset changed from -999
    // indicates that no non-digits were input
    if(strcmp(rtest, "") == 0 && offset != -999) {
      // Convert offset value to index in history array
      int idx = offset < 0 ? (count+offset+1) : offset;
      
      if(idx < 1 || idx > count) {
        printf("!: history index %d out of range\n", idx);
        free(rtest);
        return "";
      } else { 
        int count = 1;
        history* current = arr;
        while(count < idx) {
          current = current->next;
          count++;
        }
        free(rtest);
        return current->command;
      }
    } else {
      printf("%s: invalid history invocation (non-numerals detected)\n", tokens[0]);
      free(rtest);
      return "";
    }
  }
}

// Store command to history
history* store_history(char* input, history* arr, int* count) {
    // history_arr == NULL when no history is stored
    if(arr == NULL) {
      arr = malloc(sizeof(history));
      strcpy(arr->command, input);
      arr->next = NULL;
    } else {
      // Otherwise create new entry
      history* current = arr;
      while(current->next != NULL) {
        current = current->next;
      }
      
      history* entry = malloc(sizeof(history));
      strcpy(entry->command, input);
      entry->next = NULL;
      current->next = entry;
    }

    // If history is full then remove first entry
    if(*count == 20) {
      history* tmp = arr->next;
      free(arr);
      return tmp;
    } else {
      (*count)++;
      return arr;
    }
}

// Print history built-in
void print_history(char* args[], history* history_arr, int history_count) {
  if(length(args) > 1) {
    printf("history: too many arguments supplied (max 1)\n");
    return;
  }

  int count = 1;
  history* arr = history_arr;
  while(arr) {
    printf("%d: %s\n", count, arr->command);
    count++;
    arr = arr->next;
  }
}

// Print aliases built-in
void print_aliases(alias* aliases) {
  alias* current = aliases->next;
  if(current == NULL) {
    printf("alias: no aliases set\n");
  }
  while(current != NULL) {
    printf("alias %s = \"%s\"\n", current->alias, current->command);
    current = current->next;
  }
}

// Store alias
void create_alias(alias* alias_arr, int* aliasCount, char* args[]) {
  if(*aliasCount >= 20) {
    printf("alias: maximum aliases created\n");
  } else if (find_alias(alias_arr, args[1]) != NULL) {
    printf("alias: alias %s already exists\n", args[1]);
  } else {
    alias* new_alias = malloc(sizeof(alias));
    strcpy(new_alias->alias, args[1]);
    
    // Build argument string out of multiple tokens    
    int i=3;
    char* alias_cmd = malloc(sizeof(char) * 512);
    strcpy(alias_cmd, args[2]);
    while(args[i]) {
      strcat(alias_cmd, " ");
      strcat(alias_cmd, args[i]);
      i++;
    }
    
    strcpy(new_alias->command, alias_cmd);
    free(alias_cmd);

    // Store new alias to array
    printf("alias successful: alias: %s, command: %s\n", new_alias->alias, new_alias->command);
    alias* current = alias_arr; 
    while(current->next != NULL) {
      current = current->next;
    }
    current->next = new_alias;
    new_alias->next = NULL;
    (*aliasCount)++;
  }
}

// find_alias helper -> returns null if alias doesn't exist
alias* find_alias(alias* alias_arr, char* str) {
  if(alias_arr == NULL) {
    return NULL;
  }
  alias* current = alias_arr;
  while(current != NULL) {
    if(strcmp(current->alias, str) == 0) {
      // Found alias, return it
      return current;
    } else {
      current = current->next;
    }
  }
  // Looped through whole array, no match
  return NULL;
}

// load history from file
history* load_history(int* history_count) {
    FILE *file = fopen(".hist_list", "r");
    history* arr = NULL;
    if(file == NULL) {
      printf("[shell] no .hist_list file found\n");
      return arr;
    } else {
      // Empty file check
      fseek(file, 0, SEEK_END);
      if(ftell(file) == 0) {
        return arr;
      } else {
        fclose(file);
        file = fopen(".hist_list", "r");
      }
      // Read history from file
      char line[512];
      arr = malloc(sizeof(history));
      history* current = arr;
      int count = 0;

      // Create history entry for each line in file
      while(fgets(line, sizeof(line), file) != NULL) {
          history* entry = malloc(sizeof(history));
          sscanf(line, "%[^\n]", current->command);
          current->next = entry;
          current = current->next;
          count++;
      }

      // Adds blank entry to end of history array,
      // removed here
      current = arr;
      while(current->next->next) {
        current = current->next;
      }
      free(current->next);
      current->next = NULL;

      // Updates history count to correct value
      (*history_count) = count;
      fclose(file);
    }
    return arr;
}

// load aliases from file
alias* load_aliases(int* alias_count) {
    FILE *file = fopen(".aliases", "r");
    alias* arr = malloc(sizeof(alias));
    if(file == NULL) {
      printf("[shell] no .aliases file found\n");
      return arr;
    } else {
      // Empty file check
      fseek(file, 0, SEEK_END);
      if(ftell(file) == 0) {
        return arr;
      } else {
        fclose(file);
        file = fopen(".aliases", "r");
      }
      
      char line[565];
      alias* current = arr;
      int count = 0;
      // Create alias entry for each line in file
      while(fgets(line, sizeof(line), file) != NULL) {
          alias* entry = malloc(sizeof(alias));
          sscanf(line, "alias %s %[^\n]", entry->alias, entry->command);
          current->next = entry;
          current = current->next;
          count++;
      }
      // Update alias count
      (*alias_count) = count;
      fclose(file);
    }
    return arr;
}

// Save history to file
void save_history(history* arr) {
  FILE* file = fopen(".hist_list", "w");
  if(file == NULL) {
    printf("[shell] error accessing .hist_list\n"); 
  } else {
    history* current = arr;
    while(current != NULL) {
      fprintf(file, "%s\n", current->command);
      current = current->next;
    }
  }
  fclose(file);
}

// Save aliases to file
void save_aliases(alias* arr) {
  FILE* file = fopen(".aliases", "w");
  if(file == NULL) {
    printf("[shell] error accessing .aliases\n");
  } else {
    alias* current = arr->next;
    while(current != NULL) {
      fprintf(file, "alias %s %s\n", current->alias, current->command);
      current = current->next;
    }
    fclose(file);
  }
}

// Memory management helpers
void free_aliases(alias* arr) {
  alias* next = arr->next;
  free(arr);
  while(next != NULL) {
    alias* tmp = next->next;
    free(next);
    next = tmp;
  }
}

void free_history(history* arr) {
  if(arr != NULL) {
    history* next = arr->next;
    free(arr);
    while(next != NULL) {
      history* tmp = next->next;
      free(next);
      next = tmp;
    }
  }
}
