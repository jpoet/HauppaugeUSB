#include "baseif.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

int wrapFileLoad(const char *fn, void **buf, size_t *len)
{
    struct stat st;
    void *b;
    int l;
    int fd = open(fn, O_RDONLY);
    if(fd < 0) {
        WRAPLOG(WRAPLOGL_ERROR, "wrapFileLoad(): can't open settings file '%s'", fn);
        return WRAPOS_ERROR;
    }
    if(fstat(fd, &st) < 0) {
        WRAPLOG(WRAPLOGL_ERROR, "wrapFileLoad(): can't stat settings file");
        close(fd);
        return WRAPOS_ERROR;
    }
    l = st.st_size;
    b = wrapHeapAlloc(l);
    if(!b) {
        WRAPLOG(WRAPLOGL_ERROR, "wrapFileLoad(): can't alloc mem for settings");
        close(fd);
        return WRAPOS_ERROR;
    }
    if(read(fd, b, l) != l) {
        WRAPLOG(WRAPLOGL_ERROR, "wrapFileLoad(): can't read settings file");
        close(fd);
        wrapHeapFree(b);
        return WRAPOS_ERROR;
    }
    close(fd);
    *buf = b;
    *len = l;
    return WRAPOS_OK;
}



// There are POSIX no thread shared semaphores on Mac OS X (process shared only):
// https://discussions.apple.com/thread/4787587?start=0&tstart=0
// MPCreateSemaphore / MPDeleteSemaphore / MPSignalSemaphore / MPWaitOnSemaphore have been deprecated since 10.7:
// https://developer.apple.com/library/mac/documentation/Carbon/reference/Multiprocessing_Services/Reference/reference.html
// So, let's do some cool improvisation...

typedef struct {
        pthread_cond_t c;
        pthread_mutex_t m;
        volatile unsigned int cnt;
} wrapSemInfo_t;

int wrapSemCreate(wrapSem_t *sem, unsigned int cnt)
{
    wrapSemInfo_t *si = (wrapSemInfo_t*)wrapHeapAlloc(sizeof(wrapSemInfo_t));
    si->cnt = cnt;
    pthread_mutex_init(&si->m, NULL);
    pthread_cond_init(&si->c, NULL);
    *sem = (wrapSem_t)si;
    return WRAPOS_OK;
}

int wrapSemDestroy(wrapSem_t *sem)
{
    wrapSemInfo_t *si = (wrapSemInfo_t*)*sem;
    if(pthread_cond_destroy(&si->c) == EBUSY) {
        WRAPLOG(WRAPLOGL_ERROR, "wrapSemDestroy(): cond is busy");
        return WRAPOS_ERROR;
    }
    if(pthread_mutex_destroy(&si->m) == EBUSY) {
        WRAPLOG(WRAPLOGL_ERROR, "wrapSemDestroy(): mutex is busy");
        return WRAPOS_ERROR;
    }
    wrapHeapFree(si);
    *sem = NULL;
    return WRAPOS_OK;
}

int wrapSemGive(wrapSem_t *sem)
{
    wrapSemInfo_t *si = (wrapSemInfo_t*)*sem;
    pthread_mutex_lock(&si->m);
    ++si->cnt;
    pthread_cond_signal(&si->c);
    pthread_mutex_unlock(&si->m);
    return WRAPOS_OK;
}

int wrapSemTake(wrapSem_t *sem, int ms)
{
    wrapSemInfo_t *si = (wrapSemInfo_t*)*sem;
    pthread_mutex_lock(&si->m);
    int r = 0;
    if(si->cnt == 0) {
        if(ms < 0) {
            while((si->cnt == 0) &&
                  ((r = pthread_cond_wait(&si->c, &si->m)) == 0));
        } else {
            struct timeval tv;
            struct timespec ts;
            gettimeofday(&tv, NULL);
            ts.tv_nsec = tv.tv_usec * 1000 + (ms % 1000) * 1000000;
            ts.tv_sec = tv.tv_sec + ms / 1000 + ts.tv_nsec / 1000000000;
            ts.tv_nsec %= 1000000000;
            while((si->cnt == 0) &&
                  ((r = pthread_cond_timedwait(&si->c, &si->m, &ts)) == 0));
        }
    }
    int ret = WRAPOS_ERROR;
    if(si->cnt == 0) {
        if(r != ETIMEDOUT) wrapLogError("wrapSemTake(): invalid state");
        ret = (r == ETIMEDOUT) ? WRAPOS_TIMEOUT : WRAPOS_ERROR;
    } else {
        --si->cnt;
        ret = WRAPOS_OK;
    }
    pthread_mutex_unlock(&si->m);
    return ret;
}



typedef struct {
        pthread_t handle;
        wrapThreadFunction_t function;
        void *pData;
} wrapThreadInfo_t;

static void *_wrapThreadFunc(void *info) {
        ((wrapThreadInfo_t*)info)->function(((wrapThreadInfo_t*)info)->pData);
        return NULL;
}

int wrapThreadStart(wrapThread_t *thread, wrapThreadFunction_t func,
                    void *pData)
{
    wrapThreadInfo_t *info = (wrapThreadInfo_t*)
                             wrapHeapAlloc(sizeof(wrapThreadInfo_t));
    if (info == nullptr)
    {
        wrapLogError("wrapThreadStart(): Failed to allocate space");
        return WRAPOS_ERROR;
    }
    info->function = func;
    info->pData = pData;
    int err = pthread_create(&info->handle, NULL, _wrapThreadFunc, info);
    if(err)
    {
        wrapLogError("wrapThreadStart(): Failed to create thread");
        return WRAPOS_ERROR;
    }

    *thread = (wrapThread_t)info;
    return WRAPOS_OK;
}

int wrapThreadStop(wrapThread_t *thread)
{
    wrapThreadInfo_t *info = (wrapThreadInfo_t*)*thread;
    if (info == nullptr)
    {
        wrapLogError("wrapThreadStop(): invalid thread");
        return WRAPOS_ERROR;
    }
    pthread_cancel(info->handle);
    pthread_join(info->handle, NULL);
    wrapHeapFree(info);
    *thread = NULL;
    return WRAPOS_OK;
}

#ifdef __cplusplus
}
#endif
