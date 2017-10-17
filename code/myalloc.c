/*	Stuart Norcross - 12/03/10 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "myalloc.h"

#define min_size 16			//must be large enough to store the list pointers in

typedef struct block_info {
  struct block_info *next;
  struct block_info *prev;
} block_info;

typedef struct tag {
	size_t size;
} tag;

block_info *head;

void *myalloc(int size){
	if (!size) return NULL;

	if (size < min_size) {
		size = min_size;
	}

	if (head) {
		block_info *curr = head;
		//printf("block info: %p\n", curr);
		while (curr) {
			tag *bloc_size = (char*)curr - sizeof(size_t);
			//printf("tag: %p\n", bloc_size);
			if (bloc_size->size >= size) {
				return (void*) (curr);
			}
			curr = curr->next;
		}
	}

	tag *head_tag;
	tag *foot_tag;

	size_t actual_size = sizeof(size_t) + size + sizeof(size_t);

	void* block = mmap(NULL, (actual_size), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (block == (void *) -1) {
		printf("%s\n", strerror(errno));
		return NULL;
	}
	head_tag = block;
	foot_tag = (char*) block + (actual_size - sizeof(size_t));
	void* block_start = (char*) head_tag + sizeof(size_t);

	head_tag->size = size;
	foot_tag->size = size;

	return block_start;
}

void myfree(void *ptr){
	block_info *temp;
	temp = ptr;
	if (!head) {
		temp->next = NULL;
		temp->prev = NULL;
		head = temp;
	} else {
		temp->next = head;
		temp->prev = NULL;
		head->prev = temp;
		head = temp;
	}
	//printf("%p\n", head);
}
