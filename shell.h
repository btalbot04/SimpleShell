typedef struct history_ {
  char command[512];
  struct history_* next;
} history;

typedef struct alias_ {
  char alias[512];
  char command[512];
  struct alias_* next;
} alias;

#define ALIAS_RECURSION_LIMIT 32

// Input helpers
void display_prompt();
char** tokenizer(char* input_buffer, char** dest, char delim[]);
void replace_aliases(char* input_buffer, char* tokens[], alias* alias_arr, char delim[]);
void clear_tokens(char** tokens);
int length(char* args[]);
char* tokens_to_string(char* tokens[]);

// Built-ins
void cd(char* args[], char* home);
void getpath(char* args[]);
void setpath(char* args[]);
void run_alias(char* args[], alias* alias_arr, int* alias_count);
void ext_p(char* args[]);

// History helpers
char* get_history_invoc(char* tokens[], history* arr, int count);
history* store_history(char* input_buffer, history* arr, int* count);

// Alias helpers
void create_alias(alias* alias_arr, int* alias_count, char* args[]);
alias* find_alias(alias* alias_arr, char* str);
void unalias(char* args[], alias* alias_arr, int* alias_count);

// Printing functions
void print_history(char* args[], history* history_arr, int history_count);
void print_aliases(alias* aliases);

// Loading
history* load_history(int* history_count);
alias* load_aliases(int* alias_count);

// Saving
void save_history(history* arr);
void save_aliases(alias* arr);

// Memory management
void free_history(history* arr);
void free_aliases(alias* arr);
