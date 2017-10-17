/*	Stuart Norcross - 12/03/10 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "myalloc.h"

typedef struct header_block {
  size_t size;
  unsigned is_free;
  struct header_block *next;
  //struct header_block *prev;
} header;

header *head, *tail;
size_t footer;

void *myalloc(int size){
	if (!size) return NULL;
	header *new_block;
	size_t header_size = sizeof(header);
	//printf("header size: %zu\n", header_size);
	if (head) {
		header *curr = head;
		while (curr) {
			if (curr->is_free && curr->size >= size) {
				new_block = curr;
				new_block->is_free = 0;
				return (void*) (new_block + 1);
			}
			curr = curr->next;
		}
	}
	void* block = mmap(NULL, (header_size + size), PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (block == (void *) -1) {
		printf("%s\n", strerror(errno));
		return NULL;
	}
	new_block = block;
	new_block->size = size;
	new_block->is_free = 0;
	new_block->next = NULL;
	if (!head) {
		head = new_block;
	}
	if (tail) {
		tail->next = new_block;
		tail = new_block;
	}
	return (void*) (new_block + 1);
}

void myfree(void *ptr){
	header *free_header = (header*) ptr - 1;
	free_header->is_free = 1;
	if (free_header->next) {
		if (free_header->next->is_free == 1) {
			free_header->size = free_header->size + sizeof(header) + free_header->next->size;
			free_header->next = free_header->next->next;
		}
	}
}
