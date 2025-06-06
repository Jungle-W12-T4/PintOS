/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "vm/anon.h"
#include "vm/uninit.h"

static struct list frame_table;
static struct lock frame_lock;

static uint64_t page_hash (const struct hash_elem *e, void *aux);
static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void page_destructor (struct hash_elem *e, void *aux);
static struct list frame_table;

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
	const struct page *p = hash_entry (e, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof p->va);
}

static bool
page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	const struct page *pa = hash_entry(a, struct page, hash_elem);
	const struct page *pb = hash_entry(b, struct page, hash_elem);
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

	struct supplemental_page_table *spt = &thread_current ()->spt;

	upage = pg_round_down(upage);
	if(spt_find_page(spt, upage) != NULL){
		return false;
	}

	struct page *page = malloc(sizeof(struct page));
	if(page == NULL){
		return false;
	}

	bool(*page_initializer)(struct page *, enum vm_type, void *) = NULL;
	switch (VM_TYPE(type))
	{
	case VM_ANON:
		page_initializer = anon_initializer;
		break;
	case VM_FILE: 
		page_initializer = file_backed_initializer;
		break;
	default:
		PANIC("Invalid vm_type");
	}
	uninit_new(page, upage, init, type, aux, page_initializer);
	page->writable = writable;
	page->vm_type = type;

	if(!spt_insert_page(spt,page)){
		free(page);
		return false;
	}
	return true;
}

/* SPT에서 va에 해당하는 page 구조체를 찾아 반환 */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	// 해시 탐색용 임시 페이지
	struct page page;

	// va를 페이지 기준 주소로 정렬
    page.va = pg_round_down(va);

	// 해당 주소에 대응하는 page를 해시 테이블에서 탐색
    struct hash_elem *e = hash_find(&spt->spt_hash, &page.hash_elem);

	// 찾으면 page 반환, 없으면 NULL
    return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {

	return hash_insert(&spt->pages, &page->hash_elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->pages, &page->hash_elem);
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
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	ASSERT (frame != NULL);

	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

	if (frame->kva == NULL) {
		frame = vm_evict_frame();
	}
	else {
		list_push_back(&frame_table, &frame->elem);
	}

	frame->page = NULL;

	ASSERT (frame->page == NULL);
	// void *kva = palloc_get_page(PAL_USER);
	// if(kva == NULL){
	// 	PANIC("todo: implement frame eviction");
	// }

	// struct frame *frame = malloc(sizeof(struct frame));
	// ASSERT(frame != NULL);

	// frame->kva = kva;
	// frame->page = NULL;
	// frame->pinned = false;
	// lock_acquire(&frame_lock);
	// list_push_back(&frame_table, &frame->elem);
	// lock_release(&frame_lock);

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
    struct page *page = hash_entry (e, struct page, hash_elem);
    destroy (page);
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
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
	struct page *page = spt_find_page(&thread_current() -> spt, addr);
	if(page){
		return vm_do_claim_page(page);
	}
	
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
	if (frame == NULL){
		return false;
	}

	// 양방향 연결
	frame -> page = page;
	page -> frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	//페이지 테이블에 매핑
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) {
		return false;
	}

	return swap_in(page, frame->kva);

	// if(!succ){
	// 	frame->page = NULL;
	// 	page->frame = NULL;
	// 	vm_frame_free(frame);
	// 	return false;
	// }

	// if(!swap_in(page, frame->kva)){
	// 	pml4_clear_page(thread_current()->pml4, page->va);
	// 	frame->page = NULL;
	// 	page->frame = NULL;
	// 	vm_frame_free(frame);
	// 	return false;
	// }
	// return true;
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
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	if(spt == NULL)return;
	hash_destroy(&spt->pages, NULL);
}