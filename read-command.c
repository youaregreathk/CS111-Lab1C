// UCLA CS 111 Lab 1 command reading

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define DEBUG 0

// MARK: read input characters

/*
Read input characters using GET_NEXT_BYTE and GET_NEXT_BYTE_ARGUMENT, and return in a c string
*/
char *
read_chars(int (*get_next_byte) (void *),
         void *get_next_byte_argument)
{
  size_t buffer_index = 0;
  size_t buffer_size = 1024;
  char *char_buffer = checked_malloc(buffer_size);
  char next_char;

  // get all characters
  while ((next_char = get_next_byte(get_next_byte_argument)) && next_char != EOF) {
    char_buffer[buffer_index] = next_char;
    buffer_index++;

    if (buffer_index + 1 >= buffer_size) {
      char_buffer = checked_grow_alloc(char_buffer, &buffer_size);
    }
  }
  char_buffer[buffer_index] = '\0';

  return char_buffer;
}

// MARK: identify tokens

/*
Return 1 for a valid char of a simple token, or 0 otherwise
*/
int
simple_char (char a)
{
  switch (a) {
    case '!':
    case '%':
    case '+':
    case ',':
    case '-':
    case '.':
    case '/':
    case ':':
    case '@':
    case '^':
    case '_':
      return 1;
    default:
       break;
  }

  return isalnum(a);
}

/*
Identify tokens in c string and return a linked list of token structs representing CHAR_BUFFER
*/
token_t
make_tokens (char *char_buffer)
{
  token_t head = NULL;
  token_t current = NULL;

  int buffer_index = 0;
  int line = 1;
  char a;
  char b;
  token_type_t type;

  // process tokens
  while (char_buffer[buffer_index] != '\0') {
    // get token characters
    a = char_buffer[buffer_index];
    b = char_buffer[buffer_index + 1];

    // identify token
    switch(a) {
      case '&':
        if (b == '&') {
          type = AND_TOKEN;
          // skip next character
          buffer_index++;
        } else {
          type = UNKNOWN_TOKEN;
        }
        break;
      case ';':
        type = SEQUENCE_TOKEN;
        break;
      case '|':
        if (b == '|') {
          type = OR_TOKEN;
          // skip next character
          buffer_index++;
        } else {
          type = PIPE_TOKEN;
        }
        break;
      case '(':
        type = OPEN_PARENS_TOKEN;
        break;
      case ')':
        type = CLOSE_PARENS_TOKEN;
        break;
      case '<':
        type = INPUT_TOKEN;
        break;
      case '>':
        type = OUTPUT_TOKEN;
        break;
      case '\n':
        type = NEWLINE_TOKEN;
        line++;
        break;
      case '#':
        // check if comment is valid
        if (buffer_index == 0 || !simple_char(char_buffer[buffer_index - 1])) {
          while ((b = char_buffer[buffer_index]) && b != '\n' && b != '\0') {
            buffer_index++;
          }
          // don't bother pushing a comment token, just go to next token
          continue;
        } else {
          // invalid comment
          fprintf(stderr, "%i: comment must not be preceded by an ordinary token\n", line);
          exit(1);
        }
        break;
      case ' ':
      case '\t':
        buffer_index++;
        continue;
      default:
        type = UNKNOWN_TOKEN;
        break;
    }

    // handle simple word
    int word_length = 0;
    if (type == UNKNOWN_TOKEN) {
      while (simple_char(char_buffer[buffer_index + word_length])) {
        word_length++;
      }
      if (word_length > 0) {
        type = SIMPLE_TOKEN;
      }
    }

    // reject anything that couldn't be identified
    if (type == UNKNOWN_TOKEN) {
      fprintf(stderr, "%i: invalid token: %c\n", line, a);
      exit(1);
    }

    // build token
    token_t new_token = (token *)checked_malloc(sizeof(token));
    new_token->next = new_token->prev = NULL;
    new_token->word = NULL;
    new_token->type = type;
    new_token->line = line;
    if (type == SIMPLE_TOKEN) {
      new_token->word = (char *)checked_malloc(word_length*sizeof(char) + 1);
      int l;
      for (l = 0; l < word_length; l++) {
        new_token->word[l] = char_buffer[buffer_index + l];
      }
      new_token->word[word_length] = '\0';

      // don't reprocess word characters
      buffer_index += word_length-1;
    }
    if (head == NULL) {
      head = current = new_token;
    } else {
      current->next = new_token;
      new_token->prev = current;
      current = new_token;
    }

    buffer_index++;
  }

  return head;
}

