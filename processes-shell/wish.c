#define _GNU_SOURCE
#define CMD_CNT 256
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
void error_message(void) {
  char error_message[30] = "An error has occurred\n";
  write(STDERR_FILENO, error_message, strlen(error_message));
}
char *get_substring(char *start, char *end) {
  int n = 0;
  for (char *i = start; i < end; i++) {
    n++;
  }
  char *ret = (char *)malloc(n * sizeof(char));
  strncpy(ret, start, n);
  return ret;
}
void reorganize_the_token_by(char *my_tokens[256], int tokens_cnt) {
  char *tmp[256];
  int tmp_i = 0;

  for (int i = 0; i < tokens_cnt; i++) {
    char *prev_delimiterpos = my_tokens[i];
    while (1) {
      char *delimiterpos = strchr(prev_delimiterpos, '&');
      if (delimiterpos != NULL) {
        if (strcmp(prev_delimiterpos, delimiterpos) == 0) {
          tmp[tmp_i++] = strdup("&"); // add "&"
          break;
        }
        tmp[tmp_i++] = strdup(get_substring(prev_delimiterpos, delimiterpos));
        prev_delimiterpos = delimiterpos + 1; // 更新字符串查找范围(去掉分隔符)
        tmp[tmp_i++] = strdup("&");           // add "&"
      } else {
        if (prev_delimiterpos != NULL) {
          tmp[tmp_i++] = strdup(prev_delimiterpos);
        }
        break;
      }
    }
  }

  // write back
  for (int i = 0; i < tmp_i; i++) {
    my_tokens[i] = strdup(tmp[i]);
  }
  my_tokens[tmp_i] = NULL;
}
/*command's infomation:command as an object*/
typedef struct {
  char **argv[CMD_CNT];       // 命令及其参数(NULL结尾)
  char *output_file[CMD_CNT]; //(default:NULL)
  int cmd_cnt;                // char **数组的cmd个数
} command_t;
void parse_command(command_t *cmd, char **tokens) {
  for (int i = 0; i < CMD_CNT; i++) {
    cmd->output_file[i] = NULL;
  }
  cmd->cmd_cnt = 0;
  int argc = 0;
  while (tokens[argc] != NULL) {
    argc++;
  }
  // malloc for argv
  cmd->argv[cmd->cmd_cnt] = (char **)malloc(sizeof(char *) * (argc + 1));
  int j = 0;
  for (int i = 0; i < argc; i++) {
    if (strcmp(tokens[i], ">") == 0) {
      if (tokens[i + 1] == NULL) { // no parament in redirection
        error_message();
        exit(0);
      } else if (tokens[i + 2] != NULL &&
                 strcmp(tokens[i + 2], "&") !=
                     0) { // too many parament in redirection(except &)
        error_message();
        exit(0);
      }
      cmd->output_file[cmd->cmd_cnt] = strdup(tokens[i + 1]);
      i++;
    } else if (strcmp(tokens[i], "&") == 0) { // 处理'&':假设出现&和出现'>'互斥
      cmd->argv[cmd->cmd_cnt][j] = NULL;
      (cmd->cmd_cnt)++;
      j = 0;
      cmd->argv[cmd->cmd_cnt] = (char **)malloc(sizeof(char *) * (argc + 1));
    } else {
      cmd->argv[cmd->cmd_cnt][j++] = strdup(tokens[i]);
    }
  }
  cmd->argv[cmd->cmd_cnt][j] =
      NULL; // terminated with NULL(capable with the exec()family)
}
void setup_output_redirection(char *filename) {
  // 打开文件:只写,创建,覆盖
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    perror("open");
    exit(1); // error cannot open the file
  }
  // stdout重定向到文件
  if (dup2(fd, STDOUT_FILENO) < 0) {
    exit(1); // redirection:stdout failed
  }
  if (dup2(fd, STDERR_FILENO) < 0) {
    exit(1); // redirection:stderr failed
  }
  close(fd);
}
int main(int argc, char *argv[]) { // read-->phase-->exacute
  FILE *fd;
  if (argc == 1) { // interactive mode
    fd = stdin;
  } else if (argc == 2) { // batch mode
    fd = fopen(argv[1], "r");
    if (fd == NULL) {
      error_message();
      exit(1);
    }
  } else {
    error_message();
    exit(1);
  }
  command_t cmd;
  char *line = NULL;   // 给getline()传入NULL让他自己malloc
  char *line_for_free; // 给free用的line
  size_t len = 0;      // getline update its' malloc size
  ssize_t nread;       // 接收getline()的return value
  char *token_tmp;
  char *tokens[256]; // strsep()需要的
  for (int i = 0; i < 256; i++) {
    tokens[i] = NULL;
  }
  char *tokens_tmp[256];      // 处理tokens的中间缓存
  char *path[256] = {"/bin"}; // 设置一个path(<==>PATH)
  char fullpath[256] = {""};  // 完整路径名
  int path_cnt = 1;           // path数组的元素个数
  while (1) {
    if (argc == 1) {
      printf("%s", "wish> "); // prompt
    }

    // get the token
    if ((nread = getline(&line, &len, fd)) == -1) {
      break;
    }
    line_for_free = line;
    line[nread - 1] = '\0'; // 去掉lineptr中的换行符(覆盖)

    // get the tokens
    int i = 0; // token数组的index
    while ((token_tmp = strsep(&line, " ")) != NULL) {
      if (strcmp(token_tmp, "") == 0) {
        continue;
      }
      tokens[i++] = strdup(token_tmp);
    }
    tokens[i] = NULL;
    // parse the '>':from"aaaa>bbbb" to "aaaa" ">" "bbbb"
    int i_tokens_tmp = 0;
    for (int i_wra = 0; i_wra < i; i_wra++) {
      int gt_flag = 0;
      for (int j = 0; tokens[i_wra][j] != '\0';
           j++) { // 解析tokens[i_wra](有没有">")
        if (tokens[i_wra][j] == '>') {
          gt_flag = 1;
          break;
        }
      }
      if (gt_flag == 1) { // 处理分割解析有">"的情况
        char *prev_delimiterpos = tokens[i_wra];
        while (1) {
          char *delimiterpos = strchr(prev_delimiterpos, '>');
          if (delimiterpos != NULL) {
            if (strcmp(prev_delimiterpos, delimiterpos) == 0) {
              tokens_tmp[i_tokens_tmp++] = strdup(">"); // add '>'
              break;
            }
            tokens_tmp[i_tokens_tmp++] =
                strdup(get_substring(prev_delimiterpos, delimiterpos));
            // printf("get_substring=%s\n", // test it
            //        get_substring(prev_delimiterpos, delimiterpos));
            prev_delimiterpos =
                delimiterpos + 1; // 更新字符串查找范围(去掉分隔符)
            tokens_tmp[i_tokens_tmp++] = strdup(">"); // add '>'
          } else {
            if (prev_delimiterpos != NULL) {
              tokens_tmp[i_tokens_tmp++] = strdup(prev_delimiterpos);
            }
            break;
          }
        }
      } else if (gt_flag ==
                 0) { // 解析tokens[i_wra]完成(有没有">"号:确定无),写入tmp
        tokens_tmp[i_tokens_tmp++] = strdup(tokens[i_wra]);
      }
    }
    // 把获得的tokens_tmp的结果数组写回tokens数组里面
    int i_tokens_tmp_wra = 0;
    for (i_tokens_tmp_wra = 0; i_tokens_tmp_wra < i_tokens_tmp;
         i_tokens_tmp_wra++) {
      tokens[i_tokens_tmp_wra] = strdup(tokens_tmp[i_tokens_tmp_wra]);
    }
    tokens[i_tokens_tmp_wra] = NULL;

    // parse the token '&' from the tokens
    reorganize_the_token_by(tokens, i_tokens_tmp);

    // parse_command and get the cmd struct
    parse_command(&cmd, tokens);

    // execute:one by one
    int pid_cnt = 0;
    int pid[256];
    for (int cmd_cnt_wra = 0; cmd_cnt_wra <= cmd.cmd_cnt; cmd_cnt_wra++) {
      // if no "target" in redirection mode,then throw an error
      if (cmd.output_file[cmd_cnt_wra] != NULL &&
          cmd.argv[cmd_cnt_wra][0] == NULL) {
        error_message();
        continue;
      } else if (cmd.argv[cmd_cnt_wra][0] == NULL) {
        continue;
      }
      // int pid_cnt = 0;
      // int pid[256];
      if (strcmp("exit", cmd.argv[cmd_cnt_wra][0]) == 0) {
        if (cmd.argv[cmd_cnt_wra][1] == NULL) {
          exit(0);
        } else { // exit cannot have an parament
          error_message();
        }
      } else if (strcmp("cd", cmd.argv[cmd_cnt_wra][0]) == 0) {
        if (i != 2) {
          error_message();
          // printf("chdir:para_errors\n"); // test:error:cd paras errors
        }
        if (chdir(cmd.argv[cmd_cnt_wra][1]) != 0) {
          // printf("chdir:cd_path:failed\n"); // test:error:fullpath print
          // out
        } else {
          // printf("chdir:cd_path:%s\n",
          // cmd.argv[cmd_cnt_war][1]); // test:fullpath print out
        }
      } else if (strcmp("path", cmd.argv[cmd_cnt_wra][0]) == 0) {
        if (i == 1) {
          // printf("builtin:path:no input\n"); // do nothing but clean the path
          path_cnt = 0;
        } else if (i >= 2) {
          path_cnt = i - 1; // update the path_cnt
          for (int j = 1; j < i; j++) {
            path[j - 1] = strdup(cmd.argv[cmd_cnt_wra][j]);
          }
        }
      } else { // we just need to fork and exec the program
        int rc = -1;
        int fork_flag = 0;
        strcpy(fullpath, ""); // 初始化fullpath
        for (int path_cnt_i = 0; path_cnt_i < path_cnt; path_cnt_i++) {
          snprintf(fullpath, sizeof(fullpath), "%s/%s", path[path_cnt_i],
                   cmd.argv[cmd_cnt_wra][0]);
          if (access(fullpath, X_OK) == 0) {
            fork_flag = 1;
            break;
          }
        }
        if (fork_flag == 1) {
          rc = fork();
        } else if (fork_flag == 0) { // 无有效目标path
          error_message();
        }
        if (rc < 0) {
          // printf("error:fail to fork\n"); // test:error:fail to fork
        } else if (rc == 0) {
          //      printf("test_2:%d rc==0\n", rc); //
          //      test:输出子进程fork()的return
          // for (int j = 0; j < i; j++) {
          //       printf("test_4:cmd.argv[%d]=%s\n", j,
          //       cmd.argv[cmd_cnt_war][j]);
          //}
          //    printf("test_5:fullpath:%s\n", fullpath); // test:输出fullpath
          // redirection in the current dir
          if (cmd.output_file[cmd_cnt_wra] != NULL &&
              cmd.argv[cmd_cnt_wra] != NULL) {
            // char *dir_prefix = get_current_dir_name();
            // char target_file[256];
            // snprintf(target_file, sizeof(target_file), "%s/%s", dir_prefix,
            //          cmd.output_file);
            setup_output_redirection(cmd.output_file[cmd_cnt_wra]);
          }
          execv(fullpath, cmd.argv[cmd_cnt_wra]);
        } else if (rc > 0) {
          //   printf("test_3:%d rc>0\n", rc); // test:输出子进程fork()的return
          pid[pid_cnt++] = rc;
        }
      }
      // wait for all of the the child process to quit
      // for (int i = 0; i < pid_cnt; i++) {
      //  waitpid(pid[i], NULL, 0);
      //}
    }
    // wait for all of the the child process to quit
    for (int i = 0; i < pid_cnt; i++) {
      waitpid(pid[i], NULL, 0);
    }
  }
  free(line_for_free); // 用lineptr原来的指针来free()一下lineptr的内存
  return 0;
}
