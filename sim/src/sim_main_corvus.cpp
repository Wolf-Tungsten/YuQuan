#include "CYuQuanCModelGen.h"
#include <sim_main.hpp>
#include <atomic>
#include <memory>
#include <pthread.h>
#include <signal.h>
#include <thread>

static corvus_generated::CYuQuanTopModuleGen::TopPortsGen *top = nullptr;
static std::unique_ptr<corvus_generated::CYuQuanCModelGen> g_cmodel;
static std::atomic<bool> g_cmodel_cleaned{false};
struct termios new_settings, stored_settings;
uint64_t cycles = 0;
static uint64_t no_commit = 0;
static sigset_t sigset_int;
static std::thread sigint_thread;

static void setup_sigint_handler();
static void cleanup_cmodel_once();

void real_int_handler(void) {
  tcsetattr(0, TCSAFLUSH, &stored_settings);
  setlinebuf(stdout);
  setlinebuf(stderr);
  scan_uart(_isRunning) = false;
  cleanup_cmodel_once();
  if (top) {
    printf("\n" DEBUG "Exit at PC = " FMT_WORD " after %ld clock cycles.\n", top->io_wbPC, cycles / 2);
  }
  exit(0);
}

int main(int argc, char **argv, char **env) {
  printf("Sim Started\n");
  setup_sigint_handler();
  g_cmodel = std::make_unique<corvus_generated::CYuQuanCModelGen>();
  printf("CYuQuanCModelGen created\n");
  top = g_cmodel->ports();
  if (top == nullptr) {
    fprintf(stderr, "Failed to acquire YuQuan CModel ports\n");
    return 1;
  }

#ifdef DIFFTEST
  void *ram_param =
#endif
  ram_init(argv[1]);
  sdcard_init(argv[1]);

#ifdef FLASH
  flash_init(argv[2]);
#endif

#ifdef STORAGE
  storage_init(argv[3]);
#endif

#ifdef DIFFTEST
  vaddr_t pc, spike_pc;
  {
    size_t tmp[50] = {};
    difftest_init(0);
    difftest_regcpy(tmp, DIFFTEST_TO_DUT);
    tmp[32] = 0x80000000UL;
    difftest_regcpy(tmp, DIFFTEST_TO_REF);
    difftest_memcpy(0x80000000UL, ram_param, PMEM_SIZE, DIFFTEST_TO_REF);
  }
  QData *gprs = &top->io_gprs_0;
  char name[15] = {};
  size_t cpu_reg, diff_reg;
  size_t diff_regs[50];
#endif

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  tcgetattr(0, &stored_settings);
  new_settings = stored_settings;
  new_settings.c_lflag &= ~ECHOFLAGS;
  tcsetattr(0, TCSAFLUSH, &new_settings);
  int ret = 0;
  scan_uart(_init)();
  printf("TB Init done\n");

  top->reset = 1;
  top->clock = 0;
  printf("Applying initial reset, entering first eval\n");
  g_cmodel->eval();
  printf("First eval completed\n");

  printf("CYuQuanCModelGen reset start\n");

  for (int i = 0; i < 50; i++) {
    top->clock = !top->clock;
    g_cmodel->eval();
  }

  top->reset = 0;
  printf("Reset loop finished, reset deasserted\n");
  printf("CYuQuanCModelGen reset end\n");
  for (;;cycles++) {
#ifdef mainargs
    if (cycles == 246656526)
      command_init(to_string(mainargs) "\n");
#endif
    top->clock = !top->clock;
    g_cmodel->eval();
    printf(DEBUG "cycle %ld, clock %d\n", cycles, top->clock);
    no_commit = top->io_wbValid ? 0 : no_commit + 1;
    if (no_commit > 1000000) {
      printf(DEBUG "Seems like stuck.\n");
      real_int_handler();
    }

#ifdef DIFFTEST
    if (top->io_wbValid && top->clock) {
      pc = top->io_wbPC;
      spike_pc = diff_gpr_pc.pc[0];
      if (pc != spike_pc) {
        strcpy(name, "pc");
        cpu_reg = pc;
        diff_reg = spike_pc;
        goto reg_diff;
      }
      bool skip = top->io_wbIntr || top->io_exit || top->io_wbRcsr == 0x344 ||
                  top->io_wbRcsr == 0xC01 || top->io_wbMMIO;
      if (!skip) {
        difftest_exec(1);
        difftest_regcpy(diff_regs, DIFFTEST_TO_DUT);
        add_diff(mtval);
        add_diff(stval);
        add_diff(mcause);
        add_diff(scause);
        add_diff(mepc);
        add_diff(sepc);
        add_diff(mstatus);
        add_diff(mtvec);
        add_diff(stvec);
        add_diff(mie);
        add_diff(mscratch);
        add_diff(priv);
        for (int i = 0; i < 32; i++) if (diff_regs[i] != gprs[i]) {
          char tmp[10];
          sprintf(tmp, "GPR[%d]", i);
          strcpy(name, tmp);
          cpu_reg = gprs[i];
          diff_reg = diff_regs[i];
          goto reg_diff;
        }
      } else {
        if (skip && !top->io_wbIntr)
          difftest_exec(1);
        size_t tmp[50];
        difftest_regcpy(tmp, DIFFTEST_TO_DUT);
        memcpy(tmp, gprs, 32 * sizeof(size_t));
        tmp[mstatus] = top->io_mstatus;
        tmp[mepc] = top->io_mepc;
        tmp[sepc] = top->io_sepc;
        tmp[mtvec] = top->io_mtvec;
        tmp[stvec] = top->io_stvec;
        tmp[mcause] = top->io_mcause;
        tmp[scause] = top->io_scause;
        tmp[mtval] = top->io_mtval;
        tmp[stval] = top->io_stval;
        tmp[mie] = top->io_mie;
        tmp[mscratch] = top->io_mscratch;
        tmp[priv] = top->io_priv;
        tmp[32] = top->io_wbIntr ? (top->io_priv == 0b11 ? tmp[mtvec] : tmp[stvec]) : pc + (top->io_wbRvc ? 2 : 4);
        difftest_regcpy(tmp, DIFFTEST_TO_REF);
      }
    }
#endif

    if (top->io_exit == 1) {
      printf(DEBUG "Exit after %ld clock cycles.\n", cycles / 2);
      printf(DEBUG);
      if (top->io_gprs_10) {
        printf("\33[1;31mHIT BAD TRAP");
        ret = 1;
      }
      else printf("\33[1;32mHIT GOOD TRAP");
      printf("\33[0m at pc = " FMT_WORD "\n\n", top->io_wbPC - 4);
      break;
    }
    else if (top->io_exit == 2) {
      printf(DEBUG "Exit after %ld clock cycles.\n", cycles / 2);
      printf(DEBUG "\33[1;31mINVALID INSTRUCTION");
      printf("\33[0m at pc = " FMT_WORD "\n\n", top->io_wbPC - 4);
      ret = 1;
      break;
    }
#ifdef DIFFTEST
    continue;
  reg_diff:
    std::cout << DEBUG "Exit after " << cycles / 2 << " clock cycles.\n";
    std::cout << DEBUG "\33[1;31m" << name << " Diff\33[0m ";
    printf("at pc = " FMT_WORD "\n" DEBUG, pc);
    printf("pc = " FMT_WORD "\tspike_pc = " FMT_WORD "\n", pc, diff_regs[32]);
    for (int i = 0; i < 32; i++)
      printf("GPR[%d] = " FMT_WORD "\tspike_GPR[%d] = " FMT_WORD "\n", i, gprs[i], i, diff_regs[i]);
    print_csr(mstatus);
    print_csr(mtval);
    print_csr(stval);
    print_csr(mcause);
    print_csr(scause);
    print_csr(mepc);
    print_csr(sepc);
    print_csr(mstatus);
    print_csr(mtvec);
    print_csr(stvec);
    print_csr(mie);
    print_csr(mscratch);
    print_csr(priv);
    ret = 1;
    break;
#endif
  }

  scan_uart(_isRunning) = false;
  tcsetattr(0, TCSAFLUSH, &stored_settings);
  setlinebuf(stdout);
  setlinebuf(stderr);
  cleanup_cmodel_once();
  return ret;
}

static void setup_sigint_handler() {
  sigemptyset(&sigset_int);
  sigaddset(&sigset_int, SIGINT);
  pthread_sigmask(SIG_BLOCK, &sigset_int, nullptr);
  sigint_thread = std::thread([]() {
    int sig;
    while (sigwait(&sigset_int, &sig) == 0) {
      if (sig == SIGINT) {
        real_int_handler();
      }
    }
  });
  sigint_thread.detach();
}

static void cleanup_cmodel_once() {
  bool expected = false;
  if (g_cmodel_cleaned.compare_exchange_strong(expected, true)) {
    if (g_cmodel) {
      g_cmodel.reset();
    }
    top = nullptr;
  }
}