/*
Output tokens in TOKENS, for debugging
*/
void
list_tokens (token_t tokens)
{
  printf("Available tokens:\n0: AND_TOKEN\n1: SEQUENCE_TOKEN\n2: OR_TOKEN\n3: PIPE_TOKEN\n4: SIMPLE_TOKEN\n5: OPEN_PARENS_TOKEN\n6: CLOSE_PARENS_TOKEN\n7: INPUT_TOKEN\n8: OUTPUT_TOKEN\n9: NEWLINE_TOKEN\n10: COMMENT_TOKEN\n11: UNKNOWN_TOKEN\n\n");
  printf("Parsed tokens:\n");
  while (tokens) {
    printf("%d", tokens->type);
    if (tokens->type == SIMPLE_TOKEN) {
      printf(" %s", tokens->word);
    }
    printf("\n");
    tokens = tokens->next;
  }
  printf("\n");
}

// MARK: check token syntax

// operator precedence
int
operator_precedence(token_t op)
{
  if (!op) {
    return -1;
  }

  switch (op->type) {
    case SEQUENCE_TOKEN:
      return 1;
    case AND_TOKEN:
    case OR_TOKEN:
      return 2;
    case PIPE_TOKEN:
      return 3;
    default:
      return -1;
  }
}

/*
Check for some syntax errors in a linked list of token structs TOKENS, exiting with an error if one is found
*/
void
check_token_syntax (token_t tokens)
{
  int line = 1;
  int open_parens = 0;
  int left_cmd = 0;
  int needs_right_cmd = 0;

  while (tokens) {
    // handle new lines
    if (tokens->type == NEWLINE_TOKEN) {
      if (needs_right_cmd || (tokens->next && tokens->next->type == CLOSE_PARENS_TOKEN)) {
        // ignore new line in the middle of an operator command or parens
        tokens->type = UNKNOWN_TOKEN;
        line++;
      } else if (tokens->next && tokens->next->type == NEWLINE_TOKEN) {
        // multiple new lines, just skip them
        while (tokens->next && tokens->next->type == NEWLINE_TOKEN) {
          tokens = tokens->next;
          line++;
        }
      } else if (tokens->next && left_cmd) {
        // one new line outide a command -> equivalent to a sequence token
        tokens->type = SEQUENCE_TOKEN;
        line++;
      }
    }

    switch (tokens->type) {
      case AND_TOKEN:
        if (!left_cmd) {
          fprintf(stderr, "%i: '&&' must come after a valid command\n", line);
          exit(1);
        }
        left_cmd = 0;
        needs_right_cmd = 1;
        break;
      case SEQUENCE_TOKEN:
        if (!left_cmd) {
          fprintf(stderr, "%i: ';' must come after a valid command\n", line);
          exit(1);
        }
        left_cmd = 0;
        break;
      case OR_TOKEN:
        if (!left_cmd) {
          fprintf(stderr, "%i: '||' must come after a valid command\n", line);
          exit(1);
        }
        left_cmd = 0;
        needs_right_cmd = 1;
        break;
      case PIPE_TOKEN:
        if (!left_cmd) {
          fprintf(stderr, "%i: '|' must come after a valid command\n", line);
          exit(1);
        }
        left_cmd = 0;
        needs_right_cmd = 1;
        break;
      case SIMPLE_TOKEN:
        left_cmd = 1;
        needs_right_cmd = 0;
        break;
      case OPEN_PARENS_TOKEN:
        open_parens++;
        left_cmd = 0;
        needs_right_cmd = 1;
        break;
      case CLOSE_PARENS_TOKEN:
        if (!left_cmd) {
          // check if separated from a valid left cmd only by newlines
          token_t clone = tokens->prev;
          while (clone) {
            if (clone->type == SIMPLE_TOKEN || clone->type == CLOSE_PARENS_TOKEN) {
              left_cmd = 1;
              break;
            } else if (operator_precedence(clone) > 0) {
              fprintf(stderr, "%i: ')' must come after a valid command\n", line);
              exit(1);
            }
            clone = clone->prev;
          }
        }
        if (!left_cmd) {
          fprintf(stderr, "%i: ')' must come after a valid command\n", line);
          exit(1);
        }
        open_parens--;
        left_cmd = 1;
        needs_right_cmd = 0;
        break;
      case INPUT_TOKEN:
        if (!left_cmd) {
          fprintf(stderr, "%i: '<' must come after a valid command\n", line);
          exit(1);
        }
        left_cmd = 0;
        break;
      case OUTPUT_TOKEN:
        if (!left_cmd) {
          fprintf(stderr, "%i: '>' must come after a valid command\n", line);
          exit(1);
        }
        left_cmd = 0;
        break;
      case NEWLINE_TOKEN:
        line++;
        left_cmd = 0;
        break;
      default:
        break;
    }

    if (open_parens < 0) {
      fprintf(stderr, "%i: too many closing parentheses\n", line);
      exit(1);
    }
    tokens = tokens->next;
  }

  if (open_parens != 0) {
    fprintf(stderr, "%i: not enough closing parentheses\n", line);
    exit(1);
  }
  if (needs_right_cmd) {
    fprintf(stderr, "%i: missing right-side command\n", line);
    exit(1);
  }
}

