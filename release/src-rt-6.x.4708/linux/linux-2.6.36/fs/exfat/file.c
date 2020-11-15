// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 */

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/cred.h>
#else
#include <linux/compat.h>
#endif
#include <linux/blkdev.h>

#include "exfat_raw.h"
#include "exfat_fs.h"

static int exfat_cont_expand(struct inode *inode, loff_t size)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t start = i_size_read(inode), count = size - i_size_read(inode);
	int err, err2;

	err = generic_cont_expand_simple(inode, size);
	if (err)
		return err;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	inode->i_ctime = inode->i_mtime = current_time(inode);
#else
	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
#endif
	mark_inode_dirty(inode);

	if (!IS_SYNC(inode))
		return 0;

	err = filemap_fdatawrite_range(mapping, start, start + count - 1);
	err2 = sync_mapping_buffers(mapping);
	if (!err)
		err = err2;
	err2 = write_inode_now(inode, 1);
	if (!err)
		err = err2;
	if (err)
		return err;

	return filemap_fdatawait_range(mapping, start, start + count - 1);
}

static bool exfat_allow_set_time(struct exfat_sb_info *sbi, struct inode *inode)
{
	mode_t allow_utime = sbi->options.allow_utime;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	if (!uid_eq(current_fsuid(), inode->i_uid)) {
#else
	if (current_fsuid() != inode->i_uid) {
#endif
		if (in_group_p(inode->i_gid))
			allow_utime >>= 3;
		if (allow_utime & MAY_WRITE)
			return true;
	}

	/* use a default check */
	return false;
}

static int exfat_sanitize_mode(const struct exfat_sb_info *sbi,
		struct inode *inode, umode_t *mode_ptr)
{
	mode_t i_mode, mask, perm;

	i_mode = inode->i_mode;

	mask = (S_ISREG(i_mode) || S_ISLNK(i_mode)) ?
		sbi->options.fs_fmask : sbi->options.fs_dmask;
	perm = *mode_ptr & ~(S_IFMT | mask);

	/* Of the r and x bits, all (subject to umask) must be present.*/
	if ((perm & 0555) != (i_mode & 0555))
		return -EPERM;

	if (exfat_mode_can_hold_ro(inode)) {
		/*
		 * Of the w bits, either all (subject to umask) or none must
		 * be present.
		 */
		if ((perm & 0222) && ((perm & 0222) != (0222 & ~mask)))
			return -EPERM;
	} else {
		/*
		 * If exfat_mode_can_hold_ro(inode) is false, can't change
		 * w bits.
		 */
		if ((perm & 0222) != (0222 & ~mask))
			return -EPERM;
	}

	*mode_ptr &= S_IFMT | perm;

	return 0;
}

/* resize the file length */
int __exfat_truncate(struct inode *inode, loff_t new_size)
{
	unsigned int num_clusters_new, num_clusters_phys;
	unsigned int last_clu = EXFAT_FREE_CLUSTER;
	struct exfat_chain clu;
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct exfat_inode_info *ei = EXFAT_I(inode);
	int evict = (ei->dir.dir == DIR_DELETED) ? 1 : 0;

	/* check if the given file ID is opened */
	if (ei->type != TYPE_FILE && ei->type != TYPE_DIR)
		return -EPERM;

	exfat_set_vol_flags(sb, VOL_DIRTY);

	num_clusters_new = EXFAT_B_TO_CLU_ROUND_UP(i_size_read(inode), sbi);
	num_clusters_phys =
		EXFAT_B_TO_CLU_ROUND_UP(EXFAT_I(inode)->i_size_ondisk, sbi);

	exfat_chain_set(&clu, ei->start_clu, num_clusters_phys, ei->flags);

	if (new_size > 0) {
		/*
		 * Truncate FAT chain num_clusters after the first cluster
		 * num_clusters = min(new, phys);
		 */
		unsigned int num_clusters =
			min(num_clusters_new, num_clusters_phys);

		/*
		 * Follow FAT chain
		 * (defensive coding - works fine even with corrupted FAT table
		 */
		if (clu.flags == ALLOC_NO_FAT_CHAIN) {
			clu.dir += num_clusters;
			clu.size -= num_clusters;
		} else {
			while (num_clusters > 0) {
				last_clu = clu.dir;
				if (exfat_get_next_cluster(sb, &(clu.dir)))
					return -EIO;

				num_clusters--;
				clu.size--;
			}
		}
	} else {
		ei->flags = ALLOC_NO_FAT_CHAIN;
		ei->start_clu = EXFAT_EOF_CLUSTER;
	}

	i_size_write(inode, new_size);

	if (ei->type == TYPE_FILE)
		ei->attr |= ATTR_ARCHIVE;

	/* update the directory entry */
	if (!evict) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
		struct timespec64 ts;
#else
		struct timespec ts;
#endif
		struct exfat_dentry *ep, *ep2;
		struct exfat_entry_set_cache *es;
		int err;

		es = exfat_get_dentry_set(sb, &(ei->dir), ei->entry,
				ES_ALL_ENTRIES);
		if (!es)
			return -EIO;
		ep = exfat_get_dentry_cached(es, 0);
		ep2 = exfat_get_dentry_cached(es, 1);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
		ts = current_time(inode);
#else
		ts = CURRENT_TIME_SEC;
#endif
		exfat_set_entry_time(sbi, &ts,
				&ep->dentry.file.modify_tz,
				&ep->dentry.file.modify_time,
				&ep->dentry.file.modify_date,
				&ep->dentry.file.modify_time_cs);
		ep->dentry.file.attr = cpu_to_le16(ei->attr);

		/* File size should be zero if there is no cluster allocated */
		if (ei->start_clu == EXFAT_EOF_CLUSTER) {
			ep2->dentry.stream.valid_size = 0;
			ep2->dentry.stream.size = 0;
		} else {
			ep2->dentry.stream.valid_size = cpu_to_le64(new_size);
			ep2->dentry.stream.size = ep->dentry.stream.valid_size;
		}

		if (new_size == 0) {
			/* Any directory can not be truncated to zero */
			WARN_ON(ei->type != TYPE_FILE);

			ep2->dentry.stream.flags = ALLOC_FAT_CHAIN;
			ep2->dentry.stream.start_clu = EXFAT_FREE_CLUSTER;
		}

		exfat_update_dir_chksum_with_entry_set(es);
		err = exfat_free_dentry_set(es, inode_needs_sync(inode));
		if (err)
			return err;
	}

	/* cut off from the FAT chain */
	if (ei->flags == ALLOC_FAT_CHAIN && last_clu != EXFAT_FREE_CLUSTER &&
			last_clu != EXFAT_EOF_CLUSTER) {
		if (exfat_ent_set(sb, last_clu, EXFAT_EOF_CLUSTER))
			return -EIO;
	}

	/* invalidate cache and free the clusters */
	/* clear exfat cache */
	exfat_cache_inval_inode(inode);

	/* hint information */
	ei->hint_bmap.off = EXFAT_EOF_CLUSTER;
	ei->hint_bmap.clu = EXFAT_EOF_CLUSTER;
	if (ei->rwoffset > new_size)
		ei->rwoffset = new_size;

	/* hint_stat will be used if this is directory. */
	ei->hint_stat.eidx = 0;
	ei->hint_stat.clu = ei->start_clu;
	ei->hint_femp.eidx = EXFAT_HINT_NONE;

	/* free the clusters */
	if (exfat_free_cluster(inode, &clu))
		return -EIO;

	exfat_set_vol_flags(sb, VOL_CLEAN);

	return 0;
}

