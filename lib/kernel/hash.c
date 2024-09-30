/* Hash table.

   This data structure is thoroughly documented in the Tour of
   Pintos for Project 3.

   See hash.h for basic information. */

#include "hash.h"
#include "../debug.h"
#include "threads/malloc.h"

#define list_elem_to_hash_elem(LIST_ELEM) \
	list_entry(LIST_ELEM, struct hash_elem, list_elem)

static struct list *find_bucket(struct hash *, struct hash_elem *);
static struct hash_elem *find_elem(struct hash *, struct list *,
								   struct hash_elem *);
static void insert_elem(struct hash *, struct list *, struct hash_elem *);
static void remove_elem(struct hash *, struct hash_elem *);
static void rehash(struct hash *);

/* Initializes hash table H to compute hash values using HASH and
   compare hash elements using LESS, given auxiliary data AUX. */
bool hash_init(struct hash *h, hash_hash_func *hash, hash_less_func *less,
			   void *aux) {
	h->elem_cnt = 0;
	h->bucket_cnt = 4;
	h->buckets = malloc(sizeof *h->buckets * h->bucket_cnt);
	h->hash = hash;
	h->less = less;
	h->aux = aux;

	if (h->buckets != NULL) {
		hash_clear(h, NULL);
		return true;
	} else
		return false;
}

/* Removes all the elements from H.

   If DESTRUCTOR is non-null, then it is called for each element
   in the hash.  DESTRUCTOR may, if appropriate, deallocate the
   memory used by the hash element.  However, modifying hash
   table H while hash_clear() is running, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), yields undefined behavior,
   whether done in DESTRUCTOR or elsewhere. 
   (hash_init()으로 초기화된 hash의 모든 원소들을 제거합니다.
action이 null이 아니라면, 이 action은 해시 테이블의 각 원소에 의해 한번씩 호출될 것이고,
호출자(caller)는 해당 원소에 사용된 다른 자원들이나 할당된 메모리를 해제할 수 있습니다.
예를 들어, 해시 테이블의 원소들이 malloc()을 이용해서 동적으로 할당되었다면, action을 통해 각 요소를 free()할 수 있습니다.
이러한 작업이 안전한 이유는, hash_clear()가 해시 원소에서 action을 호출한 뒤에는 해당 해시 원소의 메모리에 접근하지 않을 것이기 때문입니다.
다만, action은 hash_insert()또는 hash_delete()와 같이 해시 테이블을 수정할 수 있는 함수를 호출해서는 안됩니다.)*/
void hash_clear(struct hash *h, hash_action_func *destructor) {
	size_t i;

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];

		if (destructor != NULL)
			while (!list_empty(bucket)) {
				struct list_elem *list_elem = list_pop_front(bucket);
				struct hash_elem *hash_elem = list_elem_to_hash_elem(list_elem);
				destructor(hash_elem, h->aux);
			}

		list_init(bucket);
	}

	h->elem_cnt = 0;
}

/* Destroys hash table H.

   If DESTRUCTOR is non-null, then it is first called for each
   element in the hash.  DESTRUCTOR may, if appropriate,
   deallocate the memory used by the hash element.  However,
   modifying hash table H while hash_clear() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done in DESTRUCTOR or
   elsewhere.
   (action이 null이 아닌경우, hash_clear()호출과 동일하게, action은
   각 해시 원소에 의해 한번씩 호출될 것입니다. 그런 다음, hash가 할당
   받았던 메모리를 반납시킵니다. 이 함수를 수행한 뒤에 이 hash가 hash_init()
   호출이 선행되지 않은 채로 해시 테이블 함수로 전달되어선 안된다.) */
void hash_destroy(struct hash *h, hash_action_func *destructor) {
	if (destructor != NULL)
		hash_clear(h, destructor);
	free(h->buckets);
}

/* Inserts NEW into hash table H and returns a null pointer, if
   no equal element is already in the table.
   If an equal element is already in the table, returns it
   without inserting NEW. 
   (NEW를 해시 테이블 H에 삽입하고, 동일한 요소가 테이블에 없으면
   null 포인터를 반환한다. 만약 동일한 요소가 테이블에 이미 존재
   한다면, NEW를 삽입하지 않고 그 요소를 반환한다.   )*/
struct hash_elem *hash_insert(struct hash *h, struct hash_elem *new) {
	struct list *bucket = find_bucket(h, new);
	struct hash_elem *old = find_elem(h, bucket, new);

	if (old == NULL)
		insert_elem(h, bucket, new);

	rehash(h);

	return old;
}

/* Inserts NEW into hash table H, replacing any equal element
   already in the table, which is returned. */
struct hash_elem *hash_replace(struct hash *h, struct hash_elem *new) {
	struct list *bucket = find_bucket(h, new);
	struct hash_elem *old = find_elem(h, bucket, new);

	if (old != NULL)
		remove_elem(h, old);
	insert_elem(h, bucket, new);

	rehash(h);

	return old;
}

