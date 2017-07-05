#define _GNU_SOURCE
#define __USE_GNU
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <dlfcn.h>
#ifndef __ANDROID__
#include <execinfo.h>
#include <ucontext.h>
#endif

#include "trace.h"
#include "locker.h"

static char log_file[1024] = {0};
static int pipe_fd[2] = {-1,-1};
static int print_trace(const char *format, ...);
static pthread_mutex_t trace_lock;

static char *translate_signal(int sig)
{
#define SIG_CASE(_s) \
    case _s: \
        return #_s

    switch(sig)
    {
        SIG_CASE(SIGSEGV);
        SIG_CASE(SIGBUS);
        SIG_CASE(SIGABRT);
        default:
            break;
    }

    return "unknown";
}

static int print_trace(const char *format, ...)
{
    int ret = 0;
    va_list args;
    FILE *fp = fopen(log_file, "a+");
    if (fp==NULL){
        fp = stderr;
        ret = -1;
    }

    va_start(args, format);
    vfprintf(fp, format, args);
    fflush(fp);

    if (fp!=stderr){
        fclose(fp);
    }
    va_end(args);
    return ret;
}

int trace_get_progname(char *buf, int size)
{
    char path[1024] = {0};
    char *str = NULL;
    FILE *fp = NULL;
    sprintf(path, "/proc/%d/cmdline", getpid());
    if ((fp=fopen(path, "r"))==NULL){
        fprintf(stderr, "open program cmdline file failed\n");
        return -1;
    }
    fgets(path, size, fp);
    fclose(fp);
    if ((str=strrchr(path, '/'))){
        strcpy(buf, str+1);
    }else{
        strcpy(buf, path);
    }

    return 0;
}

#ifdef __ANDROID__
static int backtrace (void **buffer, int size)
{
    int i = 0;
#if defined(__x86_64__) || defined(__i386__)
    void **_ebp = NULL;
    void **_eip = NULL;
    void **_esp = NULL;
    ucontext_t uctx;
    if (getcontext(&uctx)){
        return -1;
    }
#if defined(__x86_64__)
    _ebp = (void **)uctx.uc_mcontext.gregs[REG_RBP];
    _eip = (void **)uctx.uc_mcontext.gregs[REG_RIP];
    _esp = (void **)uctx.uc_mcontext.gregs[REG_RSP];
#elif defined(__i386__)
    _ebp = (void **)uctx.uc_mcontext.gregs[REG_EBP];
    _eip = (void **)uctx.uc_mcontext.gregs[REG_EIP];
    _esp = (void **)uctx.uc_mcontext.gregs[REG_ESP];
#endif
#if 0
    printf("ebp: 0x%x, eip: 0x%x, esp: 0x%x\n", _ebp, _eip, _esp);
    for (i=0; i<100;i++){
        printf("esp[%d]: 0x%x\n",i, _esp[i]);
    }
#endif
    for(i = 0; i < size; i++){
        _eip = _ebp + 1;
        buffer[i] = *_eip;
        _ebp = (void **)*_ebp;
        if (_ebp==NULL){
            break;
        }
    }
#elif defined(__ANDROID__)
    void **_fp = (void **)&buffer + 1;
    void **_sp = _fp + 1;
    void **_lr = _fp + 2;
    void **_pc = _fp + 3;
    for (i = 0; i < size; i++){
        buffer[i] = (void **)*_pc;
        _pc = (void **)*_fp - 1;
        _fp = (void **)*_fp - 4;
        if (_fp==NULL){
            break;
        }
    }
#endif

    return i;
}

