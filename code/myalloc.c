#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include "myalloc.h"

#define word_size 8
#define tag_size 16     //Two words in a tag
#define min_size 48			//must be large enough to store the list pointers plus header and footer
#define page_size 4096

typedef struct block_info {
  struct block_info *next;
  struct block_info *prev;
} block_info;

typedef struct tag {
	size_t size;
	unsigned int free;
} tag;

pthread_mutex_t malloc_lock;
block_info *head;             //Holds the head of the explicit list
void* mem_start;              //Used to check if a free block is at the start of memory
void* mem_end;                //Used to check if a free block is at the end of memory
unsigned int init = 0;

void split(tag *current_head, tag *current_foot, tag *next_head, tag *next_foot, block_info *current_info, block_info *next_info, int size);
void removeLinks (block_info *link_ptrs);

void *myalloc(int size){
	if (!size) return NULL;
	tag *start_tag;
  tag *start_foot;
	tag *head_tag;
  tag *foot_tag;
	block_info *head_info;
  size_t masked_size;
  size_t isfree;

  //Make sure the size is large enough to fit the two pointers
	if (size < tag_size) {
		size = tag_size;
	}

  //Make sure the size is a multiple of the word size for alignment purposes
  if (size % word_size) {
    size = size + (word_size - (size % word_size));
  }

  pthread_mutex_lock(&malloc_lock);
  if (!init) {
    init = 1;
    size_t actual_size = (tag_size * 2) + size;
    void* block = mmap(NULL, page_size * 4000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (block == (void *) -1) {
      pthread_mutex_unlock(&malloc_lock);
      printf("%s\n", strerror(errno));
      return NULL;
    }

    mem_start = block;
    mem_end = (char*)block + page_size * 4000;
    start_tag = block;
    void* block_start = (char*)block + tag_size;
    start_tag->size = (page_size * 4000) - (tag_size * 2);
    //start_tag->free = 0;

    if (start_tag->size - actual_size >= min_size) {
      head_tag = (char*)block + actual_size;
      head_tag->size = start_tag->size - actual_size;
      head_tag->size = head_tag->size | 1;              //Set head_tag to free
      //head_tag->free = 1;
      head_info = (char*)head_tag + tag_size;
      head_info->next = NULL;
      head_info->prev = NULL;
      head = head_info;
      start_tag->size = size;
      start_foot = (char*)head_tag - tag_size;
      start_foot->size = size;
      //start_foot->free = 0;
    }
    pthread_mutex_unlock(&malloc_lock);
    return block_start;
  }

	block_info *curr = head;
	while (curr) {
		start_tag = (char*)curr - tag_size;
    masked_size = start_tag->size & ~1;               //Mask out last bit
    start_foot = (char*)curr + masked_size;
		if (masked_size >= size) {
			if ((masked_size - size) >= min_size) {
        split(start_tag, start_foot, head_tag, foot_tag, curr, head_info, size);
			} else {
        removeLinks(curr);
      }
      pthread_mutex_unlock(&malloc_lock);
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
  size_t masked_size;

  pthread_mutex_lock(&malloc_lock);
  //Get the three structs for the current block
  current_head = (char*)ptr - tag_size;
  current_foot = (char*)ptr + current_head->size;
  current_info = ptr;
  //Check if the current block is at the start or end of memory
  if (current_head == mem_start) {
    next_tag = (char*)current_foot + tag_size;
    masked_size = next_tag->size & ~1;
    next_info = (char*)next_tag + tag_size;
    if (next_tag->size & 1) {
      total_size = current_head->size + masked_size + (tag_size * 2);
      removeLinks(next_info);
      current_head->size = total_size;
      current_foot = (char*)next_info + masked_size;
      current_foot->size = total_size;
    }
  } else if (((char*)current_foot + tag_size) == mem_end) {
    prev_tag = (char*)current_head - tag_size;
    masked_size = prev_tag->size & ~1;
    prev_info = (char*)prev_tag - masked_size;
    if (prev_tag->size & 1) {
      total_size = current_head->size + masked_size + (tag_size * 2);
      removeLinks(prev_info);
      current_head = (char*)prev_info - tag_size;
      current_head->size = total_size;
      current_foot->size = total_size;
      current_info = prev_info;
    }
  } else {
    next_tag = (char*)current_foot + tag_size;
    next_info = (char*)next_tag + tag_size;
    prev_tag = (char*)current_head - tag_size;
    prev_info = (char*)prev_tag - (prev_tag->size & ~1);
    if (prev_tag->size & 1 && next_tag->size & 1) {
      total_size = current_head->size + (prev_tag->size & ~1) + (next_tag->size & ~1) + (tag_size * 4);
      removeLinks(prev_info);
      removeLinks(next_info);
      current_head = (char*)prev_info - tag_size;
      current_head->size = total_size;
      current_foot->size = total_size;
      current_info = (char*)prev_info;
    } else if (prev_tag->size & 1 && !(next_tag->size & 1)) {
      total_size = current_head->size + (prev_tag->size & ~1) + (tag_size * 2);
      removeLinks(prev_info);
      current_head = (char*)prev_info - tag_size;
      current_head->size = total_size;
      current_foot->size = total_size;
      current_info = (char*)prev_info;
    } else if (!(prev_tag->size & 1) && next_tag->size & 1) {
      total_size = current_head->size + (next_tag->size & ~1) + (tag_size * 2);
      removeLinks(next_info);
      current_head->size = total_size;
      current_foot = (char*)next_info + (next_tag->size & ~1);
      current_foot->size = total_size;
    }
  }
  current_head->size = current_head->size | 1;
  //current_head->free = 1;
  current_foot->size = current_foot->size | 1;
  //current_foot->free = 1;
  current_info->next = head;
  current_info->prev = NULL;
  head->prev = current_info;
  head = current_info;
  pthread_mutex_unlock(&malloc_lock);
}

void split(tag *current_head, tag *current_foot, tag *next_head, tag *next_foot, block_info *current_info, block_info *next_info, int size) {
  size_t masked_size = current_head->size & ~1;
  next_head = (char*)current_info + (size + tag_size);
  next_head->size = masked_size - (size + tag_size * 2);
  next_head->size = next_head->size | 1;
  //next_head->free = 1;
  next_foot = current_foot;
  next_foot->size = next_head->size;
  //next_foot->size = next_foot->size | 1;
  //next_foot->free = 1;
  current_head->size = size;
  //current_head->size = current_head->size ^ 1;
  //current_head->free = 0;
  current_foot = (char*)next_head - tag_size;
  current_foot->size = size;
  //current_foot->size = current_foot->size ^ 1;
  //current_foot->free = 0;
  next_info = (char*)next_head + tag_size;
  if (current_info->next) {
    next_info->next = current_info->next;
    current_info->next->prev = next_info;
  } else {
    next_info->next = NULL;
  }
  if (current_info->prev) {
    next_info->prev = current_info->prev;
    current_info->prev->next = next_info;
  } else {                           //Nothing previous means it is the head
    next_info->prev = NULL;
    head = next_info;
  }
}

void removeLinks (block_info *link_ptrs) {
  if (link_ptrs->next && link_ptrs->prev) {
    link_ptrs->next->prev = link_ptrs->prev;
    link_ptrs->prev->next = link_ptrs->next;
  } else if (link_ptrs->next && !link_ptrs->prev) {
    link_ptrs->next->prev = NULL;
    head = link_ptrs->next;
  } else if (!link_ptrs->next && link_ptrs->prev) {
    link_ptrs->prev->next = NULL;
  }
}
