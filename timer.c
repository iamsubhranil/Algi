#include <time.h>
#include "display.h"

static clock_t start = 0, end = 0;
static const char* runningJob[] = {"Nothing"};

void timer_end(){
    if(start != 0){
        end = clock();
        double sec = (double)(end-start)/CLOCKS_PER_SEC;
        info("%s finished in %f seconds\n", runningJob[0], sec);
        start = end = 0;
        runningJob[0] = "Nothing";
    }
}

void timer_start(const char* job){
    if(start != 0)
        timer_end();
    runningJob[0] = job;
    info("Started %s\n", job);
    start = clock();
}
