#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    uint8_t *kva;
    // 할당된 물리 주소(frame)
    size_t swap_slot;
    //스왑 슬롯 번호(-1이면 아직 스왑 테이블에 있음)
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
