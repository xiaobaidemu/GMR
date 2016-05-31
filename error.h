/*************************************************************************/
/*! 此文件主要用来定义与信号操作相关的函数和变常量
*/
/**************************************************************************/

#define SIGERR                  SIGTERM

void error_exit(int signo) {
    printf("Program exit in the running, with error:%d\n", signo);
    exit(signo);
}

void errexit(int signo, std::string errinfo) {
    std::cout << errinfo;
    raise(signo);
}
