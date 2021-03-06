/*
 * DMA region bookkeeping routines
 *
 * Copyright (C) 2002 Maas Digital LLC
 *
 * This code is licensed under the GPL.  See the file COPYING in the root
 * directory of the kernel sources for details.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include "dma.h"

/* dma_prog_region */

void dma_prog_region_init(struct dma_prog_region *prog)
{
	prog->kvirt = NULL;
	prog->dev = NULL;
	prog->n_pages = 0;
	prog->bus_addr = 0;
}

int  dma_prog_region_alloc(struct dma_prog_region *prog, unsigned long n_bytes, struct pci_dev *dev)
{
	/* round up to page size */
	n_bytes = PAGE_ALIGN(n_bytes);

	prog->n_pages = n_bytes >> PAGE_SHIFT;

	prog->kvirt = pci_alloc_consistent(dev, n_bytes, &prog->bus_addr);
	if (!prog->kvirt) {
		printk(KERN_ERR "dma_prog_region_alloc: pci_alloc_consistent() failed\n");
		dma_prog_region_free(prog);
		return -ENOMEM;
	}

	prog->dev = dev;

	return 0;
}

void dma_prog_region_free(struct dma_prog_region *prog)
{
	if (prog->kvirt) {
		pci_free_consistent(prog->dev, prog->n_pages << PAGE_SHIFT, prog->kvirt, prog->bus_addr);
	}

	prog->kvirt = NULL;
	prog->dev = NULL;
	prog->n_pages = 0;
	prog->bus_addr = 0;
}

/* dma_region */

void dma_region_init(struct dma_region *dma)
{
	dma->kvirt = NULL;
	dma->dev = NULL;
	dma->n_pages = 0;
	dma->n_dma_pages = 0;
	dma->sglist = NULL;
}

int dma_region_alloc(struct dma_region *dma, unsigned long n_bytes, struct pci_dev *dev, int direction)
{
	unsigned int i;

	/* round up to page size */
	n_bytes = PAGE_ALIGN(n_bytes);

	dma->n_pages = n_bytes >> PAGE_SHIFT;

	dma->kvirt = vmalloc_32(n_bytes);
	if (!dma->kvirt) {
		printk(KERN_ERR "dma_region_alloc: vmalloc_32() failed\n");
		goto err;
	}

	/* Clear the ram out, no junk to the user */
	memset(dma->kvirt, 0, n_bytes);

	/* allocate scatter/gather list */
	dma->sglist = vmalloc(dma->n_pages * sizeof(*dma->sglist));
	if (!dma->sglist) {
		printk(KERN_ERR "dma_region_alloc: vmalloc(sglist) failed\n");
		goto err;
	}

	/* just to be safe - this will become unnecessary once sglist->address goes away */
	memset(dma->sglist, 0, dma->n_pages * sizeof(*dma->sglist));

	/* fill scatter/gather list with pages */
	for (i = 0; i < dma->n_pages; i++) {
		unsigned long va = (unsigned long) dma->kvirt + (i << PAGE_SHIFT);

		dma->sglist[i].page = vmalloc_to_page((void *)va);
		dma->sglist[i].length = PAGE_SIZE;
	}

	/* map sglist to the IOMMU */
	dma->n_dma_pages = pci_map_sg(dev, dma->sglist, dma->n_pages, direction);

	if (dma->n_dma_pages == 0) {
		printk(KERN_ERR "dma_region_alloc: pci_map_sg() failed\n");
		goto err;
	}

	dma->dev = dev;
	dma->direction = direction;

	return 0;

err:
	dma_region_free(dma);
	return -ENOMEM;
}

void dma_region_free(struct dma_region *dma)
{
	if (dma->n_dma_pages) {
		pci_unmap_sg(dma->dev, dma->sglist, dma->n_pages, dma->direction);
		dma->n_dma_pages = 0;
		dma->dev = NULL;
	}

	vfree(dma->sglist);
	dma->sglist = NULL;

	vfree(dma->kvirt);
	dma->kvirt = NULL;
	dma->n_pages = 0;
}

/* find the scatterlist index and remaining offset corresponding to a
   given offset from the beginning of the buffer */
static inline int dma_region_find(struct dma_region *dma, unsigned long offset, unsigned long *rem)
{
	int i;
	unsigned long off = offset;

	for (i = 0; i < dma->n_dma_pages; i++) {
		if (off < sg_dma_len(&dma->sglist[i])) {
			*rem = off;
			break;
		}

		off -= sg_dma_len(&dma->sglist[i]);
	}

	BUG_ON(i >= dma->n_dma_pages);

	return i;
}

dma_addr_t dma_region_offset_to_bus(struct dma_region *dma, unsigned long offset)
{
	unsigned long rem = 0;

	struct scatterlist *sg = &dma->sglist[dma_region_find(dma, offset, &rem)];
	return sg_dma_address(sg) + rem;
}

void dma_region_sync_for_cpu(struct dma_region *dma, unsigned long offset, unsigned long len)
{
	int first, last;
	unsigned long rem;

	if (!len)
		len = 1;

	first = dma_region_find(dma, offset, &rem);
	last = dma_region_find(dma, offset + len - 1, &rem);

	pci_dma_sync_sg_for_cpu(dma->dev, &dma->sglist[first], last - first + 1, dma->direction);
}

void dma_region_sync_for_device(struct dma_region *dma, unsigned long offset, unsigned long len)
{
	int first, last;
	unsigned long rem;

	if (!len)
		len = 1;

	first = dma_region_find(dma, offset, &rem);
	last = dma_region_find(dma, offset + len - 1, &rem);

	pci_dma_sync_sg_for_device(dma->dev, &dma->sglist[first], last - first + 1, dma->direction);
}

#ifdef CONFIG_MMU

/* nopage() handler for mmap access */

static struct page*
dma_region_pagefault(struct vm_area_struct *area, unsigned long address, int *type)
{
	unsigned long offset;
	unsigned long kernel_virt_addr;
	struct page *ret = NOPAGE_SIGBUS;

	struct dma_region *dma = (struct dma_region*) area->vm_private_data;

	if (!dma->kvirt)
		goto out;

	if ( (address < (unsigned long) area->vm_start) ||
	    (address > (unsigned long) area->vm_start + (dma->n_pages << PAGE_SHIFT)) )
		goto out;

	if (type)
		*type = VM_FAULT_MINOR;
	offset = address - area->vm_start;
	kernel_virt_addr = (unsigned long) dma->kvirt + offset;
	ret = vmalloc_to_page((void*) kernel_virt_addr);
	get_page(ret);
out:
	return ret;
}

static struct vm_operations_struct dma_region_vm_ops = {
	.nopage	= dma_region_pagefault,
};

int dma_region_mmap(struct dma_region *dma, struct file *file, struct vm_area_struct *vma)
{
	unsigned long size;

	if (!dma->kvirt)
		return -EINVAL;

	/* must be page-aligned */
	if (vma->vm_pgoff != 0)
		return -EINVAL;

	/* check the length */
	size = vma->vm_end - vma->vm_start;
	if (size > (dma->n_pages << PAGE_SHIFT))
		return -EINVAL;

	vma->vm_ops = &dma_region_vm_ops;
	vma->vm_private_data = dma;
	vma->vm_file = file;
	vma->vm_flags |= VM_RESERVED;

	return 0;
}

#else /* CONFIG_MMU */

int dma_region_mmap(struct dma_region *dma, struct file *file, struct vm_area_struct *vma)
{
	return -EINVAL;
}

#endif /* CONFIG_MMU */