// MARK: identify commands

command_node_t
make_command()
{
  command_node_t cmd = (command_node *)checked_malloc(sizeof(command_node));
  cmd->prev = cmd->next = NULL;
  cmd->command = (command *)checked_malloc(sizeof(command));
  cmd->command->input = cmd->command->output = NULL;
  cmd->command->status = -1;
  return cmd;
}

command_node_t
apply_redirects(command_node_t cmd, token_t *next_token)
{ 
  token_t tokens = *next_token;
  while (tokens && (tokens->type == INPUT_TOKEN || tokens->type == OUTPUT_TOKEN))
  {
    if (tokens->type == INPUT_TOKEN) {
      // update command input
      if (cmd->command->output) {
        // invalid syntax, output already set (command > output < input)
        fprintf(stderr, "%i: command output must come after command input\n", tokens->line);
        exit(1);
      } else if (tokens->next && tokens->next->type == SIMPLE_TOKEN) {
        tokens = tokens->next;
        cmd->command->input = tokens->word;
      } else {
        fprintf(stderr, "%i: missing command input\n", tokens->line);
        exit(1);
      }
    } else if (tokens->type == OUTPUT_TOKEN) {
      // update command output
      if (tokens->next && tokens->next->type == SIMPLE_TOKEN) {
        tokens = tokens->next;
        cmd->command->output = tokens->word;
      } else {
        fprintf(stderr, "%i: missing command output\n", tokens->line);
        exit(1);
      }
    } 
    tokens = tokens->next;
  }
  *next_token = tokens;
  return cmd;
}

command_node_t
make_simple_command(token_t *command_start)
{
  token_t tokens = *command_start;
  command_node_t simple = make_command();
  simple->command->type = SIMPLE_COMMAND;
  char **word = NULL;
  char **wordPos = NULL;
  size_t words_length = 1;

  while (tokens && tokens->type == SIMPLE_TOKEN)
  {
    if (DEBUG) {
      printf("Found simple word: %s \n", tokens->word);
    }
    // update command words
    words_length += 1;
    if (word == NULL) {
      word = (char **)checked_malloc(words_length*sizeof(char*));
      wordPos = word;
    } else {
      word = (char **)checked_realloc(word, words_length*sizeof(char*));
    }

    *wordPos++ = tokens->word;
    tokens = tokens->next;
  }
  *wordPos = NULL;
  if (DEBUG) {
    printf("Created simple command: %s \n", *word);
  }
  simple->command->u.word = word;

  *command_start = tokens;
  return simple;
}

