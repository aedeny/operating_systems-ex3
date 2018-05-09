// Eden Yefet
// 204778294

#include <zconf.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <memory.h>
#include <stdio.h>
#include <sys/wait.h>

#define TRUE 1
#define FALSE 0
#define ERROR_MSG "Error in system call"

#define COMPILED_FILE_NAME "current.out"
#define OUTPUT_FILE_NAME "out.txt"
#define COMPARATOR "comp.out"
#define RESULTS_FILE_NAME "results.csv"

#define TIMEOUT_SEC 5
#define NEW_LINE_DELIM '\n'

#define NO_C_FILE "NO_C_FILE"
#define COMPILATION_ERROR "COMPILATION_ERROR"
#define TIMEOUT "TIMEOUT"
#define BAD_OUTPUT "BAD_OUTPUT"
#define SIMILAR_OUTPUT "SIMILAR_OUTPUT"
#define GREAT_JOB "GREAT_JOB"

typedef enum OutputResult {
  DIFFERENT = 1,
  SIMILAR,
  IDENTICAL
} OutputResult;

typedef struct {
  char input_directory[PATH_MAX];
  char input_file[PATH_MAX];
  char correct_output_file[PATH_MAX];
} ConfigFile;

/**
 * Copies a line of text up to a delimiter.
 * @param src A pointer to the beginning of text.
 * @param dst A buffer to write to.
 * @param delimiter Delimiter representing end of line.
 * @return The number of written characters.
 */
int copy_line(const char *src, char *dst, char delimiter) {
  int i = 0;
  while (src[i] != delimiter) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
  return i + 1;
}

/**
 * Checks whether a file is a C file.
 * @param file_path The file path.
 * @return Returns 1 if the specified file is a C file, 0 otherwise.
 */
int is_c_file(char *file_path) {
  char *dot;
  if ((dot = strrchr(file_path, '.')) != NULL && strcmp(dot, ".c") == 0) {
    return TRUE;
  }
  return FALSE;
}

/**
 * Reads the configuration file.
 * @param config_file_path The path to the configuration file.
 * @param config_file A pointer to a ConfigFile structure which will be
 * updated by the config file.
 * @return Returns 1 upon success, 0 otherwise.
 */
int read_config_file(char *config_file_path, ConfigFile *config_file) {
  ssize_t ret1;
  char buffer[3 * PATH_MAX];
  int i;

  // Reads file to buffer
  int fd = open(config_file_path, O_RDONLY);
  if (fd == -1) {
    write(STDERR_FILENO, ERROR_MSG, sizeof(ERROR_MSG) - 1);
    return FALSE;
  }
  ret1 = read(fd, buffer, sizeof(buffer));
  if (ret1 == -1) {
    write(STDERR_FILENO, ERROR_MSG, sizeof(ERROR_MSG) - 1);
    return FALSE;
  }
  close(fd);

  // Parses buffer
  i = copy_line(buffer, config_file->input_directory, NEW_LINE_DELIM);
  i += copy_line(&(buffer[i]), config_file->input_file, NEW_LINE_DELIM);
  copy_line(&(buffer[i]), config_file->correct_output_file, NEW_LINE_DELIM);
  return TRUE;
}

/**
 * Recursively searches for a C file in specified path.
 * @param dp The DIR* corresponding to the path parameter.
 * @param path The path to the directory to search in.
 * @param c_file_path A char* buffer to update with the found C file path.
 * If a C file was not found, this buffer will be updated to be an empty array.
 * @return Returns 1 if a C file was found, 0 otherwise.
 */
int find_c_file(DIR *dp, char *path, char *c_file_path) {
  struct dirent *dirent = NULL;
  DIR *sub_dir = NULL;
  char path_buffer[PATH_MAX];

  while ((dirent = readdir(dp)) != NULL) {
    // If file
    if (dirent->d_type == DT_REG && is_c_file(dirent->d_name)) {
      strcpy(path_buffer, path);
      strcat(path_buffer, "/");
      strcat(path_buffer, dirent->d_name);
      strcpy(c_file_path, path_buffer);
      return 1;
    }
    // If directory
    if (dirent->d_type == DT_DIR && strcmp(dirent->d_name, ".") != 0 &&
        strcmp(dirent->d_name, "..") != 0) {

      strcpy(path_buffer, path);
      strcat(path_buffer, "/");
      strcat(path_buffer, dirent->d_name);

      sub_dir = opendir(path_buffer);
      find_c_file(sub_dir, path_buffer, c_file_path);
    }
  }
  // An empty string if no C file was found.
  strcpy(c_file_path, "");
  return 0;
}

/**
 * Compiles a C file using the gcc compiler.
 * @param path A path to the C file to be compiled.
 * @param output_file_name A path to the output file.
 * @return Returns 1 on success, 0 otherwise.
 */
