#include "aiwn.h"
#include <SDL.h>
#include <inttypes.h>
#if defined(__linux__) && defined(__x86_64__)
// See /usr/include/x86_64-linux-gnu/sys/ucontext.h
enum {
  REG_R8 = 0,
  REG_R9,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
  REG_RDI,
  REG_RSI,
  REG_RBP,
  REG_RBX,
  REG_RDX,
  REG_RAX,
  REG_RCX,
  REG_RSP,
  REG_RIP,
};
#endif

// In x86_64_backend,we are going to (if supported) use the raw FS/GS registers
// If unable to,we will fill in the "__Fs/__Gs" relocations with a function to
// return the
//  TLS pointers,OTHERWISE I will fill in an offset to FS/GS

// supported:
// MOV RAX,FS/GS:ThreadGs/Fs
//
// Unsupported
//  CALL &GetHolyGs/GetHolyFs
#if defined(__riscv) || defined(__riscv__)
__thread void *ThreadGs;
__thread void *ThreadFs;
void *Misc_ThreadReg();
void *GetHolyGsPtr() {
  char *fs = (char *)&ThreadGs; // thread tls register is FS
  char *base;
  return (char *)fs - (char *)Misc_ThreadReg();
}
void *GetHolyFsPtr() {
  char *fs = (char *)&ThreadFs;
  return (char *)fs - (char *)Misc_ThreadReg();
}
#elif defined(__x86_64__) && defined(__SEG_FS)
  #if defined(__FreeBSD__) || defined(__linux__)
__thread void *ThreadFs;
__thread void *ThreadGs;
void *GetHolyGsPtr() {
  __seg_fs char *fs = (__seg_fs char *)&ThreadGs; // thread tls register is FS
  char *base;
  asm("mov %%fs:0,%0" : "=r"(base));
  return (char *)fs - (char *)base;
}
void *GetHolyFsPtr() {
  __seg_fs char *fs = (__seg_fs char *)&ThreadFs;
  char *base;
  asm("mov %%fs:0,%0" : "=r"(base));
  return (char *)fs - (char *)base;
}
  #else
__thread void *ThreadGs;
__thread void *ThreadFs;

void *GetHolyGsPtr() {
  return &ThreadGs;
}
void *GetHolyFsPtr() {
  return &ThreadFs;
}
  #endif
#elif (defined(_M_ARM64) || defined(__aarch64__))
__thread void *ThreadGs;
__thread void *ThreadFs;
void *GetHolyGsPtr() {
  return (char *)&ThreadGs - (char *)Misc_TLS_Base();
}
void *GetHolyFsPtr() {
  return (char *)&ThreadFs - (char *)Misc_TLS_Base();
}
#else
__thread void *ThreadGs;
__thread void *ThreadFs;
void *GetHolyGsPtr() {
  return &GetHolyGs;
}
void *GetHolyFsPtr() {
  return &GetHolyFs;
}
#endif
void SetHolyFs(void *new) {
  ThreadFs = new;
}
void SetHolyGs(void *new) {
  ThreadGs = new;
}
void *GetHolyFs() {
  return ThreadFs;
}
void *GetHolyGs() {
  return ThreadGs;
}
typedef struct {
  void (*fp)(), *gs;
  int64_t num;
  void (*profiler_int)(void *fs);
  CHashTable *parent_table;
} CorePair;
#if defined(__linux__)
  #include <linux/futex.h>
#endif
#if defined(__FreeBSD__)
  #include <sys/types.h>
  #include <sys/umtx.h>
#endif
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
  #include <pthread.h>
  #include <sys/syscall.h>
  #include <sys/time.h>
  #include <unistd.h>
typedef struct {
  pthread_t pt;
  int wake_futex;
  void (*profiler_int)(void *fs);
  int64_t profiler_freq;
  struct itimerval profile_timer;
  #if defined(__APPLE__)
  pthread_cond_t wake_cond;
  #endif
} CCPU;
#elif defined(_WIN32) || defined(WIN32)
  #include <windows.h>
  #include <processthreadsapi.h>
  #include <synchapi.h>
  #include <sysinfoapi.h>
  #include <time.h>
typedef struct {
  HANDLE thread;
  HANDLE event;
  HANDLE restore_ctx_event;
  CRITICAL_SECTION mtx;
  CONTEXT ctx;
  int64_t awake_at;
  void (*profiler_int)(void *fs);
  int64_t profiler_freq, profiler_last_tick;
  char profile_poop_stk[0x1000];
} CCPU;
#endif
static _Thread_local core_num = 0;

static void threadrt(CorePair *pair) {
  DebuggerClientWatchThisTID();
  Fs = calloc(sizeof(CTask), 1);
  VFsThrdInit();
  TaskInit(Fs, NULL, 0);
  Fs->hash_table->next = pair->parent_table;
  SetHolyGs(pair->gs);
  core_num = pair->num;
  void (*fp)();
  fp = pair->fp;
  free(pair);
  fp();
}
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
static CCPU cores[128];
CHashTable *glbl_table;

