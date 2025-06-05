/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "vm/anon.h"
#include "vm/uninit.h"

static struct list frame_table;
static struct lock frame_lock;

static uint64_t page_hash (const struct hash_elem *e, void *aux UNUSED);
static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
static void page_destructor (struct hash_elem *e, void *aux UNUSED);
void hash_page_destroy(struct hash_elem *e, void *aux);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
	list_init(&frame_table);
	lock_init(&frame_lock);
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

static uint64_t
page_hash (const struct hash_elem *e, void *aux UNUSED){
	const struct page *p = hash_entry (e, struct page, h_elem);
	return hash_bytes (&p->va, sizeof p->va);
}

static bool
page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	const struct page *pa = hash_entry(a, struct page, h_elem);
	const struct page *pb = hash_entry(b, struct page, h_elem);
	return pa->va < pb->va;
	}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {

		/** #project3-Anonymous Page */
		struct page *page = malloc(sizeof(struct page));

		if (!page)
			goto err;

		typedef bool (*initializer_by_type)(struct page *, enum vm_type, void *);
		initializer_by_type initializer = NULL;

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			initializer = anon_initializer;
			break;
		case VM_FILE:
			initializer = file_backed_initializer;
			break;
		}

		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;
		
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	va = pg_round_down(va);
	struct page p;
	p.va = va;
	struct hash_elem *e = hash_find(&spt->pages, &p.h_elem);

	if(e != NULL){
		return hash_entry(e, struct page, h_elem);
	}else{
		return NULL;
	}
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	
	struct hash_elem *e = hash_insert(&spt->pages, &page->h_elem);
	return e == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt -> pages, &page->h_elem);
	vm_dealloc_page(page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	void *kva = palloc_get_page(PAL_USER);
	if(kva == NULL){
		PANIC("todo: implement frame eviction");
	}

	struct frame *frame = malloc(sizeof(struct frame));
	ASSERT(frame != NULL);

	frame->kva = kva;
	frame->page = NULL;
	frame->pinned = false;
	lock_acquire(&frame_lock);
	list_push_back(&frame_table, &frame->elem);
	lock_release(&frame_lock);

	return frame;
}

void vm_frame_free(struct frame *f){
	ASSERT(f != NULL);
	if(f -> page){
		f->page->frame = NULL;
	}
	lock_acquire(&frame_lock);
	list_remove(&f->elem);
	lock_release(&frame_lock);

	palloc_free_page(f->kva);
	free(f);
}

static void
page_destructor (struct hash_elem *e, void *aux UNUSED) {
    struct page *page = hash_entry (e, struct page, h_elem);
    destroy (page);
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	bool success = false;
	addr = pg_round_down(addr);
	if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true)){
		success = vm_claim_page(addr);
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;

	// ❶ 잘못된 주소 접근 방지: NULL 또는 커널 영역이면 처리 불가
	if(addr == NULL || is_kernel_vaddr(addr)){
		return false;
	}

	// ❷ 보호 예외 필터링: 쓰기 권한이 없는데 쓰기 요청이거나, 주소가 사용자 코드/스택 범위가 아니면 거부
	if ((!not_present && write) || (addr < (void *)0x400000 || addr >= (void *)USER_STACK)) {
		return false;
	}

	// ❸ 실제로 페이지가 존재하지 않을 때만 처리 시도 (page fault가 진짜 'not present'일 때)
	if(not_present){
		struct page *page = spt_find_page(spt, addr);

		// 현재 유저 스택 포인터를 가져옴
		void *rsp = user ? f->rsp : thread_current()->stack_pointer;

		// ❹ 스택 성장 조건 판별 (조건 1): rsp - 8 == fault addr (예: push 명령 직전)
		if(STACK_LIMIT <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK){
			vm_stack_growth(addr);
			return true;
		}
		// ❺ 스택 성장 조건 판별 (조건 2): fault addr가 rsp보다 위쪽이면 (일반적인 스택 접근)
		else if (STACK_LIMIT <= rsp && rsp <= addr && addr <= USER_STACK){
			vm_stack_growth(addr);
			return true;
		}

		// ❻ 기존 spt에서 페이지를 다시 찾고, 권한 검사
		page = spt_find_page(spt, addr);
		if(!page || (write && !page->writable)){
			return false;
		}

		// ❼ 실제 페이지 프레임에 매핑 수행
		return vm_do_claim_page(page);
	}

	// ❽ 기타의 경우 처리 불가
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
	{
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	//프레임 확보
	struct frame *frame = vm_get_frame ();
	if(frame == NULL){
		return false;
	}

	// 양방향 연결
	frame -> page = page;
	page -> frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	//페이지 테이블에 매핑
	bool succ = pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);

	if(!succ){
		frame->page = NULL;
		page->frame = NULL;
		vm_frame_free(frame);
		return false;
	}

	if(!swap_in(page, frame->kva)){
		pml4_clear_page(thread_current()->pml4, page->va);
		frame->page = NULL;
		page->frame = NULL;
		vm_frame_free(frame);
		return false;
	}
	return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;
	hash_first(&i, &src->pages);
	while (hash_next(&i))
	{
		struct page *src_page = hash_entry(hash_cur(&i), struct page, h_elem);
		enum vm_type src_type = src_page->operations->type;

		if(src_type == VM_UNINIT){
			vm_alloc_page_with_initializer(
				src_page->uninit.type,
				src_page->va,
				src_page->writable,
				src_page->uninit.init,
				src_page->uninit.aux
			);
		}else{
			if(vm_alloc_page(src_type, src_page->va, src_page->writable) && vm_claim_page(src_page->va)){
				struct page *dst_page = spt_find_page(dst, src_page->va);
				memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
			}
		}
	}
	return true;

}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	hash_clear(&spt->pages, hash_page_destroy);
}