int compile_c_file(char *path, char *output_file_name) {
  int pid = fork();
  if (pid == 0) {
    char *argv[] = {"gcc", "-o", output_file_name, path, NULL};
    execvp("gcc", argv);
    exit(-1);
  } else {
    int status;
    int r = 1;
    if (waitpid(pid, &status, 0) != -1 && WIFEXITED(status)) {
      r = WEXITSTATUS(status);
    }
    return r == 0;
  }
}

/**
 * Runs an executable file with an input file.
 * @param file_path The file path to execute.
 * @param input_file_path The input file for the execution.
 * @return Returns 1 on success, 0 otherwise.
 */
int run_file(char *file_path, char *input_file_path) {
  char buffer[PATH_MAX] = "";
  int pid;
  int in;
  int out;

  strcat(buffer, "./");
  strcat(buffer, file_path);
  in = open(input_file_path, O_RDONLY);
  if (in == -1) {
    write(STDERR_FILENO, ERROR_MSG, sizeof(ERROR_MSG) - 1);
    return FALSE;
  }
  out = open(OUTPUT_FILE_NAME, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP |
      S_IWGRP | S_IWUSR);
  if (out == -1) {
    write(STDERR_FILENO, ERROR_MSG, sizeof(ERROR_MSG) - 1);
    return FALSE;
  }
  dup2(in, 0);
  dup2(out, 1);
  close(out);
  close(in);

  pid = fork();
  if (pid == 0) {
    execl(buffer, buffer, NULL);
  } else {
    int status = 0;
    int i;
    for (i = 0; i < TIMEOUT_SEC; i++) {
      sleep(1);
      if (waitpid(pid, &status, WNOHANG) != 0) {
        break;
      }
    }
    return i < TIMEOUT_SEC;
  }
}

/**
 * Compares two ASCII text files using a comparator executable.
 * @param file1 A path to the first file.
 * @param file2 A path to the second file.
 * @param comparator A path to a comparator executable.
 * @return Returns the return value of the comparator running on file1 and
 * file2.
 */
int compare_output(char *file1, char *file2, char *
comparator) {
  int pid = fork();
  if (pid == 0) {
    execl(COMPARATOR, COMPARATOR, file1, file2, NULL);
  } else {
    int status = 0;
    waitpid(pid, &status, 0);

    return WEXITSTATUS(status);
  }
}

void add_results_entry(int fd, char *username, int grade, char *reason) {
  char buffer[PATH_MAX];
  char grade_str[5];
  sprintf(grade_str, "%d", grade);
  strcpy(buffer, username);
  strcat(buffer, ",");
  strcat(buffer, grade_str);
  strcat(buffer, ",");
  strcat(buffer, reason);
  strcat(buffer, "\n");

  write(fd, buffer, strlen(buffer));
}

int main(int argc, char **argv) {
  ConfigFile config_file;
  DIR *p_dir;
  DIR *p_current_dir;
  char c_file_path[PATH_MAX];

  // Checks number of arguments
  if (argc != 2) {
    return -1;
  }

  // Reads config file
  read_config_file(argv[1], &config_file);
  if ((p_dir = opendir(config_file.input_directory)) == NULL) {
    return -1;
  }

  // Iterates sub_directories
  struct dirent *dirent = NULL;
  int results = open(RESULTS_FILE_NAME, O_WRONLY | O_TRUNC | O_APPEND | O_CREAT,
                     S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);

  char current_dir[PATH_MAX];
  while ((dirent = readdir(p_dir)) != NULL) {
    if (dirent->d_type == DT_DIR && strcmp(dirent->d_name, ".") != 0 &&
        strcmp(dirent->d_name, "..") != 0) {
      strcpy(current_dir, config_file.input_directory);
      strcat(current_dir, "/");
      strcat(current_dir, dirent->d_name);

      if ((p_current_dir = opendir(current_dir)) == NULL) {
        return -1;
      }

      if (!find_c_file(p_current_dir, current_dir, c_file_path)) {
        add_results_entry(results, dirent->d_name, 0, NO_C_FILE);
      } else if (!compile_c_file(c_file_path, COMPILED_FILE_NAME)) {
        add_results_entry(results, dirent->d_name, 0, COMPILATION_ERROR);
      } else if (!run_file(COMPILED_FILE_NAME, config_file.input_file)) {
        add_results_entry(results, dirent->d_name, 0, TIMEOUT);
      } else {
        int output = compare_output(OUTPUT_FILE_NAME, config_file
            .correct_output_file);

        switch (output) {
          case IDENTICAL:
            add_results_entry(results, dirent->d_name, 100, GREAT_JOB);
            break;
          case SIMILAR:
            add_results_entry(results, dirent->d_name, 80, SIMILAR_OUTPUT);
            break;
          case DIFFERENT:
            add_results_entry(results, dirent->d_name, 60, BAD_OUTPUT);
            break;
          default:
            printf("Error");
        }
      }
      closedir(p_current_dir);
    }
  }

  close(results);
  remove(COMPILED_FILE_NAME);
  remove(OUTPUT_FILE_NAME);
  printf("Finished");
  return 0;
}