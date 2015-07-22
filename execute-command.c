// UCLA CS 111 Lab 1 command execution

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 0

int
command_status (command_t c)
{
  return c->status;
}

/*
*  Set up file descriptors duplicating std_in or std_out as appropriate for a given command c
*/
void
handle_redirection(command_t c)
{
	// handle < input
	if (c->input) {
		int in_fd = open(c->input, O_RDONLY, 0644);
		if (in_fd < 0) {
			return;
		}
		dup2(in_fd, 0);
		close(in_fd);
	}

	// handle > output
	if (c->output) {
		int out_fd = open(c->output, O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if (out_fd < 0) {
			return;
		}
		dup2(out_fd, 1);
		close(out_fd);
	}
}

void
execute_simple(command_t c)
{
	pid_t p = fork();
	if (p == 0) {
		// child process
		handle_redirection(c);
		execvp(c->u.word[0], c->u.word);
		// if execvp returns, the command was invalid
    error(1, 0, "Invalid simple command\n");
	} else if (p > 0) {
		// parent process
		int status;
		waitpid(p, &status, 0);
		c->status = WEXITSTATUS(status);
	} else {
		// something went wrong with fork()
    error(1, 0, "Error forking process\n");
	}
}

void
execute_subshell(command_t c, bool time_travel)
{
  if (c->input && c->u.subshell_command->input == NULL) {
    c->u.subshell_command->input = c->input;
  }
  if (c->output && c->u.subshell_command->output == NULL) {
    c->u.subshell_command->output = c->output;
  }
	execute_command(c->u.subshell_command, time_travel);
	c->status = c->u.subshell_command->status;
}

void
execute_and(command_t c, bool time_travel)
{
	execute_command(c->u.command[0], time_travel);
	if (c->u.command[0]->status == 0) {
		execute_command(c->u.command[1], time_travel);
		c->status = c->u.command[1]->status;
	} else {
		c->status = c->u.command[0]->status;
	}
}

void
execute_or(command_t c, bool time_travel)
{
	execute_command(c->u.command[0], time_travel);
	if (c->u.command[0]->status != 0) {
		execute_command(c->u.command[1], time_travel);
		c->status = c->u.command[1]->status;
	} else {
		c->status = c->u.command[0]->status;
	}
}

void
execute_sequence(command_t c, bool time_travel)
{
  execute_command(c->u.command[0], time_travel);
  execute_command(c->u.command[1],time_travel);
  c->status = c->u.command[1]->status;
}

void
execute_pipe(command_t c, bool time_travel)
{
  int fd[2];
  pipe(fd);
  pid_t first_pid = fork();
  if(first_pid == 0) {
    // left command (will write data to pipe)
    close(fd[0]);   // close unused read end
    dup2(fd[1], 1);
    execute_command(c->u.command[0], time_travel);
    close(fd[1]);
    _exit(c->u.command[0]->status);
  } else if (first_pid > 0) {
    int status;
    waitpid(-1, &status, 0);
    status = WEXITSTATUS(status);
    if (status != 0) {
      // something went wrong
      error(1, 0, "Invalid pipe command\n");
    }
    pid_t second_pid = fork();
    if (second_pid == 0) {
      // right command (waiting for data from pipe)
      close(fd[1]);   // close unused write end
      dup2(fd[0],0);
      execute_command(c->u.command[1], time_travel);
      close(fd[0]);
      _exit(c->u.command[1]->status);
    } else if (second_pid > 0) {
    	// close unnused pipe
      close(fd[0]);
      close(fd[1]);
      // parent wait for the two children
      waitpid(-1, &status, 0);
      c->status = WEXITSTATUS(status);
    }
  } 
}

void
execute_command (command_t c, bool time_travel)
{
	char cmd_type = ' ';
	switch(c->type) {
    case SIMPLE_COMMAND:
    	execute_simple(c);
    	cmd_type = 's';
    	break;
    case SUBSHELL_COMMAND:
    	execute_subshell(c, time_travel);
    	cmd_type = '(';
    	break;
    case AND_COMMAND:
    	execute_and(c, time_travel);
    	cmd_type = 'a';
    	break;
    case OR_COMMAND:
    	execute_or(c, time_travel);
    	cmd_type = 'o';
    	break;
    case SEQUENCE_COMMAND:
    	execute_sequence(c, time_travel);
    	cmd_type = ';';
    	break;
    case PIPE_COMMAND:
    	execute_pipe(c, time_travel);
    	cmd_type = '|';
    	break;
		default:
			return;
	}
	if (DEBUG) {
		printf("%c cmd - exit status: %d\n", cmd_type, c->status);
	}
}




/**
*  Time Travel
**/

list_node *depend_head;

/**
*  Helper method to make debugging easier. Print information about a linked list
*  of graph_nodes CURRENT, including it's command and dependencies
**/
void
print_dependencies(graph_node *current)
{
  int node_count = 0;
  while (current) {
    node_count++;
    printf("graph_node %d\n", node_count);
    print_command(current->root);
    int before_idx = 0;
    for (before_idx = 0; before_idx < current->before_size; before_idx++) {
      graph_node *dependency = current->before[before_idx];
      printf("dependency #%d\n", before_idx+1);
      print_command(dependency->root);
    }
    printf("\n");
    current = current->next;
  }
}


/**
*  Mark a single dependency on the list_node LIST by adding a pointer (to char * WORD)
*  to LIST->write_list (if WRITE) or LIST->read_list (if not WRITE).
**/
void
mark_read_write(list_node *list, char *word, bool write)
{
    if(write)
    {
        list->write_size++;
        list->write_list = (char **)checked_realloc(list->write_list, list->write_size*sizeof(char*));
        //I'm a bit unsure when you said "adding a pointer (to char* WORD)" and I assumed you meant
        //adding the pointer "WORD" to the write_list
        list->write_list[list->write_size-1] = word;
    }
    else
    {
        list->read_size++;
        list->read_list = (char **)checked_realloc(list->read_list, list->read_size*sizeof(char*));
        list->read_list[list->read_size-1] = word;
    }
}


/**
*  Mark list_node LIST's read and write dependency lists by going through input / output and all words
*  in command COMMAND. 
**/
void
process_read_write(list_node *list, command_t command)
{
  if (command->input) {
    mark_read_write(list, command->input, false);
  }
  if (command->output) {
    mark_read_write(list, command->output, true);
  }

  if (command->type == SIMPLE_COMMAND) {
    if (command->u.word[0]) {
      int word_idx = 1;
      while (command->u.word[word_idx]) {
        mark_read_write(list, command->u.word[word_idx], false);
        word_idx++;
      }
    }
  } else if (command->type == SUBSHELL_COMMAND) {
    process_read_write(list, command->u.subshell_command);
  } else {
    // 2 subcommands
    process_read_write(list, command->u.command[0]);
    process_read_write(list, command->u.command[1]);
  }
}

/**
*  Check for dependencies on a list_node LIST by checking list_nodes which have already
*  been checked and processed. Will compare LIST.read_list and LIST.write_list to the read
*  and write lists of all list_nodes attached to global variable depend_head (which have already
*  been processed).
**/
void
check_dependencies(list_node *list)
{
  list_node *processed_list = depend_head;
  while (processed_list) {

  // Here I implemented the function if the node in question is not in the global linked list yet
  // (that's how I understand you use it)
    
  // NOTE HOW I THINK SOME OF MY CODE IS REDUNDANT BUT I CAN'T THINK OF HOW TO COMPARTMENTALIZE THEM

     
      //RAW dependency processed_list's WL contains list's RL
      // Since there's some compilation error with for, I just used while loop
      int i = 0;
      while(i < list->read_size)
      {
          //checks write list in this current node
          int j = 0;
          while(j < processed_list->write_size)
          {
              // Compares strings using strcmp as Tuan specified in Piazza
              // Note how I use the AND condition to check whether this last dependency is already accounted for
              // **I DON'T KNOW HOW TO MAKE THIS CHECKING PROCESS CLEANER**
              
              if(list->node->before_size > 0)
              {
                  if((strcmp(list->read_list[i], processed_list->write_list[j]) == 0) && list->node->before[list->node->before_size-1] != processed_list->node  ){
                      list->node->before_size++;
                      list->node->before = (graph_node **)checked_realloc(list->node->before, list->node->before_size*sizeof(graph_node*));
                      list->node->before[list->node->before_size-1] = processed_list->node;
                  }
              }
              j++;
          }
          i++;
      }
      
      i = 0;
      while(i < list->read_size)
      {
          //WAW dependency processed_list's WL contains list's WL
          //checks write list in this current node
          int j = 0;
          while(j < processed_list->write_size)
          {
            if(list->node->before_size > 0)
            {
                if((strcmp(list->write_list[i], processed_list->write_list[j]) == 0) && list->node->before[list->node->before_size-1] != processed_list->node){
                    list->node->before_size++;
                    list->node->before = (graph_node **)checked_realloc(list->node->before, list->node->before_size*sizeof(graph_node*));
                    list->node->before[list->node->before_size-1] = processed_list->node;
                }
            }
            j++;
          }
          
          //WAR dependency processed_list's RL contains list's WL
          //checks read list in this current node
          j = 0;
          while(j < processed_list->read_size)
          {
              if(list->node->before_size > 0)
              {
                  if((strcmp(list->write_list[i], processed_list->read_list[j]) == 0) && list->node->before[list->node->before_size-1] != processed_list->node){
                      list->node->before_size++;
                      list->node->before = (graph_node **)checked_realloc(list->node->before, list->node->before_size*sizeof(graph_node*));
                      list->node->before[list->node->before_size-1] = processed_list->node;
                  }
              }
              j++;
          }
          i++;
      }


    processed_list = processed_list->next;
  }
}

/**
*  Make a new graph_node and a container list_node to hold it, to
*  represent the command COMMAND. Also check for any dependencies on commands
*  which have already been processed.
**/
graph_node *
process_command(command_t command)
{
  // new graph node
  graph_node *node = (graph_node *)checked_malloc(sizeof(graph_node));
  node->root = command;
  node->next = node->prev = NULL;
  node->pid = -1;
  node->before = NULL;
  node->before_size = 0;

  // new list node
  list_node *list = (list_node *)checked_malloc(sizeof(list_node));
  list->node = node;
  list->write_list = list->read_list = NULL;
  list->write_size = list->read_size = 0;
  list->next = list->prev = NULL;
  process_read_write(list, command);
  check_dependencies(list);

  // push to global dependencies linked list
  if (depend_head) {
    list->next = depend_head;
    depend_head->prev = list;
  }
  depend_head = list;

  return node;
}

/**
*  Create a dependency_graph representing a command_stream STREAM which finds any dependencies
*  between different commands so that they can be run in parallel without any side effects.
**/
dependency_graph_t
create_graph(command_stream_t stream)
{
  dependency_graph_t graph = (dependency_graph *)checked_malloc(sizeof(dependency_graph));
  graph->no_dependencies = graph->dependencies = NULL;
  stream->cursor = stream->head;
  // process commands
  while (stream->cursor)
  {
    graph_node *new_node = process_command(stream->cursor->command);
    if (new_node->before == NULL) {
      // no dependencies
      if (graph->no_dependencies) {
        graph_node *old_node = graph->no_dependencies;
        while (old_node->next) {
          old_node = old_node->next;
        }
        new_node->prev = old_node;
        old_node->next = new_node;
      } else {
        graph->no_dependencies = new_node;
      }
    } else {
      // dependencies
      if (graph->dependencies) {
        graph_node *old_node = graph->dependencies;
        while (old_node->next) {
          old_node = old_node->next;
        }
        new_node->prev = old_node;
        old_node->next = new_node;
      } else {
        graph->dependencies = new_node;
      }
    }
    stream->cursor = stream->cursor->next;
  }

  if (DEBUG) {
    printf("created dependency graph\n\n");
    printf("\nno dependencies\n\n");
    print_dependencies(graph->no_dependencies);
    printf("\ndependencies\n\n");
    print_dependencies(graph->dependencies);
  }

  return graph;
}

/**
*  Execute a linked list of graph_nodes CURRENT representing commands to be executed.
**/
int
execute_no_dependencies(graph_node *current)
{
  int status = -1;
  while (current) {
    pid_t pid = fork();
    if (pid == 0) {
      // child process
      execute_command(current->root, true);
      _exit(current->root->status);
    } else {
      // parent process
      current->pid = pid;
      waitpid(-1, &status, 0);
      if (DEBUG) {
        printf("exit status: %d\n", status);
      }
    }

    current = current->next;
  }
  return status;
}

/**
*  Execute a linked list of graph_nodes CURRENT representing commands to be executed, but only
*  run each command after all of its dependencies have run, so there's no race conditions. 
**/
int
execute_dependencies(graph_node *current)
{
  int status = -1;
  while (current) {
    // wait for dependencies to finish first
    int before_idx = 0;
    for (before_idx = 0; before_idx < current->before_size; before_idx++) {
      graph_node *dependency = current->before[before_idx];
      waitpid(dependency->pid, &status, 0);
    }

    pid_t pid = fork();
    if (pid == 0) {
      // child process
      execute_command(current->root, true);
      _exit(current->root->status);
    } else {
      // parent process
      current->pid = pid;
      waitpid(-1, &status, 0);
      if (DEBUG) {
        printf("exit status: %d\n", status);
      }
    }

    current = current->next;
  }
  return status;
}

/**
*  Execute the graph by running commands which have dependencies after their dependencies run.
*  Execute the dependency graph without race conditions.
**/
int
execute_graph(dependency_graph_t graph)
{
  int final_status = execute_no_dependencies(graph->no_dependencies);
  if (graph->dependencies) {
    final_status = execute_dependencies(graph->dependencies);
  }
  return final_status;
}

