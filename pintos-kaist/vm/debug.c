#include "threads/thread.h"
#include "threads/palloc.h"


/* 커널 부팅 직후 한 번 호출해서 SPT 삽입·조회가
   정상 동작하는지만 확인하는 임시 코드 */
void
vm_test_roundtrip (void) 
{
    struct supplemental_page_table *spt = &thread_current ()->spt;

    /* 가짜 사용자 가상주소 하나 지정 (page aligned) */
    void *va = (void *) 0x8048000;

    /* struct page 객체를 palloc 으로 마련 (임시) */
    struct page *p = palloc_get_page (PAL_ZERO);
    ASSERT (p != NULL);
    p->va = va;

    /* SPT 삽입 → 조회 */
    ASSERT (spt_insert_page (spt, p));
    ASSERT (spt_find_page (spt, va) == p);
}