//
//  enabletso.c
//  enabletso
//
//  Created by Saagar Jha on 5/28/21.
//

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <sys/sysctl.h>

__attribute__((constructor)) static void set_tso() {
	int new = 1;
	sysctlbyname("kern.tso_enable", NULL, NULL, &new, sizeof(new));
}

typedef struct {
	void *(*function)(void *);
	void *argument;
} pthread_context;

static pthread_context context_arena[4096 /* ought to be enough for anybody */];
static atomic_size_t arena_top = sizeof(context_arena) / sizeof(*context_arena);

static void *set_tso_trampoline(void *argument) {
	set_tso();
	pthread_context *context = (pthread_context *)argument;
	return context->function(context->argument);
}

int overriden_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
	size_t context;
	if ((context = --arena_top)) {
		context_arena[context] = (pthread_context){start_routine, arg};
		return pthread_create(thread, attr, set_tso_trampoline, context_arena + context);
	} else {
		return EAGAIN;
	}
}

__attribute__((used, section("__DATA,__interpose"))) static struct {
	int (*overriden_pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
	int (*pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
} pthread_create_interpose = {overriden_pthread_create, pthread_create};
