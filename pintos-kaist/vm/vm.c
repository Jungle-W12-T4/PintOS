/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"

typedef bool (*page_initializer)(struct page *, enum vm_type, void *kva);

static page_initializer initializer_table[] = {
	[VM_ANON] = anon_initializer,
	[VM_FILE] = file_backed_initializer,
};

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
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

/* 초기화 함수를 사용하는 "대기중인 페이지(uninit page)" 객체를 생성합니다. 
 * 새로운 페이지를 만들고 싶다면, 직접 만들지 말고 이 함수 또는 vm_alloc_page를 통해 생성해야 합니다.
 */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, vm 타입에 따라 적절한 초기화함수를 가져오세요.
		 * TODO: 그런 다음 uninit_new를 호출해서 uninit 타입의 페이지 구조체를 생성하세요. 
		 * TODO: uninit_new 호출 이후에는 해당 페이지 구조체의 필드를 수정해야 합니다. */
		struct page *page = (struct page *)malloc(sizeof(struct page));
		if (page == NULL) goto err;
		
		if (VM_TYPE(type) >= sizeof(initializer_table)/sizeof(initializer_func) || initializer_table[VM_TYPE(type)] == NULL) {goto err};

		page_initializer = initializer_table[VM_TYPE(type)];

		/* TODO: 페이지를 spt에 삽입하세요. */
		uninit_new(page, upage, init, type, aux, page_initializer);
		page->writable = writable;
		page->vm_type = type;

		if (!spt_insert_page(spt, page)) {
			free(page);
			goto err;
		}
		return true;
	}
err:
	return false;
}

/* spt에서 va를 찾고 page를 리턴한다. 실패 시에는 NULL을 리턴한다. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
    struct page dummy_page;
    struct hash_elem *e;

	dummy_page.va = pg_round_down(va);
	e = hash_find(&spt->hash, &dummy_page.hash_elem);
	if (e == NULL) {
		return NULL;
	}

	return hash_entry(e, struct page, hash_elem);
}

/* 유효성 검사 후 page를 spt에 삽입 */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
    int succ = false;
    // 이미 같은 va가 등록되어있다면 실패
	if (hash_insert(&spt->hash, &page->hash_elem) == NULL) succ = true;
    return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
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

/* palloc()을 사용해서 프레임을 할당하거나 가져옵니다.
 * 만약 사용 가능한 물리 페이지가 없다면 페이지를 하나 교체하여 메모리 공간을 확보한 후 반환합니다.
 * 이 함수는 항사 유효한 주소를 반환해야 합니다.
 * 즉, 사용자 영역의 메모리가 가득 차더라도 이 함수는 프레임을 교체해서라도 사용가능한 메모리를 반환해야 합니다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	void *kva = palloc_get_page(PAL_USER); // 사용자 풀에서 페이지 할당

	if (kva == NULL) {
		PANIC("Out of user memory and eviction not implemented yet!");
	}

	frame = malloc(sizeof(struct frame));
	if (frame == NULL) {
		PANIC("Failed to allocate frame structure");
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	frame->kva = kva;
	frame->page = NULL;

	// todo: frame 리스트에 넣을 때 락 갖고 락 해제시킴

	return frame;
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
vm_try_handle_fault (struct intr_frame *f, void *addr, bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* 'spt에 등록된' va에 대한 페이지 할당 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	struct thread *curr = thread_current();
	page = spt_find_page(&curr->spt, va);
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* 페이지 할당 및 mmu 세팅 */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* 페이지와 프레임 연결 */
	frame->page = page;
	page->frame = frame;

	/* TODO: 해당 페이지의 va가 kva를 가리키도록 페이지 테이블에 등록 */
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) {
		free(frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* 새로운 보조 페이지 테이블을 초기화합니다. */
void
 supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash, vm_hash_func, vm_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/* va 값을 기반으로 해시값을 계산합니다. */
unsigned vm_hash_func(const struct hash_elem *e, void *aux) {
	const struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

/* va 값을 기준으로 정렬 순서를 비교합니다. */
bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	const struct page *p_a = hash_entry(a, struct page, hash_elem);
	const struct page *p_b = hash_entry(b, struct page, hash_elem);
	return p_a->va < p_b->va;
}