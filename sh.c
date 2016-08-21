#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <dlfcn.h>
#include "stringlib.c"
//#include "jobs.c"

/* the max size of the buffer */
#define BUFSIZE 1024


/* handle command */
void handle_command(char *path, char *input, char *out_trunc, char *out_app, char **argv);
/* exec_command executes a command with the information given */
void exec_command(char *path, char *input, char *out_trunc, char *out_app, char **argv);
/* performs forking and creates a child process */
void run_exec(char *path, char *input, char *out_trunc, char *out_app, char **argv);
/* checks to see if there are too many redirects */
int too_many_redir(char **argv, int num_tokens);
/* parses the input for correct redirect information */
void parse_redirects(char **argv, int num_tokens);
/* shifts the arguments down to be in order */
int shift_args(char **argv, int hit, int num_tokens);
/* alters the argv array to be able to be executed */
void adjust_to_exec_array(char **argv, int num_tokens);
/* checks to see if the redirection is valid */
int check_redirect(int too_many, int no_redir_file, int redir_count, int num_tokens);
/* checks to see if the command is built in */
int check_built_in_commands(char *cmd, char *arg1, char *arg2);
/* checks to see if the command should be run in the background */
int is_background(char *cmd);
/* links files together */
void link_files(char *cmd, char *arg1, char *arg2);
/* removes a specified file */
void remove_file(char *cmd, char *arg1);
/* changes the directory shell is in */
void change_directory(char *cmd, char *arg1);
/* checks to see if the first argument has a slash */
int contains_slash(char *path);
/*gets the command the user puts in */
char *get_command(char *path);
/*breaks the input the user entered into an array of char* */
int parse_input(char **argv, char *pbuf);
/* checks the value returned by read and performs appropriate action */
void check_read(ssize_t r);
/* checks to see if the close function closed the file descriptor */
void check_close(int c);
/* checks to see if the dup function made a new file descriptor */ 
void check_dup(int d);
/* uses the write function and checks for errors and work with macros */
void prompt_user();

// global variables
job_list_t *curr_jobs;
/* main runs the prompting and calls the appropriate functions */
int main() {
	char buf[BUFSIZE];
	char* argv[BUFSIZE];
	curr_jobs = init_job_list();

	while(1) {
		memset(buf, '\0', BUFSIZE);
		memset(argv, '\0', BUFSIZE);
		prompt_user();
		ssize_t r = read(STDIN_FILENO, buf, BUFSIZE);
		check_read(r);
		buf[r] = '\0';
		char *buffer = trim_spaces(&buf[0]);
		if(strlen(buffer) > 3) {
		    int num_tokens = parse_input(argv, buffer);
		    parse_redirects(argv, num_tokens);
		    
		}		
	}
	

    return 0;
}

/* handle_command: takes in the proper information to deal with a command from the user
 *
 * Input:
 *        char *path       - is the path of the command 
 *        char *input      - is the input from the user
 *        char *out_trunc  - is the output in the case of truncating
 *        char *out_app    - is the output in the case of appending 
 *		  char  **argv     - command with the necessary arguments	
 *
 * Output:
 *        None
 */
void handle_command(char *path, char *input, char *out_trunc, char *out_app, char **argv) {
	if(check_built_in_commands(argv[0], argv[1], argv[2]) == 0)
		run_exec(path, input, out_trunc, out_app, argv);
}

/* exec_command: takes in the proper information to execute the command from the user
 *
 * Input:
 *        char *path       - is the path of the command 
 *        char *input      - is the input from the user
 *        char *out_trunc  - is the output in the case of truncating
 *        char *out_app    - is the output in the case of appending 
 *		  char  **argv     - command with the necessary arguments	
 *
 * Output:
 *        None
 */
void exec_command(char *path, char *input, char *out_trunc, char *out_app, char **argv) {
	int c_descriptor;
	if(strcmp(input, "")) {
		c_descriptor = open(input, O_RDONLY, 0444);
		check_dup(dup2(c_descriptor, STDIN_FILENO));
		check_close(close(c_descriptor));
	}
	if(strcmp(out_trunc, "")) {
		c_descriptor = open(out_trunc, O_CREAT | O_TRUNC | O_WRONLY, 0644);
		check_dup(dup2(c_descriptor, STDOUT_FILENO));
		check_close(close(c_descriptor));
	}
	if(strcmp(out_app, "")) {
		c_descriptor = open(out_app, O_CREAT | O_APPEND | O_WRONLY, 0644);
		check_dup(dup2(c_descriptor, STDOUT_FILENO));
		check_close(close(c_descriptor));
	}
	if(execv(path, argv) < 0)
		perror("Command not found.");
}

/* run_exec: takes creates a child process for the command to be executed
 *
 * Input:
 *        char *path       - is the path of the command 
 *        char *input      - is the input from the user
 *        char *out_trunc  - is the output in the case of truncating
 *        char *out_app    - is the output in the case of appending 
 *		  char  **argv     - command with the necessary arguments	
 *
 * Output:
 *        None
 */