command_node_t
make_operator_command(token_t op, command_t first, command_t second)
{
  command_node_t new_cmd = make_command();
  command_type_t new_type = SEQUENCE_COMMAND;
  switch (op->type) {
    case AND_TOKEN:
      new_type = AND_COMMAND;
      break;
    case OR_TOKEN:
      new_type = OR_COMMAND;
      break;
    case PIPE_TOKEN:
      new_type = PIPE_COMMAND;
      break;
    case SEQUENCE_TOKEN:
      new_type = SEQUENCE_COMMAND;
      break;
    default:
      return NULL;
  }
  new_cmd->command->type = new_type;
  new_cmd->command->u.command[0] = first;
  new_cmd->command->u.command[1] = second;
  return new_cmd;
}

command_node_t
make_subshell_command(command_t cmd)
{
  if (!cmd) {
    return NULL;
  }

  command_node_t new_cmd = make_command();
  new_cmd->command->type = SUBSHELL_COMMAND;
  new_cmd->command->u.subshell_command = cmd;
  return new_cmd;
}

// command stack

void
push_cmd(command_node_t *head, command_node_t new_top)
{
  if (DEBUG) {
    printf("push cmd\n");
  }
  command_node_t top = *head;
  new_top->prev = NULL;
  if (top) {
    new_top->next = top;
    top->prev = new_top;
  } else {
    new_top->next = NULL;
  }
  *head = new_top;
}

command_t
pop_cmd(command_node_t *head)
{
  if (DEBUG) {
    printf("pop cmd\n");
  }
  command_node_t top = *head;
  if (!top) {
    return NULL;
  }

  command_node_t old_top = top;
  command_t cmd = old_top->command;
  if(top->next)
  {
    top = top->next;
    top->prev = NULL;
  }
  else {
    top = NULL;
  }
  free(old_top);
  *head = top;
  return cmd;
}

// operator stack

void
push_tok(token_t *head, token_t new_top)
{
  if (DEBUG) {
    printf("push tok\n");
  }
  token_t top = *head;

  // copy token to new node
  token_t new_token = (token *)checked_malloc(sizeof(token));
  new_token->word = NULL;
  new_token->type = new_top->type;
  new_token->line = new_top->line;
  new_token->prev = NULL;
  if (top) {
    new_token->next = top;
    top->prev = new_token;
  } else {
    new_token->next = NULL;
  }
  *head = new_token;
}

token_t
pop_tok(token_t *head)
{
  if (DEBUG) {
    printf("pop tok\n");
  }
  token_t top = *head;
  if(!top)
    return NULL;
  
  token_t tok = top;
  if(top->next) {
    top = top->next;
    top->prev = NULL;
  }
  else
    top = NULL;
  *head = top;
  return tok;
}

void
list_commands (command_node_t root)
{
  printf("Available commands:\n0: AND_COMMAND\n1: SEQUENCE_COMMAND\n2: OR_COMMAND\n3: PIPE_COMMAND\n4: SIMPLE_COMMAND\n5: SUBSHELL_COMMAND\n\n");
  printf("Parsed commands:\n");
  while (root) {
    printf("%d", root->command->type);
    if (root->command->type == SIMPLE_COMMAND) {
      char **w = root->command->u.word;
      while (*w) {
        printf(" %s", *w++);
      }
    }
    printf("\n");
    root = root->next;
  }
  printf("\n");
}

