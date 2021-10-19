//
//  TSOEnabler.c
//  TSOEnabler
//
//  Created by Saagar Jha on 7/29/20.
//

#include <libkern/libkern.h>
#include <mach-o/loader.h>
#include <mach/mach_types.h>
#include <ptrauth.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

__asm__(
	".global _TSOEnabler_get_thread_pointer\n"
	"_TSOEnabler_get_thread_pointer:\n"
	"mrs x0, tpidr_el1\n"
	"ret");

uintptr_t TSOEnabler_get_thread_pointer(void);

static int tso_offset = -1;

static int find_tso_offset_in_text_exec(char *text_exec, uint64_t text_exec_size) {
	// Look for this sequence:
	// ldr xR, [xR, #OFFSET]
	// cbz xR, location
	// mrs xR, actlr_el1
	// xR, xR, #0x2
	// xR, xR, #0x70
	uint32_t *instructions = (uint32_t *)text_exec;
	for (uint64_t i = 0; i < text_exec_size / sizeof(uint32_t) - 5; ++i) {
		if ((instructions[i + 0] & 0xffc00000) == 0xf9400000 &&
			(instructions[i + 1] & 0x2f000000) == 0x24000000 &&
			(instructions[i + 2] & 0xffffffe0) == 0xd5381020 &&
			(instructions[i + 3] & 0xfffffc00) == 0xb27f0000 &&
			(instructions[i + 4] & 0xfffffc00) == 0xb27c0800) {
			printf("TSOEnabler: Found thread pointer read at %p\n", instructions + i);
			// Extract the immediate from the ldr.
			int tso_offset = (instructions[i] >> 10 & 0xfff) << 3;
			printf("TSOEnabler: TSO offset is %d\n", tso_offset);
			return tso_offset;
		}
	}
	return -1;
}

static int find_tso_offset(void) {
	// Find the kernel base. First, get the address of a function in __TEXT_EXEC:
	uintptr_t unauthenticated_printf = (uintptr_t)ptrauth_strip((void *)printf, ptrauth_key_function_pointer);
	// Then, iterate backwards to find the kernel Mach-O header.
	uintptr_t address = unauthenticated_printf & ~(PAGE_SIZE - 1);
	uint32_t magic;
	while ((void)memcpy(&magic, (void *)address, sizeof(magic)), magic != MH_MAGIC_64) {
		address -= PAGE_SIZE;
	}

	printf("TSOEnabler: found kernel base at %p\n", (void *)address);

	ptrdiff_t slide = -1;
	uintptr_t text_exec_base = 0;
	uint64_t text_exec_size = 0;
	struct mach_header_64 *header = (struct mach_header_64 *)address;
	address += sizeof(*header);
	for (int i = 0; i < header->ncmds; ++i) {
		struct load_command *command = (struct load_command *)address;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64 *segment = (struct segment_command_64 *)command;
			if (!strcmp(segment->segname, "__TEXT")) {
				slide = (uintptr_t)header - segment->vmaddr;
			} else if (!strcmp(segment->segname, "__TEXT_EXEC")) {
				text_exec_base = segment->vmaddr;
				text_exec_size = segment->vmsize;
			}
		}
		address += command->cmdsize;
	}

	if (slide < 0 || !text_exec_base) {
		return -1;
	}

	char *text_exec = (char *)(text_exec_base + slide);

	printf("TSOEnabler: kernel slide is 0x%llx, and __TEXT_EXEC is at %p\n", (long long)slide, text_exec);

	return find_tso_offset_in_text_exec(text_exec, text_exec_size);
}

static int sysctl_tso_enable SYSCTL_HANDLER_ARGS {
	printf("TSOEnabler: got request from %d\n", proc_selfpid());

	char *thread_pointer = (char *)TSOEnabler_get_thread_pointer();
	if (!thread_pointer) {
		return KERN_FAILURE;
	}

	int in;
	int error = SYSCTL_IN(req, &in, sizeof(in));

	// Write to TSO
	if (!error && req->newptr) {
		printf("TSOEnabler: setting TSO to %d\n", in);
		(thread_pointer[tso_offset] = in);
		// Read TSO
	} else if (!error) {
		int out = thread_pointer[tso_offset];
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
	tso_offset = find_tso_offset();
	if (tso_offset >= 0) {
		sysctl_register_oid(&sysctl__kern_tso_enable);
	} else {
		printf("TSOEnabler: couldn't find TSO offset!");
	}
	return KERN_SUCCESS;
}

kern_return_t TSOEnabler_stop(kmod_info_t *ki, void *d) {
	if (tso_offset >= 0) {
		sysctl_unregister_oid(&sysctl__kern_tso_enable);
	}
	printf("TSOEnabler: TSOEnabler_stop()\n");
	return KERN_SUCCESS;
}
