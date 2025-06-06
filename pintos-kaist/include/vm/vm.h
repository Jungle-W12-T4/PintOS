#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "lib/kernel/hash.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* "page"를 나타내는 구조체입니다.
 * 이 구조체는 일종의 "부모 클래스"로, 네 개의 "자식 클래스"를 가지고 있습니다.
 * uninit_page, file_page, anon_page, and page cache (project4).
 * 이 구조체에 이미 정의된 멤버들은 절대 제거하거나 수정하지 마세요. */
struct page {
    const struct page_operations *operations;
    void *va;              /* 유저 공간의 주소 */
    struct frame *frame;   /* frame 역참조 */

    // 추가
    struct hash_elem hash_elem; /* SPT에서 사용되는 해시 요소 */
    // struct list_elem elem; /* frame table 등에서 사용 */
    bool writable; /* 페이지가 쓰기 가능한지 여부 */
	enum vm_type vm_type;

    /* 타입별 데이터는 union에 묶여있습니다.
     * 각 함수는 현재 union이 어떤 타입인지 자동으로 감지합니다. */
    union {
        struct uninit_page uninit;
        struct anon_page anon;
        struct file_page file;
#ifdef EFILESYS
        struct page_cache page_cache;
#endif
	};
};

/* "frame"을 나타냅니다. */
struct frame {
    void *kva; // 물리 메모리 주소와 1:1로 매핑된 가상 주소
    struct page *page; // 페이지 구조체
    struct list_elem elem; // frame table에 삽입하기 위한 리스트 노드
    // struct list_elem clock_elem; // clock 리스트용 노드
    // bool accesssed;  // CLOCK 알고리즘용 사용 비트
    bool pinned;  // 스왑 금지 여부 (페이지 폴트 처리 중 등)
};

struct lazy_load_info {
	struct file *file;
	off_t offset;
	size_t page_read_bytes;
	size_t page_zero_bytes;
};

/* 페이지 동작을 위한 함수 테이블입니다.
 * 이는 C언어에서 인터페이스를 구현하는 하나의 방법입니다.
 * 구조체의 멤버에 이 메서드 테이블을 넣고, 필요할 때마다 호출하세요.
 */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* 현재 프로세스의 메모리 공간을 나타냅니다. 
 * 보조 페이지 테이블 구현에 대해서는 어떠한 제약사항도 없습니다. 
 */
struct supplemental_page_table {
   struct hash pages;
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

unsigned vm_hash_func(const struct hash_elem *e, void *aux);
bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif  /* VM_VM_H */