typedef char (*trace_buf_t)[1024];
static char **backtrace_symbols(void **bt, int size)
{
    int i;
    uint8_t **pc = NULL;
    uint64_t symbol_offset = 0;
    uint64_t offset = 0;
    char *sname = NULL;
    char *fname = NULL;
    char **trace = (char **)calloc( (sizeof(char *)+1024), size );
    trace_buf_t trace_buf = (trace_buf_t)((char *)trace + sizeof(char *)*size);

    for (i=0; i<size; i++){
        Dl_info dl_info;
        pc = (uint8_t **)bt[i];
        trace[i] = trace_buf[i];
        if (dladdr((uint8_t *)pc, &dl_info)==0){
            continue;
        }
        sname = (char *)dl_info.dli_sname;
        fname = (char *)dl_info.dli_fname;

        if (sname==NULL){
            sname = "";
        }
        if (fname==NULL){
            fname = "";
        }

        symbol_offset = (uint64_t)pc;
        offset = (uint64_t)pc - (uint64_t)dl_info.dli_saddr;
        if (offset==symbol_offset){
            sprintf(trace_buf[i], "%s() [0x%x]", fname, symbol_offset);
        }else{
            sprintf(trace_buf[i], "%s(%s+0x%x) [0x%x]", fname, sname, offset, symbol_offset);
        }
    }

    return trace;
}

static int android_trace(void **bt, int size)
{
    char **stacktrace = backtrace_symbols(bt, size);
    int i;
    int count = 0;
    for (i=0; i<size; i++){
        print_trace("%d: %s", i, stacktrace[i]);
    }

    free(stacktrace);
    return 0;
}

#elif defined(__x86_64__)||defined(__i386__)
static int addr_to_line(char *offset, char *fpath)
{
    if (fork()==0){
        dup2(pipe_fd[1], 1);
        close(pipe_fd[1]);
        if(execlp("addr2line", "addr2line", offset, "-e", fpath, NULL)<0){
            fprintf(stderr, "execl addr2line failed\n");
            exit(1);
        }
        exit(0);
    }
    char buf[1024] = {0};
    char *str = buf;
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 500000,
    };
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(pipe_fd[0], &readfds);
    if (select(pipe_fd[0] + 1, &readfds, NULL,  NULL, &tv)>0 && FD_ISSET(pipe_fd[0], &readfds)){
        do{
            read(pipe_fd[0], str, 1);
        }while(*str++!='\n');
        FD_ZERO(&readfds);
        FD_SET(pipe_fd[0], &readfds);
    }
    if (str!=buf){
        print_trace(buf);
    }
    return 0;
}

static int x86_trace(void **bt, int size)
{
    char **stacktrace = backtrace_symbols(bt, size);
    int i;
    int count = 0;
    for (i=0; i<size; i++){
        /* trace format is: binfile(symbol+offset) [addr] */
        char *trace = stacktrace[i];
        if (trace==NULL){
            continue;
        }
        char buf[1024] = {0};
        char offset[32] = {0};
        char *fpath = NULL;
        char *addr = NULL;
        char *symbol = NULL;
        char *token = NULL;
        char *fname = NULL;
        print_trace("%d: %s\t\t", count++, trace);
        /* get bin file name from trace */
        fpath = buf;
        strcpy(buf, trace);
        token = strchr(buf, '(');
        if (token==NULL){
            continue;
        }
        *token++ = '\0';

        /* get symbol form trace */
        symbol = token;
        token = strchr(token, ')');
        if (token==NULL){
            continue;
        }
        *token++ = '\0';
        token = strchr(token, '[');
        if (token==NULL){
            continue;
        }

        /* get addr from trace */
        addr = ++token;
        token = strchr(token, ']');
        if (token==NULL){
            continue;
        }
        *token = '\0';

        /* get file name from file path */
        if ( (fname = strrchr(fpath, '/')) ){
            fname++;
        }else{
            fname = fpath;
        }

		unsigned int symbol_offset = 0;
        /* check if bin file is .so */
        uint8_t **pc = NULL;
        Dl_info dl_info;
        if (strstr(fname, ".so") && *symbol!='\0'){
            pc = (uint8_t **)bt[i];
            /* if 'symbol' is not symbol name (just an address), or can't find it address from .so */
            if ( (dladdr((uint8_t *)pc, &dl_info)==0) || *symbol=='+'){
                /* if 'symbol' is address, we must use it as offset, or we can use 'addr' as offset */
                if (*symbol=='+'){
                    addr = symbol + 1;
                }
				strcpy(offset, addr);
            }else{
                /* if 'symbol' is symbol name, we must get it address of .so file.
                 * the dli_saddr is the absolute address in memory, the dli_fbase is the absolute address of .so.
                 * so dli_saddr - dli_fbase is the symbol's relatively address in .so file */
                unsigned int symbol_offset = (uint64_t)dl_info.dli_saddr - (uint64_t)dl_info.dli_fbase;
				char *func_offset = strchr(symbol, '+');
				int f_offset = 0;
				if (func_offset){
					func_offset++;
					sscanf(func_offset, "%x", &f_offset);
				}
				symbol_offset += (f_offset - 1);
        		sprintf(offset, "0x%x", symbol_offset);
            }
        }else{
			sscanf(addr, "%x", &symbol_offset);
            if (symbol && *symbol!='+'){
                symbol_offset--;
            }
			sprintf(offset, "0x%x", symbol_offset);
        }

        addr_to_line(offset, fpath);
    }
    free(stacktrace);

    return 0;
}
#endif