/* Finds and returns an element equal to E in hash table H, or a
   null pointer if no equal element exists in the table. 
   (hash에서 element와 같은 원소를 찾습니다. 찾았다면 그 원소를 반환,
   그렇지 않다면 null을 반환.)*/
struct hash_elem *hash_find(struct hash *h, struct hash_elem *e) {
	return find_elem(h, find_bucket(h, e), e);
}

/* Finds, removes, and returns an element equal to E in hash
   table H.  Returns a null pointer if no equal element existed
   in the table.

   If the elements of the hash table are dynamically allocated,
   or own resources that are, then it is the caller's
   responsibility to deallocate them. 
   (hash에서 element와 같은 원소를 찾습니다. 찾았다면 hash에서 삭제한 뒤에
   그 원소를 반환하고, 그렇지 않다면 null을 반환하며 hash는 수정되지 않습니다.
   때에 따라, 호출자는 반환되는 원소와 관련된 모든 자원의 할당을 해제해야할
   책임이 있습니다. 예를 들어, 해시 테이블의 원소들이 malloc()을 이용해서
   동적으로 할당되었다면, 호출자는 더 이상 필요하지 않은 원소를 반드시 free()
   해줘야한다.)*/
struct hash_elem *hash_delete(struct hash *h, struct hash_elem *e) {
	struct hash_elem *found = find_elem(h, find_bucket(h, e), e);
	if (found != NULL) {
		remove_elem(h, found);
		rehash(h);
	}
	return found;
}

/* Calls ACTION for each element in hash table H in arbitrary
   order.
   Modifying hash table H while hash_apply() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done from ACTION or elsewhere. */
void hash_apply(struct hash *h, hash_action_func *action) {
	size_t i;

	ASSERT(action != NULL);

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];
		struct list_elem *elem, *next;

		for (elem = list_begin(bucket); elem != list_end(bucket); elem = next) {
			next = list_next(elem);
			action(list_elem_to_hash_elem(elem), h->aux);
		}
	}
}

/* Initializes I for iterating hash table H.

   Iteration idiom:

   struct hash_iterator i;

   hash_first (&i, h);
   while (hash_next (&i))
   {
   struct foo *f = hash_entry (hash_cur (&i), struct foo, elem);
   ...do something with f...
   }

   Modifying hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. 
   (iterators를 해시 테이블의 첫번째 요소 직전으로 초기화한다.)*/
void hash_first(struct hash_iterator *i, struct hash *h) {
	ASSERT(i != NULL);
	ASSERT(h != NULL);

	i->hash = h;
	i->bucket = i->hash->buckets;
	i->elem = list_elem_to_hash_elem(list_head(i->bucket));
}

/* Advances I to the next element in the hash table and returns
   it.  Returns a null pointer if no elements are left.  Elements
   are returned in arbitrary order.

   Modifying a hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. 
   (hash_next()에 의해 리턴된 값 중 가장 최근 값을 리턴한다. hash_first()
   로 iterator를 초기화 하고 나서 최초의 hash_next()를 호출하기 전에 
   hash_cur()를 호출하는 것은 정의되지 않은 행동입니다.)*/
struct hash_elem *hash_next(struct hash_iterator *i) {
	ASSERT(i != NULL);

	i->elem = list_elem_to_hash_elem(list_next(&i->elem->list_elem));
	while (i->elem == list_elem_to_hash_elem(list_end(i->bucket))) {
		if (++i->bucket >= i->hash->buckets + i->hash->bucket_cnt) {
			i->elem = NULL;
			break;
		}
		i->elem = list_elem_to_hash_elem(list_begin(i->bucket));
	}

	return i->elem;
}

/* Returns the current element in the hash table iteration, or a
   null pointer at the end of the table.  Undefined behavior
   after calling hash_first() but before hash_next(). */
struct hash_elem *hash_cur(struct hash_iterator *i) {
	return i->elem;
}

/* Returns the number of elements in H. 
(현재 hash에 저장된 원소의 수 반환)*/
size_t hash_size(struct hash *h) { return h->elem_cnt; }

/* Returns true if H contains no elements, false otherwise. 
(현재 hash에 원소가 없다면 true를, 하나라도 가지고 있다면 false를 반환한다.)*/
bool hash_empty(struct hash *h) { return h->elem_cnt == 0; }

/* Fowler-Noll-Vo hash constants, for 32-bit word sizes. */
#define FNV_64_PRIME 0x00000100000001B3UL
#define FNV_64_BASIS 0xcbf29ce484222325UL

/* Returns a hash of the SIZE bytes in BUF. 
요소의 데이터를 64bit unsigned int로 해싱한 값을 리턴.*/
uint64_t hash_bytes(const void *buf_, size_t size) {
	/* Fowler-Noll-Vo 32-bit hash, for bytes. */
	const unsigned char *buf = buf_;
	uint64_t hash;

	ASSERT(buf != NULL);

	hash = FNV_64_BASIS;
	while (size-- > 0)
		hash = (hash * FNV_64_PRIME) ^ *buf++;

	return hash;
}