void InteruptCore(int64_t core) {
  pthread_kill(cores[core].pt, SIGUSR1);
}
static void InteruptRt(int ul, siginfo_t *info, ucontext_t *_ctx) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);
  CHashExport *y  = HashFind("InteruptRt", glbl_table, HTT_EXPORT_SYS_SYM, 1);
  mcontext_t *ctx = &_ctx->uc_mcontext;
  void (*fp)();
  if (y) {
    fp = y->val;
  #if defined(__FreeBSD__) && defined(__x86_64__)
    FFI_CALL_TOS_2(fp, ctx->mc_rip, ctx->mc_rbp);
  #elif defined(__linux__) && defined(__x86_64__)
    FFI_CALL_TOS_2(fp, ctx->gregs[REG_RIP], ctx->gregs[REG_RBP]);
  #endif
  #if (defined(_M_ARM64) || defined(__aarch64__)) && defined(__FreeBSD__)
    FFI_CALL_TOS_2(fp, NULL, ctx->mc_gpregs.gp_x[29]);
  #elif (defined(__riscv) || defined(__riscv__)) && defined(__linux__)
    FFI_CALL_TOS_2(fp, NULL, ctx->__gregs[8]);
  #elif (defined(_M_ARM64) || defined(__aarch64__)) && defined(__linux__)
    FFI_CALL_TOS_2(fp, NULL, ctx->regs[29]);
  #endif
  }
}
static void ExitCoreRt(int s) {
  pthread_exit(0);
}

static void ProfRt(int64_t sig, siginfo_t *info, ucontext_t *_ctx) {
  int64_t c = core_num;
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGPROF);
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);
  if (!pthread_equal(pthread_self(), cores[c].pt))
    return;
  if (cores[c].profiler_int) {
  #if defined(__x86_64__)
    #if defined(__FreeBSD__)
    FFI_CALL_TOS_1(cores[c].profiler_int, _ctx->uc_mcontext.mc_rip);
    #elif defined(__linux__)
    FFI_CALL_TOS_1(cores[c].profiler_int, _ctx->uc_mcontext.gregs[REG_RIP]);
    #endif
  #endif
    cores[c].profile_timer.it_value.tv_usec    = cores[c].profiler_freq;
    cores[c].profile_timer.it_interval.tv_usec = cores[c].profiler_freq;
  }
}

void SpawnCore(void (*fp)(), void *gs, int64_t core) {
  struct sigaction sa;
  char buf[144];
  CHashTable *parent_table = NULL;
  if (Fs)
    parent_table = Fs->hash_table;
  CorePair pair = {fp, gs, core, NULL, parent_table},
           *ptr = malloc(sizeof(CorePair));
  *ptr          = pair;
  pthread_create(&cores[core].pt, NULL, (void *)&threadrt, ptr);
  char nambuf[16];
  snprintf(nambuf, sizeof nambuf, "Seth(Core%" PRIu64 ")", core);
  #if defined(__APPLE__)
  pthread_setname_np(nambuf);
  #else
  pthread_setname_np(cores[core].pt, nambuf);
  #endif
  #if defined(__APPLE__)
  pthread_cond_init(&cores[core].wake_cond, NULL);
  #endif
  signal(SIGUSR1, InteruptRt);
  signal(SIGUSR2, ExitCoreRt);
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler   = SIG_IGN;
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = (void *)&ProfRt;
  sigaction(SIGPROF, &sa, NULL);
}
int64_t mp_cnt() {
  static int64_t ret = 0;
  if (!ret)
    ret = sysconf(_SC_NPROCESSORS_ONLN);
  return ret;
}

void MPSleepHP(int64_t ns) {
  struct timespec ts = {0};
  ts.tv_nsec         = (ns % 1000000) * 1000U;
  ts.tv_sec          = ns / 1000000;
  Misc_LBts(&cores[core_num].wake_futex, 0);
  #if defined(__linux__)
  syscall(SYS_futex, &cores[core_num].wake_futex, FUTEX_WAIT, 1, &ts, NULL, 0);
  #endif
  #if defined(__FreeBSD__)
  _umtx_op(&cores[core_num].wake_futex, UMTX_OP_WAIT, 1, NULL, &ts);
  #endif
  #if defined(__APPLE__)
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_nsec += (ns % 1000000) * 1000U;
  ts.tv_sec += ts.tv_nsec / 1000000000U;
  ts.tv_nsec %= 1000000000U;
  ts.tv_sec += ns / 1000000;
  pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mtx);
  pthread_cond_timedwait(&cores[core_num].wake_cond, &mtx, &ts);
  #endif
  Misc_LBtr(&cores[core_num].wake_futex, 0);
}

