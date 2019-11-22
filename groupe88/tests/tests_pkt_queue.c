#include <stdio.h>
#include <string.h>
#include "CUnit/Basic.h"

#include "../src/packet_interface.h"
#include "../src/pkt_queue_interface.h"


/* TESTS DE LA LIBRAIRE PKT_QUEUE */

/* The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
int init_suite1(void)
{
   return 0;
}

/* The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
int clean_suite1(void)
{
   return 0;
}

void test_get_first_seqnum(void)
{
  pkt_t* pkt1 = pkt_new();
  pkt_t* pkt2 = pkt_new();
  pkt_t* pkt3 = pkt_new();

  pkt_queue_t* queue = calloc(1,sizeof(pkt_queue_t));
  queue->firstNode = NULL;

  pkt_set_type(pkt1,1);
  pkt_set_type(pkt2,1);
  pkt_set_type(pkt3,1);

  pkt_set_tr(pkt1,0);
  pkt_set_tr(pkt2,0);
  pkt_set_tr(pkt3,0);

  pkt_set_seqnum(pkt1,0);
  pkt_set_seqnum(pkt2,1);
  pkt_set_seqnum(pkt3,2);

  pkt_set_payload(pkt1,"bjr1",sizeof("bjr1"));
  pkt_set_payload(pkt2,"bjr2",sizeof("bjr1"));
  pkt_set_payload(pkt3,"bjr3",sizeof("bjr1"));

  add(queue,pkt2);
  CU_ASSERT(1 == pkt_get_seqnum(queue->firstNode->item));

  add(queue,pkt3);
  CU_ASSERT(1 == pkt_get_seqnum(queue->firstNode->item));

  add(queue,pkt1);

  CU_ASSERT(0 == pkt_get_seqnum(queue->firstNode->item)); 
}

void test_pop(void)
{
  pkt_t* pkt1 = pkt_new();
  pkt_t* pkt2 = pkt_new();
  pkt_t* pkt3 = pkt_new();

  pkt_queue_t* queue = calloc(1,sizeof(pkt_queue_t));
  queue->firstNode = NULL;

  pkt_set_type(pkt1,1);
  pkt_set_type(pkt2,1);
  pkt_set_type(pkt3,1);

  pkt_set_tr(pkt1,0);
  pkt_set_tr(pkt2,0);
  pkt_set_tr(pkt3,0);

  pkt_set_seqnum(pkt1,0);
  pkt_set_seqnum(pkt2,1);
  pkt_set_seqnum(pkt3,2);

  pkt_set_payload(pkt1,"bjr1",sizeof("bjr1"));
  pkt_set_payload(pkt2,"bjr2",sizeof("bjr1"));
  pkt_set_payload(pkt3,"bjr3",sizeof("bjr1"));

  add(queue,pkt1);
  add(queue,pkt2);
  add(queue,pkt3);

  pkt_t * pkt3bis = pop(queue);
  CU_ASSERT(0 == pkt_get_seqnum(pkt3bis)); //ici

  pkt_t * pkt2bis = pop(queue);
  CU_ASSERT(1 == pkt_get_seqnum(pkt2bis));

  pkt_t * pkt1bis = pop(queue);
  CU_ASSERT(2 == pkt_get_seqnum(pkt1bis));  //ici
}

void test_add(void)
{
  pkt_t* pkt1 = pkt_new();
  pkt_t* pkt2 = pkt_new();
  pkt_t* pkt3 = pkt_new();

  pkt_queue_t* queue = calloc(1,sizeof(pkt_queue_t));
  queue->firstNode = NULL;

  pkt_set_type(pkt1,1);
  pkt_set_type(pkt2,1);
  pkt_set_type(pkt3,1);

  pkt_set_tr(pkt1,0);
  pkt_set_tr(pkt2,0);
  pkt_set_tr(pkt3,0);

  pkt_set_seqnum(pkt1,0);
  pkt_set_seqnum(pkt2,1);
  pkt_set_seqnum(pkt3,2);

  pkt_set_payload(pkt1,"bjr1",sizeof("bjr1"));
  pkt_set_payload(pkt2,"bjr2",sizeof("bjr1"));
  pkt_set_payload(pkt3,"bjr3",sizeof("bjr1"));

  add(queue,pkt2);
  CU_ASSERT(1 == queue->nNodes);
  CU_ASSERT(1 == pkt_get_seqnum(queue->firstNode->item));

  add(queue,pkt3);
  CU_ASSERT(2 == queue->nNodes);
  CU_ASSERT(2 == pkt_get_seqnum(queue->firstNode->next->item));

  add(queue,pkt1);
  CU_ASSERT(3 == queue->nNodes);
  CU_ASSERT(0 == pkt_get_seqnum(queue->firstNode->item)); 

  add(queue,pkt1);
  CU_ASSERT(3 == queue->nNodes); 
}

void test_updateSeq(void)
{
  pkt_t* pkt1 = pkt_new();
  pkt_t* pkt2 = pkt_new();
  pkt_t* pkt3 = pkt_new();

  pkt_queue_t* queue = calloc(1,sizeof(pkt_queue_t));
  queue->firstNode = NULL;

  pkt_set_type(pkt1,1);
  pkt_set_type(pkt2,1);
  pkt_set_type(pkt3,1);

  pkt_set_tr(pkt1,0);
  pkt_set_tr(pkt2,0);
  pkt_set_tr(pkt3,0);

  pkt_set_seqnum(pkt1,0);
  pkt_set_seqnum(pkt2,1);
  pkt_set_seqnum(pkt3,2);

  pkt_set_payload(pkt1,"bjr1",sizeof("bjr1"));
  pkt_set_payload(pkt2,"bjr2",sizeof("bjr1"));
  pkt_set_payload(pkt3,"bjr3",sizeof("bjr1"));

  add(queue,pkt2);
  if(pkt_get_seqnum(pkt2)==queue->nextSeq){updateSeq(queue);}
  CU_ASSERT(0 == queue->nextSeq);
  add(queue,pkt3);
  if(pkt_get_seqnum(pkt3)==queue->nextSeq){updateSeq(queue);}
  CU_ASSERT(0 == queue->nextSeq);
  add(queue,pkt1);
  if(pkt_get_seqnum(pkt1)==queue->nextSeq){updateSeq(queue);}
  CU_ASSERT(3 == queue->nextSeq); 
}


/* The main() function for setting up and running the tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main()
{
   CU_pSuite pSuite = NULL;

   /* initialize the CUnit test registry */
   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   /* add a suite to the registry */
   pSuite = CU_add_suite("Suite_1", init_suite1, clean_suite1);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   /* NOTE - ORDER IS IMPORTANT - MUST TEST fread() AFTER fprintf() */
   if ((NULL == CU_add_test(pSuite, "test of updateSeq()", test_updateSeq)) ||
       (NULL == CU_add_test(pSuite, "test of get_first_seqnum()", test_get_first_seqnum)) ||
       (NULL == CU_add_test(pSuite, "test of add()", test_add)) ||
       (NULL == CU_add_test(pSuite, "test of pop()", test_pop)))
   {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* Run all tests using the CUnit Basic interface */
   CU_basic_set_mode(CU_BRM_VERBOSE);
   CU_basic_run_tests();
   CU_cleanup_registry();
   return CU_get_error();
}
