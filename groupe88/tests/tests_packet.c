#include <stdio.h>
#include <string.h>
#include "CUnit/Basic.h"

#include "../src/packet_interface.h"

/* TESTS DE LA LIBRAIRE PACKET */

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

void test_encode(void)
{
  pkt_t * paquet = pkt_new();
  char * msg="hello world";
  pkt_set_type(paquet,PTYPE_DATA);
  pkt_set_tr(paquet,0);
  pkt_set_window(paquet,28);
  pkt_set_seqnum(paquet,123);
  pkt_set_length(paquet,11);
  pkt_set_timestamp(paquet,0x00000017);
  pkt_set_crc1(paquet,0);
  pkt_set_crc2(paquet,0);
  pkt_set_payload(paquet,msg,11);

  char* buf = (char*)calloc(26,sizeof(char));
  size_t len = 26;
  pkt_status_code enc = pkt_encode(paquet, buf, &len);

  char* bufferToTest = (char*)calloc(26,sizeof(char));

  bufferToTest[0]=0x5c;
  bufferToTest[1]=0x0b;
  bufferToTest[2]=0x7b;
  bufferToTest[3]=0x17;
  bufferToTest[4]=0x00;
  bufferToTest[5]=0x00;
  bufferToTest[6]=0x00;
  bufferToTest[7]=0x50;
  bufferToTest[8]=0x12;
  bufferToTest[9]=0x12;
  bufferToTest[10]=0x86;
  bufferToTest[11]=0x68;
  bufferToTest[12]=0x65;
  bufferToTest[13]=0x6c;
  bufferToTest[14]=0x6c;
  bufferToTest[15]=0x6f;
  bufferToTest[16]=0x20;
  bufferToTest[17]=0x77;
  bufferToTest[18]=0x6f;
  bufferToTest[19]=0x72;
  bufferToTest[20]=0x6c;
  bufferToTest[21]=0x64;
  bufferToTest[22]=0x0d;
  bufferToTest[23]=0x4a;
  bufferToTest[24]=0x11;
  bufferToTest[25]=0x85;
  
  CU_ASSERT(enc==0);

  CU_ASSERT(memcmp((void *)buf, (void*)bufferToTest,len)==0);
}


void test_decode(void)
{
  pkt_t * paquet = pkt_new();
  char * msg="hello world";
  pkt_set_type(paquet,PTYPE_DATA);
  pkt_set_tr(paquet,0);
  pkt_set_window(paquet,28);
  pkt_set_seqnum(paquet,123);
  pkt_set_length(paquet,11);
  pkt_set_timestamp(paquet,0x17000000);
  pkt_set_crc1(paquet,0);
  pkt_set_crc2(paquet,0);
  pkt_set_payload(paquet,msg,11);

  size_t len = 26;

  char* bufferToTest = (char*)calloc(26,sizeof(char));

  bufferToTest[0]=0x5c;
  bufferToTest[1]=0x0b;
  bufferToTest[2]=0x7b;
  bufferToTest[3]=0x17;
  bufferToTest[4]=0x00;
  bufferToTest[5]=0x00;
  bufferToTest[6]=0x00;
  bufferToTest[7]=0x50;
  bufferToTest[8]=0x12;
  bufferToTest[9]=0x12;
  bufferToTest[10]=0x86;
  bufferToTest[11]=0x68;
  bufferToTest[12]=0x65;
  bufferToTest[13]=0x6c;
  bufferToTest[14]=0x6c;
  bufferToTest[15]=0x6f;
  bufferToTest[16]=0x20;
  bufferToTest[17]=0x77;
  bufferToTest[18]=0x6f;
  bufferToTest[19]=0x72;
  bufferToTest[20]=0x6c;
  bufferToTest[21]=0x64;
  bufferToTest[22]=0x0d;
  bufferToTest[23]=0x4a;
  bufferToTest[24]=0x11;
  bufferToTest[25]=0x85;


  pkt_t * paquet2 = pkt_new();
  pkt_status_code dec = pkt_decode(bufferToTest, len, paquet2);
  
  CU_ASSERT(dec==0);

  CU_ASSERT(pkt_get_tr(paquet)==pkt_get_tr(paquet2));

  CU_ASSERT(pkt_get_type(paquet)==pkt_get_type(paquet2));

  CU_ASSERT(pkt_get_window(paquet)==pkt_get_window(paquet2));

  CU_ASSERT(pkt_get_length(paquet)==pkt_get_length(paquet2));

  CU_ASSERT(pkt_get_seqnum(paquet)==pkt_get_seqnum(paquet2));

  CU_ASSERT(memcmp((void*)pkt_get_payload(paquet),(void*)pkt_get_payload(paquet2),pkt_get_length(paquet))==0);
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
   if ((NULL == CU_add_test(pSuite, "test of decode()", test_decode)) ||
       (NULL == CU_add_test(pSuite, "test of encode()", test_encode)))
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
