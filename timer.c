#include <err.h>
#include <time.h> // for timers
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void zigbeeSet(const char *partialTopic, char* message);

extern const char HallLight[];
extern const short HallLightState;

extern short LightState; // all off

extern timer_t hallLightTimer;

void timerHandler(int signum, siginfo_t *info, void *ucontex __attribute__((unused))) {
	// info->si_value.sival_int is supposed to have the timer, according to sigaction(2) and timer_create, but it's always 0.
	// info->si_timerid is supposed to be a special kernel id for the timer that won't match timer_create, but it always
	// matches. Is this specific to arm?
	printf("Timer expired. Signal: %d. KTimer: %u. Timer: %u\n", signum, info->si_timerid, info->si_value.sival_int);

	timer_t timerID = (timer_t)(long)info->si_timerid;
	if (timerID == hallLightTimer) {
		zigbeeSet(HallLight, R"({"state":"OFF"})");
		LightState &= ~HallLightState;
	}
}

void initTimerHandler() {
    struct sigaction sa;

    // Set up the signal handler
    sa.sa_sigaction = timerHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        err(EXIT_FAILURE, "sigaction");
    }
}

timer_t createTimer() {
    timer_t timerID;

    // Create the timer
    if (timer_create(CLOCK_MONOTONIC, NULL, &timerID) == -1) {
        err(EXIT_FAILURE, "timer_create");
    }

	//printf("Timer created: %zu\n", timerID);

    return timerID;
}

void deleteTimer(timer_t timerID) {
    // Cleanup
    timer_delete(timerID);
}

void startTimer(timer_t timerID, int sec) {
    struct itimerspec timer_spec;

    // Configure the timer to expire after 2 seconds and then every 2 seconds
    timer_spec.it_value.tv_sec = sec;  // Initial expiration
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 0;  // Subsequent expiration
    timer_spec.it_interval.tv_nsec = 0;

    // Start the timer
    if (timer_settime(timerID, 0, &timer_spec, NULL) == -1) {
        err(EXIT_FAILURE, "timer_settime");
    }

	printf("Timer started: %zu, %ds\n", (size_t)timerID, sec);
}
