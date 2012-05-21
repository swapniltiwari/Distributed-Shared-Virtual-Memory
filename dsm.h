#ifndef DSM_H
#define DSM_H

void initializeDSM(int ismaster, char * masterip, int mport, char *otherip, int oport,
        unsigned numpagestoalloc);
void * getsharedregion();

#endif
