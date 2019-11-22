#include <stdio.h>
#include <string.h>
#include "CUnit/Basic.h"

#include "../src/socket_interface.h"
#include "../src/pkt_queue_interface.h"
#include "../src/packet_interface.h"



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

void test_pkt_treat(void)
{

  source_t * source = (source_t *)malloc(sizeof(source_t));
  source->stock = (pkt_queue_t *)malloc(sizeof(pkt_queue_t));
  source->stock->nNodes = 0;
  source->stock->firstNode = NULL;
  source->stock->nextSeq = 0;


  pkt_t * pkt1 = pkt_new();
  pkt_set_type(pkt1, PTYPE_DATA);
  pkt_set_timestamp(pkt1, 123456);
  pkt_set_length(pkt1,512);

  pkt_t * pkt2 = pkt_new();
  pkt_set_type(pkt2, PTYPE_DATA);
  pkt_set_timestamp(pkt2, 123457);
  pkt_set_length(pkt2,512);
  pkt_set_seqnum(pkt2,1);

  pkt_t * pkt3 = pkt_new();
  pkt_set_type(pkt3, PTYPE_DATA);
  pkt_set_timestamp(pkt3, 123458);
  pkt_set_length(pkt3,512);
  pkt_set_seqnum(pkt3,2);

  pkt_t * pkt4 = pkt_new();
  pkt_set_type(pkt4, PTYPE_DATA);
  pkt_set_timestamp(pkt4, 123459);
  pkt_set_length(pkt4,512);
  pkt_set_seqnum(pkt4,3);

  pkt_t * pkt_tr = pkt_new();
  pkt_set_type(pkt_tr, PTYPE_DATA);
  pkt_set_tr(pkt_tr, 1);
  pkt_set_timestamp(pkt_tr, 123460);
  pkt_set_seqnum(pkt_tr,3);

  pkt_t * rep1 = pkt_treat(pkt1, source);
  CU_ASSERT(pkt_get_type(rep1)==PTYPE_ACK);
  CU_ASSERT(pkt_get_timestamp(rep1)==123456);
  CU_ASSERT(pkt_get_seqnum(rep1)==1);
  CU_ASSERT(pkt_get_window(rep1)==30);

  pkt_t * rep2 = pkt_treat(pkt3, source);
  CU_ASSERT(pkt_get_type(rep2)==PTYPE_ACK);
  CU_ASSERT(pkt_get_timestamp(rep2)==123458);
  CU_ASSERT(pkt_get_seqnum(rep2)==1);
  CU_ASSERT(pkt_get_window(rep2)==29);


  pkt_t * rep3 = pkt_treat(pkt_tr, source);
  CU_ASSERT(pkt_get_type(rep3)==PTYPE_NACK);
  CU_ASSERT(pkt_get_timestamp(rep3)==123460);
  CU_ASSERT(pkt_get_seqnum(rep3)==3);
  CU_ASSERT(pkt_get_window(rep3)==29);


  pkt_t * rep4 = pkt_treat(pkt2, source);
  CU_ASSERT(pkt_get_type(rep4)==PTYPE_ACK);
  CU_ASSERT(pkt_get_timestamp(rep4)==123457);
  CU_ASSERT(pkt_get_seqnum(rep4)==3);
  CU_ASSERT(pkt_get_window(rep4)==28);


  pkt_t * rep5 = pkt_treat(pkt3, source);
  CU_ASSERT(pkt_get_type(rep5)==PTYPE_ACK);
  CU_ASSERT(pkt_get_timestamp(rep5)==123458);
  CU_ASSERT(pkt_get_seqnum(rep5)==3);
  CU_ASSERT(pkt_get_window(rep5)==28);


  pkt_t * rep6 = pkt_treat(pkt4, source);
  CU_ASSERT(pkt_get_type(rep6)==PTYPE_ACK);
  CU_ASSERT(pkt_get_timestamp(rep6)==123459);
  CU_ASSERT(pkt_get_seqnum(rep6)==4);
  CU_ASSERT(pkt_get_window(rep6)==27);
}

void test_canWrite(void)
{

  source_t * source = (source_t *)malloc(sizeof(source_t));
  source->stock = (pkt_queue_t *)malloc(sizeof(pkt_queue_t));
  source->stock->nNodes = 0;
  source->stock->firstNode = NULL;
  source->stock->nextSeq = 254;

  pkt_t * pkt1 = pkt_new();
  pkt_set_type(pkt1, PTYPE_DATA);
  pkt_set_timestamp(pkt1, 123456);
  pkt_set_length(pkt1,0);
  pkt_set_seqnum(pkt1,254);

  pkt_t * pkt2 = pkt_new();
  pkt_set_type(pkt2, PTYPE_DATA);
  pkt_set_timestamp(pkt2, 123457);
  pkt_set_length(pkt2,0);
  pkt_set_seqnum(pkt2,255);

  pkt_t * pkt3 = pkt_new();
  pkt_set_type(pkt3, PTYPE_DATA);
  pkt_set_timestamp(pkt3, 123458);
  pkt_set_length(pkt3,0);
  pkt_set_seqnum(pkt3,0);

  pkt_t * pkt4 = pkt_new();
  pkt_set_type(pkt4, PTYPE_DATA);
  pkt_set_timestamp(pkt4, 123459);
  pkt_set_length(pkt4,0);
  pkt_set_seqnum(pkt4,1);

  pkt_t * pkt_tr = pkt_new();
  pkt_set_type(pkt_tr, PTYPE_DATA);
  pkt_set_tr(pkt_tr, 1);
  pkt_set_timestamp(pkt_tr, 123460);
  pkt_set_seqnum(pkt_tr,0);

  pkt_treat(pkt1, source);

  CU_ASSERT(canWrite(source)==0);
  pop(source->stock);

  pkt_treat(pkt3, source);


  CU_ASSERT(canWrite(source)!=0);

  pkt_treat(pkt_tr, source);

  CU_ASSERT(canWrite(source)!=0);

  pkt_treat(pkt2, source);

  CU_ASSERT(canWrite(source)==0);
  pop(source->stock);
  CU_ASSERT(canWrite(source)==0);
  pop(source->stock);
  
  pkt_treat(pkt3, source);

  CU_ASSERT(canWrite(source)!=0);

  pkt_treat(pkt4, source);

  CU_ASSERT(canWrite(source)==0);

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
   if ((NULL == CU_add_test(pSuite, "test of pkt_treat() (seqnumOK(), pkt_ack())", test_pkt_treat)) ||
       (NULL == CU_add_test(pSuite, "test of canWrite()", test_canWrite)))
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
