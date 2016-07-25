#ifndef _LIST_H_
#define _LIST_H_

struct list_node 
{
	struct list_node *next;
	struct list_node *prev;
};

#define list_init(head) do {			\
	(head)->next = (head)->prev = head;	\
} while (0)

//add a new member in head
static inline void list_insert (struct list_node *new_member, struct list_node *head)
{
	new_member->prev = head;
	new_member->next = head->next;
	new_member->prev->next = new_member;
	new_member->next->prev = new_member;
}

//add a new member in tail
static inline void list_insert_tail (struct list_node *new_member, struct list_node *head)
{
	new_member->next = head;
	new_member->prev = head->prev;
	new_member->prev->next = new_member;
	new_member->next->prev = new_member;
}

//delete a member
static inline void list_remove (struct list_node *del_member)
{
	del_member->prev->next = del_member->next;
	del_member->next->prev = del_member->prev;
	del_member->next = NULL;
	del_member->prev = NULL;
}

//clear a list
static inline int list_empty (struct list_node *head)
{
	return (head->next == head);
}

//get list size,the size is not include the head
static inline int list_size(struct list_node *head)
{
	struct list_node* find;
	int num;

	num = 0;
	find = head->next;
	while(find != head)
	{
		num++;
		find = find->next;
	}

	return num;
}

//p is contained in the head of the list
//memeber is the member of type,which is the outer struct
//type is the outer struct
#define list_entry(p, type, member)					\
	((type *)((char *)(p)-(unsigned long)(&((type *)0)->member)))

//tmp is the temp struct list_node member.
//head is the head of the list.
#define list_for_each(tmp, head)				     \
	for (tmp = (head)->next; tmp != (head); tmp = tmp->next)

//pos: outer struct
//head: list head
//member: the list_node member of the outer struct
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
			&pos->member != (head); 					\
			pos = list_entry(pos->member.next, typeof(*pos), member))

#endif /* _LIST_H_ */
