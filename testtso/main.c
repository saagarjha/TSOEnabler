//
//  main.c
//  testtso
//
//  Created by Saagar Jha on 5/28/21.
//

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

atomic_uint barrier;
atomic_uint data[10000];

void *writer(void *unused) {
	(void)unused;
	while (true) {
		for (size_t i = 0; i < sizeof(data) / sizeof(*data); ++i) {
			atomic_fetch_add_explicit(data + i, 1, memory_order_relaxed);
		}
		atomic_fetch_add_explicit(&barrier, 1, memory_order_release);
	}
	return NULL;
}

void *reader(void *unused) {
	(void)unused;
	unsigned int count = 0;
	while (true) {
		for (size_t i = 0; i < sizeof(data) / sizeof(*data) - 1; ++i) {
			unsigned int value2 = atomic_load_explicit(data + i + 1, memory_order_relaxed);
			unsigned int value1 = atomic_load_explicit(data + i, memory_order_relaxed);
			if (value1 < value2) {
				exit(0);
			}
		}

		while (count == atomic_load_explicit(&barrier, memory_order_acquire))
			;
		++count;
	}
	return NULL;
}

int main() {
	pthread_t writer_thread;
	pthread_t reader_thread;
	pthread_create(&writer_thread, NULL, writer, NULL);
	pthread_create(&reader_thread, NULL, reader, NULL);
	pthread_join(reader_thread, NULL);
	pthread_join(writer_thread, NULL);
}
