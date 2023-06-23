#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)     ((b) + 1)
#define BLOCK_HEADER(ptr) ((struct _block *)(ptr) - 1)

static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit.  Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;         /* Size of the allocated _block of memory in bytes */
   struct _block *next;  /* Pointer to the next _block of allcated memory   */
   bool   free;          /* Is this _block free?                            */
   char   padding[3];    /* Padding: IENTRTMzMjAgU3ByaW5nIDIwMjM            */
};


struct _block *heapList = NULL; /* Free list to track the _blocks available */
struct _block *nextFitPT = NULL; /* Global to keep track of NextFit pointer */

/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 * \TODO Implement Next Fit
 */
struct _block *findFreeBlock(struct _block **last, size_t size) 
{
   struct _block *curr = heapList;

#if defined FIT && FIT == 0
   /* First fit */
   //
   // While we haven't run off the end of the linked list and
   // while the current node we point to isn't free or isn't big enough
   // then continue to iterate over the list.  This loop ends either
   // with curr pointing to NULL, meaning we've run to the end of the list
   // without finding a node or it ends pointing to a free node that has enough
   // space for the request.
   // 
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }
#endif

#if defined BEST && BEST == 0
   /* Best fit */
   //
   // While we haven't run off the end of the linked list and
   // while the current node we point to isn't free or isn't big enough
   // then continue to iterate over the list.  This loop ends either
   // with curr pointing to NULL, meaning we've run to the end of the list
   // without finding a node or it ends pointing to a free node that has enough
   // space for the request.
   //
   // Once a node with enough space has been found, the difference between
   // the free _block size and the requested size is calculated and compared 
   // to other free _blocks with enough space. The winner is the _block with
   // the SMALLEST remaining size.
   //
   struct _block *winner = NULL;
   int winning_remain = INT_MAX;

   while (curr != NULL)
   {
      if ((curr->free && curr->size >= size))
      {
         if ((curr->size - size) < winning_remain)
         {
            winning_remain = (curr->size - size);
            winner = curr;
         }
      }

      *last = curr;
      curr  = curr->next;
   }

   curr = winner;
#endif

#if defined WORST && WORST == 0
   /* Worst fit */
   //
   // While we haven't run off the end of the linked list and
   // while the current node we point to isn't free or isn't big enough
   // then continue to iterate over the list.  This loop ends either
   // with curr pointing to NULL, meaning we've run to the end of the list
   // without finding a node or it ends pointing to a free node that has enough
   // space for the request.
   //
   // Once a node with enough space has been found, the difference between
   // the free _block size and the requested size is calculated and compared 
   // to other free _blocks with enough space. The winner is the _block with
   // the LARGEST remaining size.
   //
   struct _block *winner = NULL;
   int winning_remain = 0;

   while (curr != NULL) //Iterates through list while curr is not NULL
   {
      if ((curr->free && curr->size >= size))
      {
         if ((curr->size - size) > winning_remain)
         {
            winning_remain = (curr->size - size);
            winner = curr;
         }
      }
      *last = curr;
      curr  = curr->next;
   }

   curr = winner;
#endif

// \TODO Put your Next Fit code in this #ifdef block
#if defined NEXT && NEXT == 0
   /* If nextFitPT == NULL, start at beginning of list and implement like FirstFit */
   if (nextFitPT == NULL)
   {
      while (curr && !(curr->free && curr->size >= size)) 
      {
         *last = curr;
         curr  = curr->next;
      }
      nextFitPT = curr;
   }

   else 
   {
      /* Benchmark pointer to stop at if we loop back around from middle of linked list */
      struct _block *benchmark = nextFitPT;
      
      curr = nextFitPT;
      while (curr && !(curr->free && curr->size >= size))
      {
         *last = curr;
         curr  = curr->next;
      }
      nextFitPT = curr;

      /* If curr == NULL, reached the bottom of the list and need to start from top*/
      if (curr == NULL)
      {
         curr = heapList;
         while ((curr != benchmark) && !(curr->free && curr->size >= size))
         {
            *last = curr;
            curr  = curr->next;
         }
         nextFitPT = curr;
      }
   }
