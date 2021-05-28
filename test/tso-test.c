/*******************************************************************************
 * Compile and run:
 * 	gcc -DTSO=1 -o tso-test tso-test.c -lpthread && ./tso-test
 * 	gcc -DTSO=0 -o tso-test tso-test.c -lpthread && ./tso-test
 *
 * With TSO=0 the assertion should fail.
 ******************************************************************************/
#include <stdatomic.h>
#include <pthread.h>
#include <sys/sysctl.h>
#include <assert.h>

#define NITERS 1000000 // number of iterations

atomic_int lock;
#define lock_acquire() \
	while(atomic_exchange_explicit(&lock, 1, memory_order_relaxed)) {}
#define lock_release() atomic_store(&lock, 0)

#ifndef TSO
#error "compile with -DTSO=1 or -DTSO=0"
#endif
static void enable_tso()
{
	int enable = TSO;
	size_t size = sizeof(enable);
	int err = sysctlbyname("kern.tso_enable", NULL, &size, &enable, size);
	assert(err == 0);
}

static unsigned int x;
static void *run(void *arg)
{
	(void) arg;
	enable_tso();
	for (int i=0; i < NITERS; i++) {
		lock_acquire();
		x++;
		lock_release();
	}
	return 0;
}

int main()
{
	pthread_t t;
	pthread_create(&t, 0, run, 0);
	run(0);
	pthread_join(t, 0);
	assert(x == 2*NITERS && "TSO is not enabled");
	return 0;
}