void run_exec(char *path, char *input, char *out_trunc, char *out_app, char **argv) {
	pid_t child_pid = fork();
	int status;
	if(child_pid == -1) {
		perror("Forking failed.");
		cleanup_job_list(curr_jobs);
		exit(EXIT_FAILURE);
	} else if(child_pid == 0) {
		exec_command(path, input, out_trunc, out_app, argv);
		cleanup_job_list(curr_jobs);
		exit(EXIT_SUCCESS);
	} else {
		wait(&status);
	}
}

/* too_many_redir: determines if there are too many redirects on one line of input
 *
 * Input:
 *		  char  **argv   - command with the necessary arguments	
 *        int num_tokens - is the number of tokens
 *
 * Output:
 *        an int where 1 is there are to many redirects and zero otherwise
 */
int too_many_redir(char **argv, int num_tokens) {
	int in = 0;
	int out = 0;
	for(int i = 0; i < num_tokens; i++) {
		if(strcmp(argv[i], "<") == 0) 
			in++;
		if(strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0)
			out++;
	}
	if(in > 1 || out > 1)
		return 1;
	return 0;
}


/* parse_redirects: takes in the appropriate information to be able to parse the information
 *                 inputted by the user to get the appropriate command and associated information
 *
 * Input:
 *		  char  **argv   - command with the necessary arguments	
 *        int num_tokens - is the number of tokens
 *
 * Output:
 *        None
 */
void parse_redirects(char **argv, int num_tokens) {
	int redir_count = 0;
	int no_redir_file = 0;
	char path[128];
	char *input = "";
	char *out_trunc = "";
	char *out_app = "";
	int init_tokens = num_tokens;
	
	int too_many = too_many_redir(argv, num_tokens);
	if (!too_many) {
		for(int i = 0; i < num_tokens; i++) {
			if(strcmp(argv[i], "<") == 0) {
				redir_count++;
				if((i + 1) == num_tokens) {
					no_redir_file = 1;
					break;
				}
				input = argv[i + 1];
				num_tokens = shift_args(argv, i, num_tokens);
			}	
		}
		for(int i = 0; i < num_tokens; i++) {
			if(strcmp(argv[i], ">") == 0) {
				redir_count++;
				if((i + 1) == num_tokens) {
					no_redir_file = 1;
					break;
				}
				out_trunc = argv[i + 1];
				num_tokens = shift_args(argv, i, num_tokens);
			}	
		}
		for(int i = 0; i < num_tokens; i++) {
			if(strcmp(argv[i], ">>") == 0) {
				redir_count++;
				if((i + 1) == num_tokens) {
					no_redir_file = 1;
					break;
				}
				out_app = argv[i + 1];
				num_tokens = shift_args(argv, i, num_tokens);
			}
		}	
	}
	if(check_redirect(too_many, no_redir_file, redir_count, init_tokens))
		return;
	strcpy(path, argv[0]);
	adjust_to_exec_array(argv, num_tokens);
	handle_command(path, input, out_trunc, out_app, argv);
}



/* shift_args: shifts the argv array by two to remove null values placed during redirection parsing
 *
 * Input:
 *		  char  **argv   - command with the necessary arguments	
 *        int hit        - is the location of the redirect symbol in argv
 *        int num_tokens - is the number of tokens
 *
 * Output:
 *        None
 */
int shift_args(char **argv, int hit, int num_tokens) {
	argv[hit] = '\0';
	argv[hit + 1] = '\0';
	for(int j = hit + 2; j < num_tokens; j++) 
				argv[j - 2] = argv[j];
	return num_tokens - 2;

}

/* adjust_to_exec_array: alters the argv array by getting the commands from any paths put in
 *
 * Input:
 *		  char  **argv   - command with the necessary arguments	
 *        int num_tokens - is the number of tokens
 *
 * Output:
 *        None
 */
void adjust_to_exec_array(char **argv, int num_tokens) {
	for(int i = 0; i < num_tokens; i++) {
		if(argv[i] != NULL) {
			argv[0] = get_command(argv[0]);
		}
	}
	argv[num_tokens] = NULL;
}

/* check_redirect checks to see if the redirection was valid
 *
 * Input:
 *        char *redirect   - is the output file if there is one inputted 
 *		  char  **argv     - the tokenized array of strings
 *		  int recount      - is the count of redirects in the input
 *		  int num_tokens   - is the number of tokens that was originally parsed
 *		  int hit          - is the index one past where the redirect was found
 *
 * Output:
 *        an int: 1 where there is no valid redirection and zero if there is
 */
int check_redirect(int too_many, int no_redir_file, int redir_count, int num_tokens) {
	if(too_many) {
		perror("Can't have two input redirects on one line");
		return 1;
	}
	if(redir_count == 1 && no_redir_file == 1) {
		perror("No redirection file specified.");
		return 1;
	}
	if(redir_count == 1 && num_tokens < 3) {
		perror("No command.");
		return 1;
	}
	return 0;
}

/* check_built_in_commands: checks to see if the command is built in and if so it executes it
 *
 * Input:
 *        char *cmd        - is the command 
 *		  char *arg1     - is the first value to execute
 *		  char *arg2       - is the second value to execute if needed
 *
 * Output:
 *        an int: 1 if the command was built in and zero otherwise
 */
