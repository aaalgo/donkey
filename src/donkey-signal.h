#include <stdlib.h>
#include <signal.h>
#include <string.h>

namespace donkey {

class WaitSignal {
    sigset_t waitset;
    static void sigsegv_handler (int) {
        /*
        logger->fatal("SIGSEGV, force exit.");
        logger->fatal(Stack().format());
        */
        exit(1);
    }
    static void handleSigSegV () {
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sigsegv_handler;
        sa.sa_flags = SA_RESETHAND;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, NULL);
    }
public:
    WaitSignal (bool debug = false) {
        //logger = &Poco::Logger::root();
        sigemptyset(&waitset);
        if (!debug) {
            handleSigSegV();
            sigaddset(&waitset, SIGINT);
        }
        sigaddset(&waitset, SIGTERM);
        sigaddset(&waitset, SIGHUP);
        sigprocmask(SIG_BLOCK, &waitset, NULL );
    }

    int wait (int *sig) {
        return sigwait(&waitset, sig);
    }
};
}