/* Returns a hash of string S.
null-terminated string을 해싱한 값을 리턴함. */
uint64_t hash_string(const char *s_) {
	const unsigned char *s = (const unsigned char *)s_;
	uint64_t hash;

	ASSERT(s != NULL);

	hash = FNV_64_BASIS;
	while (*s != '\0')
		hash = (hash * FNV_64_PRIME) ^ *s++;

	return hash;
}

/* Returns a hash of integer I.
(i를 해싱한 값을 반환.) */
uint64_t hash_int(int i) { return hash_bytes(&i, sizeof i); }

/* Returns the bucket in H that E belongs in. */
static struct list *find_bucket(struct hash *h, struct hash_elem *e) {
	size_t bucket_idx = h->hash(e, h->aux) & (h->bucket_cnt - 1);
	return &h->buckets[bucket_idx];
}

/* Searches BUCKET in H for a hash element equal to E.  Returns
   it if found or a null pointer otherwise. */
static struct hash_elem *find_elem(struct hash *h, struct list *bucket,
								   struct hash_elem *e) {
	struct list_elem *i;

	for (i = list_begin(bucket); i != list_end(bucket); i = list_next(i)) {
		struct hash_elem *hi = list_elem_to_hash_elem(i);
		if (!h->less(hi, e, h->aux) && !h->less(e, hi, h->aux))
			return hi;
	}
	return NULL;
}

/* Returns X with its lowest-order bit set to 1 turned off. */
static inline size_t turn_off_least_1bit(size_t x) { return x & (x - 1); }

/* Returns true if X is a power of 2, otherwise false. */
static inline size_t is_power_of_2(size_t x) {
	return x != 0 && turn_off_least_1bit(x) == 0;
}

/* Element per bucket ratios. */
#define MIN_ELEMS_PER_BUCKET 1  /* Elems/bucket < 1: reduce # of buckets. */
#define BEST_ELEMS_PER_BUCKET 2 /* Ideal elems/bucket. */
#define MAX_ELEMS_PER_BUCKET 4  /* Elems/bucket > 4: increase # of buckets. */

/* Changes the number of buckets in hash table H to match the
   ideal.  This function can fail because of an out-of-memory
   condition, but that'll just make hash accesses less efficient;
   we can still continue. 
   (해시 테이블 H의 버킷 수를 이상적인 수로 변경합니다.
	이 함수는 메모리 부족으로 인해 실패할 수 있지만, 그럴 경우 
	해시 접근이 덜 효율적이게 될 뿐이고 계속 진행할 수는 있습니다.)*/
static void rehash(struct hash *h) {
	size_t old_bucket_cnt, new_bucket_cnt;
	struct list *new_buckets, *old_buckets;
	size_t i;

	ASSERT(h != NULL);

	/* Save old bucket info for later use. */
	old_buckets = h->buckets;
	old_bucket_cnt = h->bucket_cnt;

	/* Calculate the number of buckets to use now.
	   We want one bucket for about every BEST_ELEMS_PER_BUCKET.
	   We must have at least four buckets, and the number of
	   buckets must be a power of 2. */
	new_bucket_cnt = h->elem_cnt / BEST_ELEMS_PER_BUCKET;
	if (new_bucket_cnt < 4)
		new_bucket_cnt = 4;
	while (!is_power_of_2(new_bucket_cnt))
		new_bucket_cnt = turn_off_least_1bit(new_bucket_cnt);

	/* Don't do anything if the bucket count wouldn't change. */
	if (new_bucket_cnt == old_bucket_cnt)
		return;

	/* Allocate new buckets and initialize them as empty. */
	new_buckets = malloc(sizeof *new_buckets * new_bucket_cnt);
	if (new_buckets == NULL) {
		/* Allocation failed.  This means that use of the hash table will
		   be less efficient.  However, it is still usable, so
		   there's no reason for it to be an error. */
		return;
	}
	for (i = 0; i < new_bucket_cnt; i++)
		list_init(&new_buckets[i]);

	/* Install new bucket info. */
	h->buckets = new_buckets;
	h->bucket_cnt = new_bucket_cnt;

	/* Move each old element into the appropriate new bucket. */
	for (i = 0; i < old_bucket_cnt; i++) {
		struct list *old_bucket;
		struct list_elem *elem, *next;

		old_bucket = &old_buckets[i];
		for (elem = list_begin(old_bucket); elem != list_end(old_bucket);
			 elem = next) {
			struct list *new_bucket =
				find_bucket(h, list_elem_to_hash_elem(elem));
			next = list_next(elem);
			list_remove(elem);
			list_push_front(new_bucket, elem);
		}
	}

	free(old_buckets);
}

/* Inserts E into BUCKET (in hash table H). */
static void insert_elem(struct hash *h, struct list *bucket,
						struct hash_elem *e) {
	h->elem_cnt++;
	list_push_front(bucket, &e->list_elem);
}

/* Removes E from hash table H. */
static void remove_elem(struct hash *h, struct hash_elem *e) {
	h->elem_cnt--;
	list_remove(&e->list_elem);
}