#endif

   return curr;
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size) 
{
   /* Request more space from OS */
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   /* OS allocation failed */
   if (curr == (struct _block *)-1) 
   {
      return NULL;
   }

   /* Update heapList if not set */
   if (heapList == NULL) 
   {
      heapList = curr;
   }

   /* Attach new _block to previous _block */
   if (last) 
   {
      last->next = curr;
   }

   /* Update _block metadata:
      Set the size of the new block and initialize the new block to "free".
      Set its next pointer to NULL since it's now the tail of the linked list.
   */
   curr->size = size;
   curr->next = NULL;
   curr->free = false;

   max_heap = max_heap + size + sizeof(struct _block);
   num_grows++;
   num_blocks++;
   return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size) 
{
   if( atexit_registered == 0 )
   {
      atexit_registered = 1;
      atexit( printStatistics );
   }

   /* Align to multiple of 4 */
   size = ALIGN4(size);
   num_requested = num_requested + size;

   /* Handle 0 size */
   if (size == 0) 
   {
      return NULL;
   }

   /* Look for free _block.  If a free block isn't found then we need to grow our heap. */

   struct _block *last = heapList;
   struct _block *next = findFreeBlock(&last, size);

   if (next) //a FreeBlock was found
   {
      int remainder = (next->size - size);
      if (remainder >= (sizeof(struct _block)+4))
      {   
         //Split block
         char * c = (char*) next;
         struct _block *newblock = (struct _block*)(c + size + sizeof(struct _block)); //Assign pointer to new block
         newblock->size = (next->size - size) - sizeof(struct _block);
         newblock->free = true;
         newblock->next = next->next;

         next->size = size;
         next->next = newblock;
         num_splits++;
         num_blocks++;
      }
      num_reuses++;
   }

   /* Could not find free _block, so grow heap */
   if (next == NULL) 
   {
      next = growHeap(last, size);
   }

   /* Could not find free _block or grow heap, so just return NULL */
   if (next == NULL) 
   {
      return NULL;
   }
   
   /* Mark _block as in use */
   next->free = false;

   /* Return data address associated with _block to the user */
   num_mallocs++;
   return BLOCK_DATA(next);
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr) 
{
   if (ptr == NULL) 
   {
      return;
   }

   /* Make _block as free */
   struct _block *curr = BLOCK_HEADER(ptr);
   assert(curr->free == 0);
   curr->free = true;
   
   struct _block *nextblock = curr->next;
   if (nextblock != NULL && nextblock->free)
   {
      curr->next = nextblock->next;
      curr->size = curr->size + (nextblock->size + sizeof(struct _block));
      num_coalesces++;
      num_blocks--;
   }

   struct _block *check = heapList;
   while(check != NULL && check->next != NULL)
   {
      struct _block *checkNext = check->next;
      if (check->free && checkNext->free)
      {
         check->next = checkNext->next;
         check->size = check->size + (checkNext->size + sizeof(struct _block));
         num_coalesces++;
         num_blocks--;
      }
      check = checkNext;
   }
   
   num_frees++;
}

void *calloc( size_t nmemb, size_t size )
{
   size_t sizeRequested = nmemb * size;
   return memset(malloc(sizeRequested), 0, sizeRequested);
}

void *realloc( void *ptr, size_t size )
{
   if (size == 0)
   {
      if (ptr != NULL)
      {
         free(ptr);
      }
      return NULL;
   }

   if (ptr == NULL)
   {
      return malloc(size);
   }

   void* mpointer = malloc(size);
   struct _block *temp = BLOCK_HEADER(ptr);
   memcpy(mpointer, ptr, temp->size);
   free(ptr);

   return mpointer;
}

/* vim: IENTRTMzMjAgU3ByaW5nIDIwMjM= -----------------------------------------*/
/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/
