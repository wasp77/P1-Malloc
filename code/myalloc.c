/*	Stuart Norcross - 12/03/10 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "myalloc.h"

#define word_size 8
#define min_size 32			//must be large enough to store the list pointers in
#define page_size 4096

typedef struct block_info {
  struct block_info *next;
  struct block_info *prev;
} block_info;

typedef struct tag {
	size_t size;
	unsigned int free;
} tag;

block_info *head;
unsigned int init = 0;

void *myalloc(int size){
	if (!size) return NULL;
	tag *start_tag;
	tag *head_tag;
	block_info *head_info;

	if (size < (word_size * 2)) {
		size = (word_size * 2);
	}

  if (!init) {
    init = 1;
    size_t actual_size = (word_size * 2) + size;
    void* block = mmap(NULL, page_size * 100, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (block == (void *) -1) {
      printf("%s\n", strerror(errno));
      return NULL;
    }

    start_tag = block;
    void* block_start = (char*) start_tag + (word_size * 2);
    start_tag->size = (page_size * 100) - (word_size * 2);
    start_tag->free = 0;

    if (actual_size < start_tag->size) {
      if (start_tag->size - actual_size >= min_size) {
        head_tag = (char*)block + actual_size;
        head_tag->size = start_tag->size - actual_size;
        head_tag->free = 1;
        head_info = (char*)head_tag + (word_size * 2);
        head_info->next = NULL;
        head_info->prev = NULL;
        head = head_info;
        start_tag->size = size;
      }
      return block_start;
    } else {
      return NULL;
    }
  }

	block_info *curr = head;
	while (curr) {
		start_tag = (char*)curr - (word_size * 2);
		if (start_tag->size >= size) {
			if ((start_tag->size - size) >= min_size) {
				head_tag = (char*)curr + size;
				head_tag->size = start_tag->size - size;
				head_tag->free = 1;
				head_info = (char*)head_tag + (word_size * 2);
				start_tag->size = size;
				if (curr->next) {
					head_info->next = curr->next;
					curr->next->prev = head_info;
				} else {
					head_info->next = NULL;
				}
				if (curr->prev) {
					head_info->prev = curr->prev;
					curr->prev->next = head_info;
				} else {                           //Nothing previous means it is the head
					head_info->prev = NULL;
          head = head_info;
				}
			} else {
        if (curr->next && curr->prev) {
          curr->next->prev = curr->prev;
          curr->prev->next = curr->next;
        } else if (curr->next && !curr->prev) {
          curr->next->prev = NULL;
          head = curr->next;
        } else if (!curr->next && curr->prev) {
          curr->prev->next = NULL;
        } else {
          printf("we screwed up \n");
          return NULL;
        }
      }
      //printf("%p\n", curr);
			return (void*) (curr);
		}
		curr = curr->next;
	}
  printf("not enough memory\n");
  return NULL;
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
}
