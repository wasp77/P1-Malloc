#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "myalloc.h"

#define word_size 8
#define tag_size 16
#define min_size 48			//must be large enough to store the list pointers in
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
  tag *start_foot;
	tag *head_tag;
  tag *foot_tag;
	block_info *head_info;

	if (size < (tag_size)) {
		size = (tag_size);
	}

  if (!init) {
    init = 1;
    size_t actual_size = (tag_size * 2) + size;
    void* block = mmap(NULL, page_size * 5000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (block == (void *) -1) {
      printf("%s\n", strerror(errno));
      return NULL;
    }

    start_tag = block;
    void* block_start = (char*) start_tag + (tag_size);
    start_tag->size = (page_size * 5000) - (tag_size * 2);
    start_tag->free = 0;

    if (start_tag->size - actual_size >= min_size) {
      head_tag = (char*)block + actual_size;
      head_tag->size = start_tag->size - actual_size;
      head_tag->free = 1;
      head_info = (char*)head_tag + tag_size;
      head_info->next = NULL;
      head_info->prev = NULL;
      head = head_info;
      start_tag->size = size;
      start_foot = (char*)head_tag - tag_size;
      start_foot->size = size;
      start_foot->free = 0;
    }
    //printf("Initialised %p\n", );
    return block_start;
  }

	block_info *curr = head;
	while (curr) {
		start_tag = (char*)curr - tag_size;
    start_foot = (char*)curr + start_tag->size;
		if (start_tag->size >= size) {
			if ((start_tag->size - size) >= min_size) {
				head_tag = (char*)curr + (size + tag_size);
				head_tag->size = start_tag->size - (size + tag_size * 2);
				head_tag->free = 1;
        foot_tag = start_foot;
        foot_tag->size = head_tag->size;
        foot_tag->free = 1;
				start_tag->size = size;
        start_tag->free = 0;
        start_foot = (char*)head_tag - tag_size;
        start_foot->size = size;
        start_foot->free = 0;
        head_info = (char*)head_tag + tag_size;
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
			return (void*) (curr);
		}
		curr = curr->next;
	}
  printf("not enough memory\n");
  return NULL;
}

void myfree(void *ptr){
	block_info *current_info;
  block_info *prev_info;
  block_info *next_info;
  tag *current_head;
  tag *current_foot;
  tag *next_tag;
  tag *prev_tag;
  size_t total_size;
  current_head = (char*)ptr - tag_size;
  current_foot = (char*)current_head + tag_size + current_head->size;
  current_info = ptr;
  prev_tag = (char*)current_head - tag_size;                             //Get the foot tag of the previous block
  prev_info = (char*)prev_tag - prev_tag->size;
  next_tag = (char*)current_foot + tag_size;                             //Get the head tag of the next block
  next_info = (char*)next_tag + tag_size;

  if (prev_tag->free && next_tag->free) {
    total_size = current_head->size + prev_tag->size + next_tag->size + (tag_size * 4);
    current_info = (char*)ptr - ((tag_size * 2) + prev_tag->size);
  } else if (prev_tag->free && !next_tag->free) {
    total_size = current_head->size + prev_tag->size + (tag_size * 2);
    prev_info->prev->next = current_info->next;
    current_info->next->prev = prev_info->prev;
    current_info = (char*)ptr - ((tag_size * 2) + prev_tag->size);
    current_info->next = head;
    current_info->prev = NULL;
    head->prev = current_info;
    head = current_info;
  } else if (!prev_tag->free && next_tag->free) {
    total_size = current_head->size + next_tag->size + (tag_size * 2);
    current_info->prev->next = next_info->next;
    next_info->next->prev = current_info->prev;
    current_info->next = head;
    current_info->prev = NULL;
    head->prev = current_info;
    head = current_info;
  } else {
    current_info = ptr;
    current_info->next = head;
    current_info->prev = NULL;
    head->prev = current_info;
    head = current_info;
  }

}
