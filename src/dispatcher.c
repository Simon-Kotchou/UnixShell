#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "dispatcher.h"
#include "shell_builtins.h"
#include "parser.h"

/**
 * dispatch_external_command() - run a pipeline of commands
 *
 * @pipeline:   A "struct command" pointer representing one or more
 *              commands chained together in a pipeline.  See the
 *              documentation in parser.h for the layout of this data
 *              structure.  It is also recommended that you use the
 *              "parseview" demo program included in this project to
 *              observe the layout of this structure for a variety of
 *              inputs.
 *
 * Note: this function should not return until all commands in the
 * pipeline have completed their execution.
 *
 * Return: The return status of the last command executed in the
 * pipeline.
 */
static int dispatch_external_command(struct command *pipeline)
{
	/*
	 * Note: this is where you'll start implementing the project.
	 *
	 * It's the only function with a "TODO".  However, if you try
	 * and squeeze your entire external command logic into a
	 * single routine with no helper functions, you'll quickly
	 * find your code becomes sloppy and unmaintainable.
	 *
	 * It's up to *you* to structure your software cleanly.  Write
	 * plenty of helper functions, and even start making yourself
	 * new files if you need.
	 *
	 * For D1: you only need to support running a single command
	 * (not a chain of commands in a pipeline), with no input or
	 * output files (output to stdout only).  In other words, you
	 * may live with the assumption that the "input_file" field in
	 * the pipeline struct you are given is NULL, and that
	 * "output_type" will always be COMMAND_OUTPUT_STDOUT.
	 *
	 * For D2: you'll extend this function to support input and
	 * output files, as well as pipeline functionality.
	 *
	 * Good luck!
	 */		
	int fd_table[2];
	int in = STDIN_FILENO;
	pid_t pid;
	int status = 0;

	while(pipeline->output_type == COMMAND_OUTPUT_PIPE){
		if(pipeline->input_filename != NULL){
			fd_table[0] = open(pipeline->input_filename, O_RDONLY, 0644);
			dup2(fd_table[0], STDIN_FILENO);
			close(fd_table[0]);
		}
	
		if(pipe(fd_table) == -1){
			perror("Pipe error");
			return -1;
		}
		
		if((pid = fork()) == 0) {
			if(in != STDIN_FILENO){
				dup2(in, STDIN_FILENO);
				close(in);
			}

			dup2(fd_table[1], STDIN_FILENO);
			close(fd_table[1]);
		
			if (execvp(pipeline->argv[0],(char **)pipeline->argv) == 1) {
				perror("Error running execvp");
				return -1;
			}
		}
		else if(pid != -1) {
			close(in);
			close(fd_table[1]);
			in = fd_table[0];
			
			while((waitpid(pid, &status, 0)) == -1) {
			
			}
			if(!WIFEXITED(status)) {
				perror("Child status error");
				return -1;
			}
		}
		else{
			perror("Fork error");
			return -1;
		}
		pipeline = pipeline->pipe_to;												
	}

	if(pipeline->output_type == COMMAND_OUTPUT_FILE_TRUNCATE){
		fd_table[1] = open(pipeline->output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		dup2(fd_table[1], STDOUT_FILENO);
		close(fd_table[1]);
	}
	else if(pipeline->output_type == COMMAND_OUTPUT_FILE_APPEND){
		fd_table[1] = open(pipeline->output_filename, O_WRONLY | O_CREAT | O_APPEND);
		dup2(fd_table[1], STDOUT_FILENO);
		close(fd_table[1]);
	}
	if(pipeline->input_filename != NULL && pipeline->output_type == COMMAND_OUTPUT_STDOUT){
		fd_table[0] = open(pipeline->input_filename, O_RDONLY, 0644);
		dup2(fd_table[0], STDIN_FILENO);
		close(fd_table[0]);
	}
	else if(in != STDIN_FILENO){
		dup2(in,STDIN_FILENO);
		close(in);
	}
	if((pid = fork()) == 0) {
		if(execvp(pipeline->argv[0], (char **)pipeline->argv)== -1){
			perror("Error running execvp");
			return -1;
		}
	}
	else if(pid != -1){
		while((waitpid(pid, &status, 0)) == -1){
		
		}
		if(!WIFEXITED(status)){
			perror("Child status error");
			return -1;
		}
	}
	else{
		perror("Fork error");
		return -1;
	}

	if( in != STDIN_FILENO) {
		dup2(in,STDIN_FILENO);
		close(in);						
	}
	if(fd_table[1] != STDOUT_FILENO){
		dup2(fd_table[1], STDOUT_FILENO);
		close(fd_table[1]);
	}	
	return status;
}
				
/**
 * dispatch_parsed_command() - run a command after it has been parsed
 *
 * @cmd:                The parsed command.
 * @last_rv:            The return code of the previously executed
 *                      command.
 * @shell_should_exit:  Output parameter which is set to true when the
 *                      shell is intended to exit.
 *
 * Return: the return status of the command.
 */
static int dispatch_parsed_command(struct command *cmd, int last_rv,
				   bool *shell_should_exit)
{
	/* First, try to see if it's a builtin. */
	for (size_t i = 0; builtin_commands[i].name; i++) {
		if (!strcmp(builtin_commands[i].name, cmd->argv[0])) {
			/* We found a match!  Run it. */
			return builtin_commands[i].handler(
				(const char *const *)cmd->argv, last_rv,
				shell_should_exit);
		}
	}

	/* Otherwise, it's an external command. */
	return dispatch_external_command(cmd);
}

int shell_command_dispatcher(const char *input, int last_rv,
			     bool *shell_should_exit)
{
	int rv;
	struct command *parse_result;
	enum parse_error parse_error = parse_input(input, &parse_result);

	if (parse_error) {
		fprintf(stderr, "Input parse error: %s\n",
			parse_error_str[parse_error]);
		return -1;
	}

	/* Empty line */
	if (!parse_result)
		return last_rv;

	rv = dispatch_parsed_command(parse_result, last_rv, shell_should_exit);
	free_parse_result(parse_result);
	return rv;
}