void MPAwake(int64_t core) {
  if (Misc_Bt(&cores[core].wake_futex, 0)) {
  #if defined(__linux__)
    syscall(SYS_futex, &cores[core].wake_futex, 1, FUTEX_WAKE, NULL, NULL, 0);
  #elif defined(__FreeBSD__)
    _umtx_op(&cores[core].wake_futex, UMTX_OP_WAKE, 1, NULL, NULL);
  #endif
  #if defined(__APPLE__)
    pthread_cond_signal(&cores[core].wake_cond);
  #endif
  }
}
void __ShutdownCore(int core) {
  pthread_kill(cores[core].pt, SIGUSR2);
  pthread_join(cores[core].pt, NULL);
}

// Freq in microseconds
void MPSetProfilerInt(void *fp, int c, int64_t f) {
  if (!fp) {
    struct itimerval none;
    none.it_value.tv_sec  = 0;
    none.it_value.tv_usec = 0;
    setitimer(ITIMER_PROF, &none, NULL);
  } else {
    cores[c].profiler_int                      = fp;
    cores[c].profiler_freq                     = f;
    cores[c].profile_timer.it_value.tv_sec     = 0;
    cores[c].profile_timer.it_value.tv_usec    = f;
    cores[c].profile_timer.it_interval.tv_sec  = 0;
    cores[c].profile_timer.it_interval.tv_usec = f;
    setitimer(ITIMER_PROF, &cores[c].profile_timer, NULL);
  }
}
#else
static CCPU cores[128];
CHashTable *glbl_table;
static int64_t ticks    = 0;
static int64_t tick_inc = 1;
static int64_t nproc;
static void WindowsProfileCode(int c);
static void update_ticks(UINT tid, UINT msg, DWORD_PTR dw_user, void *ul,
                         void *ul2) {
  int64_t period;
  ticks += tick_inc;
  for (int64_t idx = 0; idx < nproc; ++idx) {
    EnterCriticalSection(&cores[idx].mtx);
    if (cores[idx].awake_at && ticks >= cores[idx].awake_at) {
      SetEvent(cores[idx].event);
      cores[idx].awake_at = 0;
    }
    LeaveCriticalSection(&cores[idx].mtx);
  }
}
int64_t GetTicksHP() {
  static int64_t init;
  TIMECAPS tc;
  if (!init) {
    init = 1;
    timeGetDevCaps(&tc, sizeof tc);
    tick_inc = tc.wPeriodMin;
    timeSetEvent(tick_inc, tick_inc, &update_ticks, NULL, TIME_PERIODIC);
  }
  return ticks * 1000;
}
void SpawnCore(void (*fp)(), void *gs, int64_t core) {
  CHashTable *parent_table = NULL;
  if (Fs)
    parent_table = Fs->hash_table;
  CorePair pair = {fp, gs, core, NULL, parent_table},
           *ptr = malloc(sizeof(CorePair));
  *ptr          = pair;
  InitializeCriticalSection(&cores[core].mtx);
  cores[core].event             = CreateEvent(NULL, 0, 0, NULL);
  cores[core].restore_ctx_event = CreateEvent(NULL, 0, 0, NULL);
  cores[core].thread            = CreateThread(NULL, 0, threadrt, ptr, 0, NULL);
  SetThreadPriority(cores[core].thread, THREAD_PRIORITY_HIGHEST);
  InstallDbgSignalsForThread();
  nproc++;
}
void MPSleepHP(int64_t us) {
  int64_t s, e;
  s = GetTicksHP() / 1000;
  EnterCriticalSection(&cores[core_num].mtx);
  cores[core_num].awake_at = s + us / 1000;
  LeaveCriticalSection(&cores[core_num].mtx);
  WaitForSingleObject(cores[core_num].event, INFINITE);
}
// Dont make this static,used for miscWIN.s
void ForceYield0() {
  CHashExport *y = HashFind("Yield", glbl_table, HTT_EXPORT_SYS_SYM, 1);
  FFI_CALL_TOS_0(y->val);
}

void InteruptCore(int64_t core) {
  CONTEXT ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.ContextFlags = CONTEXT_ALL;
  SuspendThread(cores[core].thread);
  GetThreadContext(cores[core].thread, &ctx);
  *(uint64_t *)(ctx.Rsp -= 8) = ctx.Rip;
  ctx.Rip = &Misc_ForceYield; // THIS ONE SAVES ALL THE REGISTERS
  SetThreadContext(cores[core].thread, &ctx);
  ResumeThread(cores[core].thread);
}

int64_t mp_cnt() {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
}
void MPAwake(int64_t c) {
  EnterCriticalSection(&cores[c].mtx);
  cores[c].awake_at = 0;
  SetEvent(cores[c].event);
  LeaveCriticalSection(&cores[c].mtx);
}
void __ShutdownCore(int core) {
  TerminateThread(cores[core].thread, 0);
}
void MPSetProfilerInt(void *fp, int c, int64_t f) {
  EnterCriticalSection(&cores[c].mtx);
  cores[c].profiler_int  = fp;
  cores[c].profiler_freq = f * 1000. / 1000000.;
  if (!cores[c].profiler_freq)
    cores[c].profiler_freq = 1;
  cores[c].profiler_last_tick = ticks;
  LeaveCriticalSection(&cores[c].mtx);
}
#endif