void on_sigsegv(int signum, siginfo_t *info, void *ptr)
{
    void *bt[100];
    int size = backtrace(bt, sizeof(bt)/sizeof(bt[0]));

    /* prevent log in a mess in case of multi threads*/
	MUTEX_LOCK(&trace_lock);

    time_t logtime = time(NULL);
    struct tm *local_time = localtime(&logtime);
    char *datetime = asctime(local_time);
    print_trace("\n------------------thread: 0x%x-----------------------\n", pthread_self());
    print_trace("get signal %s at time: %s", translate_signal(signum), datetime);
    print_trace("backtrace:\n");

#if defined(__ANDROID__)
    android_trace( bt + 2, size - 2);
#elif defined(__i386__)||defined(__x86_64__)
    x86_trace( bt + 2, size - 2);
#endif

	MUTEX_UNLOCK(&trace_lock);

    /* if another thread hold the lock, we can't exit, must wait it print it's log */
	sleep(1);
	if(pthread_mutex_trylock(&trace_lock) == 0)
	{
		MUTEX_UNLOCK(&trace_lock);
    	fprintf(stderr,"get signal %s at time: %s", translate_signal(signum), datetime);
		if (access(log_file,F_OK)==0){
	    	fprintf(stderr,"please see log file %s\n", log_file);
		}
		exit(1);
	}
}

int trace_init(const char *logdir)
{
#ifdef TRACE
    int isdir = 1;
    if (access(logdir, F_OK)!=0){
        isdir = 0;
    }else{
        struct stat st;
        stat(logdir, &st);
        isdir = S_ISDIR(st.st_mode);
    }
    /* check log dir and make trace file name */
    if (!isdir){
        fprintf(stderr, "%s is not exist, use /tmp\n", logdir);
        logdir = "/tmp";
    }

    char prog_name[1024] = {0};
    char log_dir[256] = {0};
    int len = strlen(logdir);
    if (logdir[len-1]=='/'){
        len--;
    }
    strncpy(log_dir, logdir, len);
    trace_get_progname(prog_name, 1023);
    sprintf(log_file, "%s/%s.%d.trace",log_dir, prog_name, getpid());

#ifndef __ANDROID__
    if (pipe(pipe_fd)!=0){
        return -1;
    }
#endif
	MUTEX_LOCK_INIT(&trace_lock);

#if 0
    /* create new stack for signal */
    stack_t ss;
    int s_size = 1024*128;
    ss.ss_sp = malloc(s_size);
    if (ss.ss_sp==NULL){
        fprintf(stderr, "failed to malloc\n");
        return -1;
    }
    ss.ss_size = s_size;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1){
        fprintf(stderr, "failed to set signal stack\n");
        return -1;
    }
#endif

    /* install signal action */
    struct sigaction myAction;
    sigemptyset(&myAction.sa_mask);
    myAction.sa_sigaction = on_sigsegv;
    myAction.sa_flags = SA_SIGINFO|SA_ONSTACK;
    sigaction(SIGSEGV, &myAction, NULL);
    sigaction(SIGBUS, &myAction, NULL);
    sigaction(SIGABRT, &myAction, NULL);
    sigaction(SIGFPE, &myAction, NULL);
    sigaction(SIGILL, &myAction, NULL);
#if 0
    sigaction(SIGUSR1, &myAction, NULL);
    sigaction(SIGSYS, &myAction, NULL);
#endif
#endif
    return 0;
}
