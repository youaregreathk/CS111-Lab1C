// UCLA CS 111 Lab 1 command internals

typedef enum token_type
  {
    AND_TOKEN,            // &&
    SEQUENCE_TOKEN,       // ;
    OR_TOKEN,             // ||
    PIPE_TOKEN,           // |
    SIMPLE_TOKEN,         // any word
    OPEN_PARENS_TOKEN,    // (
    CLOSE_PARENS_TOKEN,   // )
    INPUT_TOKEN,          // <
    OUTPUT_TOKEN,         // >
    NEWLINE_TOKEN,        // \n
    COMMENT_TOKEN,        // #something\n
    UNKNOWN_TOKEN         // anything else
  } token_type_t;

typedef enum command_type
  {
    AND_COMMAND,         // A && B
    SEQUENCE_COMMAND,    // A ; B
    OR_COMMAND,          // A || B
    PIPE_COMMAND,        // A | B
    SIMPLE_COMMAND,      // a simple command
    SUBSHELL_COMMAND,    // ( A )
  } command_type_t;

typedef struct token
{
  token_type_t type;
  int line;
  char *word;
  struct token* next;
  struct token* prev;
} token;

// Data associated with a command.
typedef struct command
{
  enum command_type type;

  // Exit status, or -1 if not known (e.g., because it has not exited yet).
  int status;

  // I/O redirections, or null if none.
  char *input;
  char *output;

  union
  {
    // for AND_COMMAND, SEQUENCE_COMMAND, OR_COMMAND, PIPE_COMMAND:
    struct command *command[2];

    // for SIMPLE_COMMAND:
    char **word;

    // for SUBSHELL_COMMAND:
    struct command *subshell_command;
  } u;
} command;

typedef struct command_node
{
  command_t command;
  struct command_node *next;
  struct command_node *prev;
} command_node;

typedef struct command_stream
{
  struct command_node *head;
  struct command_node *tail;
  struct command_node *cursor; // initialize to head
} command_stream;

/**
*  Time Travel
**/

typedef struct graph_node
{
  command_t root;
  struct graph_node **before;
  int before_size;
  pid_t pid;

  struct graph_node *next;
  struct graph_node *prev;
} graph_node;

typedef struct dependency_graph
{
  graph_node *no_dependencies;
  graph_node *dependencies;
} dependency_graph;

typedef struct list_node
{
  graph_node *node;
  char **read_list;
  char **write_list;
  int read_size;
  int write_size;

  struct list_node *next;
  struct list_node *prev;
} list_node;
