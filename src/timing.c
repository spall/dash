#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/times.h>

static struct timespec *start;
static struct timespec *end;
const char* outputfile;
static int oldscnum;
static int argc;
static char **argv;
static pid_t caller;

void starttime(int argc_, char **argv_)
{
        caller = getpid();
        argc = argc_;
        argv = argv_;
        // environment variables we use
        const char* scnum = getenv("SCNUM"); // contains the shell command number
        outputfile = getenv("OUTPUTFILE"); // contains the path of outputfile

        if (scnum == NULL) {
          exit(EXIT_FAILURE);
        }
        if (outputfile == NULL) {
          exit(EXIT_FAILURE);
        }

        // read number from file
        FILE *scnum_file = fopen(scnum, "r");
        char* line = NULL;
        size_t len = 0;
        if (getline(&line, &len, scnum_file) == -1) {
              perror("getline");
              exit(EXIT_FAILURE);
        }
        fclose(scnum_file);

        // store number in CURSCNUM environment variable
        setenv("CURSCNUM", line, 1);

        // increment number and write back to file
        scnum_file = fopen(scnum, "w");
        oldscnum = atoi(line);
        fprintf(scnum_file, "%d\n", oldscnum + 1);
        fflush(scnum_file);
        fclose(scnum_file);

        FILE *out = fopen(outputfile, "a");
        fprintf(out, "executing shell-command: %d", oldscnum);
	int i;
	for(i = 0; i < argc; i ++) {
	  fprintf(out, " %s", argv[i]);
	}
	fprintf(out, "\n");
        fflush(out);
        fclose(out);
        
        start = malloc(sizeof(struct timespec));
        end = malloc(sizeof(struct timespec));

        if (start == NULL || end == NULL) {
          perror("malloc");
          exit(EXIT_FAILURE);
        }

        if (-1 == clock_gettime(CLOCK_REALTIME, start)) {
          exit(EXIT_FAILURE);
        }
}

void endtime()
{
  
        if (getpid() != caller) {
	  return;
	}
	
        if (clock_gettime(CLOCK_REALTIME, end) == -1) {
          exit(EXIT_FAILURE);
        }

        struct tms *cpu_time = malloc(sizeof(struct tms));
        if (cpu_time == NULL) {
          perror("malloc");
          exit(EXIT_FAILURE);
        }

        if (times(cpu_time) == -1) {
          exit(EXIT_FAILURE);
        }

        fprintf(stderr, "cpu is %lld\n", cpu_time->tms_cutime);
        
        double user_time = ((double)(cpu_time->tms_utime + cpu_time->tms_cutime)) / CLOCKS_PER_SEC;
        double sys_time = ((double)(cpu_time->tms_stime + cpu_time->tms_cstime)) / CLOCKS_PER_SEC;
         
        struct timespec *tt = malloc(sizeof(struct timespec));
        if (tt == NULL) {
          perror("malloc");
          exit(EXIT_FAILURE);
        }
        struct timespec *overhead = malloc(sizeof(struct timespec));
        if (overhead == NULL) {
          perror("malloc");
          exit(EXIT_FAILURE);
        }
        estimate_timing_overhead(overhead);
        
        timespec_subtract_3(tt, end, start, overhead);
        
        FILE *out = fopen(outputfile, "a");
        fprintf(out, "argv=");
        int i;
        for(i = 0; i < argc; i ++) {
          fprintf(out, " %s", argv[i]);
        }
        fprintf(out, "\nelapsed= %lld.%.9ld; user= %f ; sys= %f\n", (long long)tt->tv_sec, tt->tv_nsec, user_time, sys_time);
        fprintf(out, "finished shell-command: %d\n", oldscnum);
        fflush(out);
        fclose(out);
}

int estimate_timing_overhead(struct timespec *overhead) {
  struct timespec *ov1 = malloc(sizeof(struct timespec));
  struct timespec *ov2 = malloc(sizeof(struct timespec));
  
  if (ov1 == NULL || ov2 == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  int r1 = clock_gettime(CLOCK_REALTIME, ov1);
  int r2 = clock_gettime(CLOCK_REALTIME, ov2);
  if (-1 == r1 || -1 == r2) {
    exit(EXIT_FAILURE);
  }
  
  if (1 == timespec_subtract(overhead, ov2, ov1)) { // ov2 - ov1
    fprintf(stderr, "Negative overhead\n");
    exit(EXIT_FAILURE);
  }

  free(ov1);
  free(ov2);
  return EXIT_SUCCESS;
}

int timespec_subtract(struct timespec *result, struct timespec *x, struct timespec *y)
{

  time_t sec_add;
  unsigned long long nsec_add;
  if (x->tv_nsec < y->tv_nsec) {
    sec_add = x->tv_sec - 1; /* steal 1 second */
    nsec_add = 1000000000 + x->tv_nsec;
    
    result->tv_nsec = nsec_add - y->tv_nsec;
    result->tv_sec = sec_add - y->tv_sec;
  } else {
    result->tv_nsec = x->tv_nsec - y->tv_nsec;
    result->tv_sec = x->tv_sec - y->tv_sec;
  }

  if (x->tv_sec == y->tv_sec) {
    return x->tv_nsec < y->tv_nsec;
  }
  
  return x->tv_sec < y->tv_sec;
}

// result = x - y - z
int timespec_subtract_3(struct timespec *result, struct timespec *x, struct timespec *y, struct timespec *z) {

  struct timespec *tmp = malloc(sizeof(struct timespec));
  
  if(tmp == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  if (1 == timespec_subtract(tmp, x, y)) {
    fprintf(stderr, "Negative time: %lld.%ld - %lld.%ld\n", (long long)x->tv_sec, x->tv_nsec, (long long)y->tv_sec, y->tv_nsec);
    exit(EXIT_FAILURE);
  }

  if (1 == timespec_subtract(result, tmp, z)) {
    fprintf(stderr, "Negative time: %lld.%ld - %lld.%ld\n", (long long)tmp->tv_sec, tmp->tv_nsec, (long long)z->tv_sec, z->tv_nsec);
    exit(EXIT_FAILURE);
  }

  free(tmp);

  return EXIT_SUCCESS;
}