void exfat_truncate(struct inode *inode, loff_t size)
{
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	unsigned int blocksize = 1 << inode->i_blkbits;
	loff_t aligned_size;
	int err;

	mutex_lock(&sbi->s_lock);
	if (EXFAT_I(inode)->start_clu == 0) {
		/*
		 * Empty start_clu != ~0 (not allocated)
		 */
		exfat_fs_error(sb, "tried to truncate zeroed cluster.");
		goto write_size;
	}

	err = __exfat_truncate(inode, i_size_read(inode));
	if (err)
		goto write_size;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	inode->i_ctime = inode->i_mtime = current_time(inode);
#else
	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
#endif
	if (IS_DIRSYNC(inode))
		exfat_sync_inode(inode);
	else
		mark_inode_dirty(inode);

	inode->i_blocks = ((i_size_read(inode) + (sbi->cluster_size - 1)) &
			~(sbi->cluster_size - 1)) >> inode->i_blkbits;
write_size:
	aligned_size = i_size_read(inode);
	if (aligned_size & (blocksize - 1)) {
		aligned_size |= (blocksize - 1);
		aligned_size++;
	}

	if (EXFAT_I(inode)->i_size_ondisk > i_size_read(inode))
		EXFAT_I(inode)->i_size_ondisk = aligned_size;

	if (EXFAT_I(inode)->i_size_aligned > i_size_read(inode))
		EXFAT_I(inode)->i_size_aligned = aligned_size;
	mutex_unlock(&sbi->s_lock);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
int exfat_getattr(const struct path *path, struct kstat *stat,
		unsigned int request_mask, unsigned int query_flags)
#else
int exfat_getattr(struct vfsmount *mnt, struct dentry *dentry,
		struct kstat *stat)
#endif

{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	struct inode *inode = d_backing_inode(path->dentry);
	struct exfat_inode_info *ei = EXFAT_I(inode);
#else
	struct inode *inode = dentry->d_inode;
#endif

	generic_fillattr(inode, stat);
	exfat_truncate_atime(&stat->atime);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	stat->result_mask |= STATX_BTIME;
	stat->btime.tv_sec = ei->i_crtime.tv_sec;
	stat->btime.tv_nsec = ei->i_crtime.tv_nsec;
#endif
	stat->blksize = EXFAT_SB(inode->i_sb)->cluster_size;
	return 0;
}

int exfat_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct exfat_sb_info *sbi = EXFAT_SB(dentry->d_sb);
	struct inode *inode = dentry->d_inode;
	unsigned int ia_valid;
	int error;

	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size > i_size_read(inode)) {
		error = exfat_cont_expand(inode, attr->ia_size);
		if (error || attr->ia_valid == ATTR_SIZE)
			return error;
		attr->ia_valid &= ~ATTR_SIZE;
	}

	/* Check for setting the inode time. */
	ia_valid = attr->ia_valid;
	if ((ia_valid & (ATTR_MTIME_SET | ATTR_ATIME_SET | ATTR_TIMES_SET)) &&
	    exfat_allow_set_time(sbi, inode)) {
		attr->ia_valid &= ~(ATTR_MTIME_SET | ATTR_ATIME_SET |
				ATTR_TIMES_SET);
	}

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)) && \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 37))) || \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	error = setattr_prepare(dentry, attr);
#else
	error = inode_change_ok(inode, attr);