command_node_t
identify_commands(token_t tokens)
{
  command_node_t cmd_stack = NULL;
  token_t op_stack = NULL;

  while (tokens) {
    if (DEBUG) {
      printf("identifying token: %d \n", tokens->type);
    }

    // 1. if a simple command, push it onto cmd_stack
    if (tokens->type == SIMPLE_TOKEN) {
      command_node_t simple = make_simple_command(&tokens);
      apply_redirects(simple, &tokens);
      push_cmd(&cmd_stack, simple);
      // already skipped to next token
      continue;
    }

    // 2. if it's a "(", push it onto the operator stack
    else if (tokens->type == OPEN_PARENS_TOKEN) {
      push_tok(&op_stack, tokens);
    }
      
    // 3/4. if it's an operator
    else if (tokens->type == AND_TOKEN || tokens->type == SEQUENCE_TOKEN || tokens->type == OR_TOKEN || tokens->type == PIPE_TOKEN) {
      if (!op_stack) {
        // 3. if the operator stack is empty, push the operator onto the operator_stack
        push_tok(&op_stack ,tokens);
      } else {
        // 4. if the operator stack is NOT empty
        // a. pop all operators with greater or equal precedence off the operator_stack
        //    for each operator, pop 2 commands off the command stack
        //    combine into new command and push it onto the command stack
        while (operator_precedence(op_stack) >= operator_precedence(tokens)) {
          token_t op = pop_tok(&op_stack);
          command_t second = pop_cmd(&cmd_stack);
          command_t first = pop_cmd(&cmd_stack);
          command_node_t new_cmd = make_operator_command(op, first, second);
          push_cmd(&cmd_stack, new_cmd);
        }
        // b. stop when reach an operator with lower precedence or a "("
        // c. push new operator onto the operator stack
        push_tok(&op_stack, tokens);
      }
    }
   
    // 5. if it's a ")", pop operators off (similar to 4a.) until a matching "("
    else if (tokens->type == CLOSE_PARENS_TOKEN) {
      while (op_stack->type != OPEN_PARENS_TOKEN) {
        token_t op = pop_tok(&op_stack);
        command_t second = pop_cmd(&cmd_stack);
        command_t first = pop_cmd(&cmd_stack);
        command_node_t new_cmd = make_operator_command(op, first, second);
        push_cmd(&cmd_stack, new_cmd);
      }
      // create a subshell command by popping off top command on command stack
      pop_tok(&op_stack);
      command_t top_cmd = pop_cmd(&cmd_stack);
      command_node_t subshell_cmd = make_subshell_command(top_cmd);
      tokens = tokens->next;
      apply_redirects(subshell_cmd, &tokens);
      push_cmd(&cmd_stack, subshell_cmd);
      // already skipped to next token
      continue;
    }

    else if (tokens->type == NEWLINE_TOKEN) {
      while (op_stack) {
        token_t op = pop_tok(&op_stack);
        command_t second = pop_cmd(&cmd_stack);
        command_t first = pop_cmd(&cmd_stack);
        command_node_t new_cmd = make_operator_command(op, first, second);
        push_cmd(&cmd_stack, new_cmd);
      }
    }

    // 6. go back to 1
    tokens = tokens->next;
  }

  // cleanup: push all remaining operators
  while (op_stack) {
    token_t op = pop_tok(&op_stack);
    command_t second = pop_cmd(&cmd_stack);
    command_t first = pop_cmd(&cmd_stack);
    command_node_t new_cmd = make_operator_command(op, first, second);
    push_cmd(&cmd_stack, new_cmd);
  }

  if (DEBUG) {
    printf("\nop_stack and cmd_stack:\n\n");
    list_tokens(op_stack);
    list_commands(cmd_stack);
  }

  return cmd_stack;
}

// MARK: make_command_stream implementation

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
         void *get_next_byte_argument)
{
  char *char_buffer = read_chars(get_next_byte, get_next_byte_argument);
  token *tokens = make_tokens(char_buffer);

  if (!tokens) {
    return NULL;
  }
  
  if (DEBUG) {
    printf("before syntax check:\n\n");
    list_tokens(tokens);
  }
  check_token_syntax(tokens);
  if (DEBUG) {
    printf("after syntax check:\n\n");
    list_tokens(tokens);
  }
  command_node_t root = identify_commands(tokens);

  if (!root) {
    return NULL;
  }

  command_stream_t stream = (command_stream *)checked_malloc(sizeof(command_stream));

  // reverse order of the commands
  stream->tail = stream->head = stream->cursor = root;
  root = root->next;
  while (root) {
    command_node_t next_root = root->next;
    root->next = stream->head;
    stream->head->prev = root;
    root->prev = NULL;
    stream->head = stream->cursor = root;
    root = next_root;
  }
  stream->tail->next = NULL;
  return stream;
}

// MARK: read_command_stream implementation

command_t
read_command_stream (command_stream_t s)
{
  if (!s || !s->cursor) {
    return NULL;
  }
  command_t cmd = s->cursor->command;
  s->cursor = s->cursor->next;
  return cmd;
}
