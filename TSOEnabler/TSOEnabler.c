//
//  TSOEnabler.c
//  TSOEnabler
//
//  Created by Saagar Jha on 7/29/20.
//

#include <mach/mach_types.h>
#include <libkern/libkern.h>
#include <stddef.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#define TSO_OFFSET 0x4e8

#define unslid_printf 0xfffffe000724ccb0
#define unslid_thread_bind_cluster_type 0xfffffe000725d7c0

static void (*thread_bind_cluster_type)(thread_t, char);

static char *get_thread_pointer(void) {
	char *pointer = NULL;
	// Yes, a mrs x0, tpidr_el1; ret would work, but I'm trying to minimize inline assembly
	__asm__ volatile("mrs %0, tpidr_el1": "=r"(pointer)::);
	return pointer;
}

static int sysctl_tso_enable SYSCTL_HANDLER_ARGS {
	printf("TSOEnabler: got request from %d\n", proc_selfpid());
	
	char *thread_pointer = get_thread_pointer();
	if (!thread_pointer) {
		return KERN_FAILURE;
	}
	
	int in;
	int error = SYSCTL_IN(req, &in, sizeof(in));
	
	// Write to TSO
	if (!error && req->newptr) {
		printf("TSOEnabler: setting TSO to %d\n", in);
		if ((thread_pointer[TSO_OFFSET] = in)) {
			printf("TSOEnabler: binding to performance core\n");
			thread_bind_cluster_type(current_thread(), 'P');
		}
	// Read TSO
	} else if (!error) {
		int out = thread_pointer[TSO_OFFSET];
		printf("TSOEnabler: TSO is %d\n", out);
		error = SYSCTL_OUT(req, &out, sizeof(out));
	}
	
	if (error) {
		printf("TSOEnabler: request failed with error %d\n", error);
	}
	
	return error;
}

SYSCTL_PROC(_kern, OID_AUTO, tso_enable, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, NULL, 0, &sysctl_tso_enable, "I", "Enable TSO");

kern_return_t TSOEnabler_start(kmod_info_t *ki, void *d) {
	printf("TSOEnabler: TSOEnabler_start()\n");
	sysctl_register_oid(&sysctl__kern_tso_enable);
	// Find thread_bind_cluster_type without PAC yelling at us
	uintptr_t unauthenticated_printf = (uintptr_t)ptrauth_strip((void *)printf, ptrauth_key_function_pointer);
	thread_bind_cluster_type = (void (*)(thread_t, char))ptrauth_sign_unauthenticated((void *)(unauthenticated_printf + (unslid_thread_bind_cluster_type - unslid_printf)), ptrauth_key_function_pointer, 0);
	printf("TSOEnabler: found thread_bind_cluster_type at %p\n", thread_bind_cluster_type);
	return KERN_SUCCESS;
}

kern_return_t TSOEnabler_stop(kmod_info_t *ki, void *d) {
	sysctl_unregister_oid(&sysctl__kern_tso_enable);
	printf("TSOEnabler: TSOEnabler_stop()\n");
	return KERN_SUCCESS;
}