#endif
	attr->ia_valid = ia_valid;
	if (error)
		goto out;

	if (((attr->ia_valid & ATTR_UID) &&
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
		 (!uid_eq(attr->ia_uid, sbi->options.fs_uid))) ||
		((attr->ia_valid & ATTR_GID) &&
		 (!gid_eq(attr->ia_gid, sbi->options.fs_gid))) ||
#else
		 (attr->ia_uid != sbi->options.fs_uid)) ||
		((attr->ia_valid & ATTR_GID) &&
		 (attr->ia_gid != sbi->options.fs_gid)) ||
#endif
	    ((attr->ia_valid & ATTR_MODE) &&
	     (attr->ia_mode & ~(S_IFREG | S_IFLNK | S_IFDIR | 0777)))) {
		error = -EPERM;
		goto out;
	}

	/*
	 * We don't return -EPERM here. Yes, strange, but this is too
	 * old behavior.
	 */
	if (attr->ia_valid & ATTR_MODE) {
		if (exfat_sanitize_mode(sbi, inode, &attr->ia_mode) < 0)
			attr->ia_valid &= ~ATTR_MODE;
	}

	if (attr->ia_valid & ATTR_SIZE) {
		error = exfat_block_truncate_page(inode, attr->ia_size);
		if (error)
			goto out;

		down_write(&EXFAT_I(inode)->truncate_lock);
		truncate_setsize(inode, attr->ia_size);
		exfat_truncate(inode, attr->ia_size);
		up_write(&EXFAT_I(inode)->truncate_lock);
	}

	setattr_copy(inode, attr);
	exfat_truncate_atime(&inode->i_atime);
	mark_inode_dirty(inode);

out:
	return error;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
int exfat_file_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = filp->f_mapping->host;
	int err;

	err = __generic_file_fsync(filp, start, end, datasync);
	if (err)
		return err;

	err = sync_blockdev(inode->i_sb->s_bdev);
	if (err)
		return err;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
	return blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL);
#else
	return blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL, NULL);
#endif
}
#endif

const struct file_operations exfat_file_operations = {
	.llseek		= generic_file_llseek,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
	.read        = do_sync_read,
	.write       = do_sync_write,
	.aio_read    = generic_file_aio_read,
	.aio_write   = generic_file_aio_write,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0)
	.read        = new_sync_read,
	.write       = new_sync_write,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
#endif
	.mmap		= generic_file_mmap,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
	.fsync		= generic_file_fsync,
#else
	.fsync		= exfat_file_fsync,
#endif
	.splice_read	= generic_file_splice_read,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
	.splice_write	= generic_file_splice_write,
#else
	.splice_write	= iter_file_splice_write,
#endif
};

const struct inode_operations exfat_file_inode_operations = {
	.setattr     = exfat_setattr,
	.getattr     = exfat_getattr,
};