int check_built_in_commands(char *cmd, char *arg1, char *arg2) {
	if(strcmp(cmd, "exit") == 0) {
		cleanup_job_list(curr_jobs);
		exit(EXIT_SUCCESS);
	}
	if(strcmp(cmd, "cd") == 0) {
		change_directory(cmd, arg1);
		return 1;
	}
	if(strcmp(cmd, "ln") == 0) {
		link_files(cmd, arg1, arg2);
		return 1;
	}
	if(strcmp(cmd, "rm") == 0) {
		remove_file(cmd, arg1);
		return 1;
	}
	if(strcmp(cmd, "bg") == 0) {
		return 1;
	}
	if(strcmp(cmd, "fg") == 0) {
		return 1;
	}
	if(strcmp(cmd, "jobs") == 0) {
		return 1;
	}
	return 0;
}

int is_background(char *cmd) {
	if(strcmp(cmd, "&") == 0)
		return 1;
	return 0;
}

/* link_files: links several files together if there is valid input
 *
 * Input:
 *        char *cmd        - is the command 
 *		  char *arg1       - is the oldpath
 *		  char *arg2       - is the new path
 *
 * Output:
 *        None
 */
void link_files(char *cmd, char *arg1, char *arg2) {
	if(strcmp(arg1, "") == 0 || strcmp(arg2, "") == 0)
		printf(" %s: no such directory\n", cmd);
	else {
		if(link(arg1, arg2) == -1) {
			printf(" %s: linking not possible\n", cmd);
		}
			
	}
}

/* remove_file: removes the appropriate file if it exists
 *
 * Input:
 *        char *cmd        - is the command 
 *		  char *arg1      - is the file to remove
 *
 * Output:
 *        None
 */
void remove_file(char *cmd, char *arg1) {
	if(strcmp(arg1, "") == 0)
		printf(" %s: no such directory(1)\n", cmd);
	else {
		if(unlink(arg1) == -1) 
			printf(" %s: no such directory\n", cmd);
	}
}

/* change_directory: changes the directory of the user
 *
 * Input:
 *        char *cmd        - is the command 
 *		  char *arg1      - is the directory to go to
 *
 * Output:
 *        None
 */
void change_directory(char *cmd, char *arg1) {
	if(strcmp(arg1, "") == 0)
		printf(" %s: no such directory\n", cmd);
	else {
		if(chdir(arg1) == -1)
			printf(" %s: no such directory\n", cmd);
	}
}

/* contains_slash: checks to see if a string contains a slash
 *
 * Input:
 *        char *path        - is the string to check
 *
 * Output:
 *        an int: 1 if there is a slash and 0 otherwise
 */
int contains_slash(char *path) {
	size_t len = strlen(path);
	for(size_t i = 0; i < len; i++) {
		if(*path == '/'){
			return 1;
		}
		path++;
	}
	return 0;
}


/* get_command: gets the command from a path
 *
 * Input:
 *        char *path        - is the path to get the command from
 *
 * Output:
 *        a string that is the command if it is valid, otherwise it returns the inputted string
 */
char *get_command(char *path) {
	int is_slash = contains_slash(path);
	if(is_slash == 0)
		return path;
	return (strrchr(path, '/') + 1);
}

/* parse_input: tokenizes the input from the user
 *
 * Input:
 *        char **argv       - is the array of strings that will contain the tokenized input
 *        char *pbuf        - is the buffer to be tokenized
 *
 * Output:
 *        an int corresponding to the length of the tokenized array
 */
int parse_input(char **argv, char *pbuf) {
	char *delim = "\r\n\t\f\v ";
	return tokenize(pbuf, delim, argv);
}

/* check_read: checks to see if the reading was valid
 *
 * Input:
 *        ssize_t r         - is the length read in by the buffer
 *
 * Output:
 *        None
 */
void check_read(ssize_t r) {
	if(r == -1) 
		perror("Error: there was an error reading input.\n");
	if(r == 0) {
		printf("%s", "\n");
		cleanup_job_list(curr_jobs);
		exit(0);
	}
}

/* check_close: checks to see if the file descriptor closed properly
 *
 * Input:
 *        int c         - is the value returned by the close function 
 *
 * Output:
 *        None
 */
void check_close(int c) {
	if(c == -1) 
		perror("Error: there was an error closing a file descriptor.\n");
}

/* check_cup: checks to see if the dup function made a new file descriptor
 *
 * Input:
 *        int d         - is the value returned by the dup function 
 *
 * Output:
 *        None
 */
void check_dup(int d) {
	if(d == -1) 
		perror("Error: there was an error closing a file descriptor.\n");
}

/* prompt user is used to prompt the user in the event that 33sh is run as opposed to 33noprompt */
void prompt_user() {
	#ifdef PROMPT
		if(write(STDOUT_FILENO, "33sh> ", 6) < 0)
			perror("Error: there was an error writing in 33sh.\n");
	#endif
}








