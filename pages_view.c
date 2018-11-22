// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/highmem.h>

struct page_entry {
	struct page *page;
	struct list_head list;
};

static struct proc_dir_entry *proc_entry;

static int pte_try_add(pte_t *pte, pgtable_t token, unsigned long addr,
		       void *data)
{
	struct page_entry *entry;
	struct list_head *head = data;
	struct page *page = pte_page(*pte);

	/* We are not interested in non-anonymous pages */
	if (!PageAnon(page))
		return 0;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (entry) {
		INIT_LIST_HEAD(&entry->list);
		entry->page = page;
		list_add(&entry->list, head);
	}

	return 0;
}

static void mm_get_pages(struct mm_struct *mm, struct list_head *head)
{
	struct vm_area_struct *vma;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		/* Skip mapped files */
		if (vma->vm_file)
			continue;
		/* Skip executable VMAs (probably shared libraries) and stack */
		if (vma->vm_flags & (VM_EXEC | VM_GROWSDOWN))
			continue;
		apply_to_page_range(mm, vma->vm_start,
				    vma->vm_end - vma->vm_start,
				    pte_try_add, head);
	}
}

static void *pages_view_seq_start(struct seq_file *s, loff_t *ppos)
{
	struct list_head *head = s->private;

	return seq_list_start(head, *ppos);
}

static void *pages_view_seq_next(struct seq_file *s, void *v, loff_t *ppos)
{
	struct list_head *head = s->private;

	return seq_list_next(v, head, ppos);
}

static void pages_view_seq_stop(struct seq_file *s, void *v)
{
}

static int pages_view_seq_show(struct seq_file *s, void *v)
{
	struct page_entry *entry = list_entry(v, struct page_entry, list);
	unsigned int *page_data_start;
	unsigned int *page_data;

	page_data_start = page_data = kmap(entry->page);

	for (; page_data < page_data_start + PAGE_SIZE; ++page_data)
		seq_printf(s, "%08x", *page_data);

	kunmap(entry->page);

	return 0;
}

static const struct seq_operations pages_view_seq_ops = {
	.start = pages_view_seq_start,
	.next  = pages_view_seq_next,
	.stop  = pages_view_seq_stop,
	.show  = pages_view_seq_show,
};

static int pages_view_open(struct inode *inode, struct file *file)
{
	struct list_head *head;
	struct seq_file *s;
	int ret = -ENOMEM;

	head = kmalloc(sizeof(*head), GFP_KERNEL);
	if (!head)
		return ret;

	ret = seq_open(file, &pages_view_seq_ops);
	if (!ret) {
		s = file->private_data;
		s->private = head;
		INIT_LIST_HEAD(head);
		mm_get_pages(current->mm, head);
	} else {
		kfree(head);
	}

	return ret;
}

static const struct file_operations pages_view_fops = {
	.owner   = THIS_MODULE,
	.open    = pages_view_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static int __init pages_view_init(void)
{
	proc_entry = proc_create("pages_view", 0, NULL, &pages_view_fops);
	return 0;
}

static void __exit pages_view_exit(void)
{
	proc_remove(proc_entry);
}

module_init(pages_view_init);
module_exit(pages_view_exit);

MODULE_AUTHOR("Dmitry Gerasimov <di.gerasimov@gmail.com>");
MODULE_DESCRIPTION("Memory pages content view");
MODULE_LICENSE("GPL");
