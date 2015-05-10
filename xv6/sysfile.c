//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"


extern struct inode* iget (uint dev, uint inum);
extern uint ichecksum(struct inode *ip);


// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    if(argint(n, &fd) < 0) {
        return -1;
    }

    if(fd < 0 || fd >= NOFILE || (f=proc->ofile[fd]) == 0) {
        return -1;
    }

    if(pfd) {
        *pfd = fd;
    }

    if(pf) {
        *pf = f;
    }

    return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int fdalloc(struct file *f)
{
    int fd;

    for(fd = 0; fd < NOFILE; fd++){
        if(proc->ofile[fd] == 0){
            proc->ofile[fd] = f;
            return fd;
        }
    }

    return -1;
}

int sys_dup(void)
{
    struct file *f;
    int fd;

    if(argfd(0, 0, &f) < 0) {
        return -1;
    }

    if((fd=fdalloc(f)) < 0) {
        return -1;
    }

    filedup(f);

    return fd;
}

int sys_read(void)
{
    struct file *f;
    int n;
    char *p;

    if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0) {
        return -1;
    }

    return fileread(f, p, n);
}

int sys_write(void)
{
    struct file *f;
    int n;
    char *p;

    if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0) {
        return -1;
    }

    return filewrite(f, p, n);
}

int sys_close(void)
{
    int fd;
    struct file *f;

    if(argfd(0, &fd, &f) < 0) {
        return -1;
    }

    proc->ofile[fd] = 0;
    fileclose(f);

    return 0;
}

int sys_fstat(void)
{
    struct file *f;
    struct stat *st;

    if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0) {
        return -1;
    }

    return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int sys_link(void)
{
    char name[DIRSIZ], *new, *old;
    struct inode *dp, *ip;
    int r;

    if(argstr(0, &old) < 0 || argstr(1, &new) < 0) {
        return -1;
    }

    if((ip = namei_trans(old)) == 0) {
        return -1;
    }

    ilock_trans(ip);
    begin_op();


    if(ip->type == T_DIR){
        iunlockput(ip);
        end_op();
        return -1;
    }

    ip->nlink++;
    iupdate(ip);
    iunlock(ip);

    if((dp = nameiparent(new, name)) == 0) {
        goto bad;
    }

    if ((r = ilock(dp)) != 0) {
    	cprintf("ilock failed with r = %d \n", r);
    	panic("sys_link");
    }

    if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
        iunlockput(dp);
        goto bad;
    }

    iunlockput(dp);
    iput(ip);

    end_op();

    return 0;

bad:
    ilock(ip);
    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);
    end_op();
    return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dp)
{
    int off;
    struct dirent de;

    for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
        if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de)) {
            panic("isdirempty: readi");
        }

        if(de.inum != 0) {
            return 0;
        }
    }
    return 1;
}

//PAGEBREAK!
int sys_unlink(void)
{
    struct inode *ip, *dp;
    struct dirent de;
    char name[DIRSIZ], *path;
    uint off;
    int r;

    if(argstr(0, &path) < 0) {
        return -1;
    }

    if((dp = nameiparent_trans(path, name)) == 0) {
        return -1;
    }

    begin_op();

    if ((r = ilock(dp)) < 0) {
		cprintf("ilocked failed with r = %d \n", r);
		panic("sys_unlink");
    }

    // Cannot unlink "." or "..".
    if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
        goto bad;
    }

    if((ip = dirlookup(dp, name, &off)) == 0) {
        goto bad;
    }

    if ((r = ilock_ext(ip, 0)) < 0) {
        cprintf("ilocked failed with r = %d \n", r);
        panic("sys_unlink");
    }

    if(ip->nlink < 1) {
        panic("unlink: nlink < 1");
    }

    if(ip->type == T_DIR && !isdirempty(ip)){
        iunlockput(ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));

    if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de)) {
        panic("unlink: writei");
    }

    if(ip->type == T_DIR){
        dp->nlink--;
        iupdate(dp);
    }

    iunlockput(dp);

    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);

    end_op();

    return 0;

bad:
    iunlockput(dp);
    end_op();
    return -1;
}

static struct inode* create(char *path, short type, short major, short minor)
{
    uint off;
    struct inode *ip, *dp;
    char name[DIRSIZ];
    int d2r; // distance to root directory

    if((dp = nameiparent_trans(path, name)) == 0) {
        return 0;
    }
    d2r = dist2root(path);
    ilock(dp);

    if((ip = dirlookup(dp, name, &off)) != 0){
        iunlockput(dp);
        ilock(ip);

        if(type == T_FILE && ip->type == T_FILE) {
            return ip;
        }

        iunlockput(ip);

        return 0;
    }

    if((ip = ialloc(dp->dev, type)) == 0) {
        panic("create: ialloc");
    }

    ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;

    // hgp: add for ditto inodes
    if (type == T_DIR && d2r < DITTO_HIGHER) {
    	struct inode *child1, *child2;
    	if (d2r < DITTO_LOWER) {  // close to root, create 2 dittos
    		child1 = ialloc(dp->dev, T_DITTO);
    		child2 = ialloc(dp->dev, T_DITTO);
    		ip->child1 = child1->inum;
    		ip->child2 = child2->inum;
    	}
    	else {
    		if (d2r < DITTO_HIGHER) {  // close enough to root, create 1 ditto
    			child1 = ialloc(dp->dev, T_DITTO);
    			ip->child1 = child1->inum;
    		}
    	}
    }

    iupdate(ip);

    if(type == T_DIR){  // Create . and .. entries.
        dp->nlink++;  // for ".."
        iupdate(dp);

        // No ip->nlink++ for ".": avoid cyclic ref count.
        if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0) {
            panic("create dots");
        }
    }

    if(dirlink(dp, name, ip->inum) < 0) {
        panic("create: dirlink");
    }

    iunlockput(dp);

    return ip;
}

int sys_open(void)
{
    char *path;
    int fd, omode;
    struct file *f;
    struct inode *ip;

    if(argstr(0, &path) < 0 || argint(1, &omode) < 0) {
        return -1;
    }

    if(omode & O_CREATE){
        begin_op();
        ip = create(path, T_FILE, 1, 0);  // hgp: change major to 1
        end_op();

        if(ip == 0) {
            return -1;
        }

    } else {
        if((ip = namei_trans(path)) == 0) {
            return -1;
        }

        // hgp: use ilock_trans()
        if (ilock_trans(ip) == E_CORRUPTED)
        	return E_CORRUPTED;

        if(ip->type == T_DIR && omode != O_RDONLY){
            iunlockput(ip);
            return -1;
        }
    }

    if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
        if(f) {
            fileclose(f);
        }

        iunlockput(ip);
        return -1;
    }

    iunlock(ip);

    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

    return fd;
}

// sys_forceopen, the only difference between sys_open and sys_forceopen
// is that it uses ilock_ext() with checksum = 0.
// it will open file without verify checksum
int sys_forceopen(void)
{
	char *path;
	int fd, omode;
	struct file *f;
	struct inode *ip;

	if (argstr(0, &path) < 0 || argint(1, &omode) < 0) {
		return -1;
	}

	if (omode & O_CREATE) {
		begin_op();
		ip = create(path, T_FILE, 1, 0);  //hgp: major is 1
		end_op();

		if (ip == 0) {  // create failed
			return -1;
		}
	}
	else {
		if ((ip = namei_trans(path)) == 0) {  // lookup and return inode of path
			return -1;
		}

		// use ilock_ext() with checksum 0
		// read data from inode
		ilock_ext(ip, 0);
		if (ip->type == T_DIR && omode != O_RDONLY) {
			iunlockput(ip);
			return -1;
		}
	}

	if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
		if (f) {
			fileclose(f);
		}

		iunlockput(ip);
		return -1;
	}

	iunlock(ip);

	f->type = FD_INODE;
	f->ip = ip;
	f->off = 0;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
	return fd;
}

int sys_iopen(void)
{
	int dev;
	int fd, inum, omode=0;
	struct file *f;
	struct inode *ip;
	int r;

	if (argint(0, &dev) < 0 || argint(1, &inum) < 0) {
		return -1;
	}

	if ((ip = iget((uint)dev, inum)) == 0) {
		return -2;
	}

	if ((r = ilock_trans(ip)) == E_CORRUPTED) {
		return E_CORRUPTED;
	}
	else if (r !=0) {
		return -4;
	}

	if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
		if (f) {
			fileclose(f);
		}
		iunlockput(ip);
		return -3;
	}

	iunlock(ip);

	f->type = FD_INODE;
	f->ip = ip;
	f->off = 0;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
	return fd;
}

uint sys_ichecksum(void)
{
	int dev;
	int fd, omode=0, inum;
	struct file *f;
	struct inode *ip;
	uint checksum;
	int r;

	if (argint(0, &dev) < 0 || argint(1, &inum) < 0) {
		return -1;
	}

	if ((ip = iget((uint)dev, inum)) == 0) {
		return -2;
	}

	if ((r = ilock_trans(ip)) == E_CORRUPTED) {
		return E_CORRUPTED;
	}
	else if (r !=0) {
		return -4;
	}

	if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
		if (f) {
			fileclose(f);
		}
		iunlockput(ip);
		return -3;
	}

	checksum = ichecksum(ip);
	iunlock(ip);

	f->type = FD_INODE;
	f->ip = ip;
	f->off = 0;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

	// close file
	proc->ofile[fd] = 0;
	fileclose(f);

	return checksum;
}

static void ipropagate(struct inode *ip)
{
	int max = ((LOGSIZE - 1 -1 -2) / 2) * 512;
	int i = 0;
	int n = ip->size, n1;
	uint off = 0;
	struct inode *ic;

	while (i < n) {
		n1 = n - i;
		if (n1 > max) {
			n1 = max;
		}

		begin_op();
		if (ip->child1) {
			ic = iget(ip->dev, ip->child1);
			ilock_ext(ic, 0);
			iduplicate(ip, ic, off, n1);  // from fs.c
			iunlock(ic);
		}

		end_op();

		begin_op();
		  if (ip->child2) {
			ic = iget(ip->dev, ip->child2);
			ilock_ext(ic, 0);
			iduplicate(ip, ic, off, n1);
			iunlock(ic);
		  }
		end_op();

		off += n1;
		i += n1;
	}
}

static struct inode* duplicate(char *path, int nditto)
{
	struct inode *ip;
	struct inode *child1, *child2;
	if ((ip = namei_trans(path)) == 0) {
		return 0;
	}

	if (ilock_trans(ip) == E_CORRUPTED) {
		return 0;
	}

	begin_op();
	if (nditto > 0) {
		if (ip->child1) {
			return 0;
		}
		child1 = ialloc(ip->dev, T_DITTO);
		ip->child1 = child1->inum;
	}

	if (nditto > 1) {
		if (ip->child2) {
			return 0;
		}
		child2 = ialloc(ip->dev, T_DITTO);
		ip->child2 = child2->inum;
	}
	end_op();

	ipropagate(ip);
	begin_op();
	iupdate(ip);
	iunlockput(ip);
	end_op();

	return ip;
}

int sys_duplicate(void)
{
	char *path;
	int nditto;
	struct inode *ip;

	if (argstr(0, &path) < 0 || argint(1, &nditto) < 0 ||
			(ip=duplicate(path, nditto)) == 0) {
		return -1;
	}
	return 0;
}


int sys_mkdir(void)
{
    char *path;
    struct inode *ip;

    begin_op();

    if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
        end_op();
        return -1;
    }

    iunlockput(ip);
    end_op();

    return 0;
}

int sys_mknod(void)
{
    struct inode *ip;
    char *path;
    int len;
    int major, minor;

    begin_op();

    if((len=argstr(0, &path)) < 0 ||
            argint(1, &major) < 0 || argint(2, &minor) < 0 ||
            (ip = create(path, T_DEV, major, minor)) == 0){

        end_op();
        return -1;
    }

    iunlockput(ip);
    end_op();

    return 0;
}

int sys_chdir(void)
{
    char *path;
    struct inode *ip;

    if(argstr(0, &path) < 0 || (ip = namei(path)) == 0) {
        return -1;
    }

    if(ilock(ip) == E_CORRUPTED) {
    	return E_CORRUPTED;
    }

    if(ip->type != T_DIR){
        iunlockput(ip);
        return -1;
    }

    iunlock(ip);

    iput(proc->cwd);
    proc->cwd = ip;

    return 0;
}

int sys_exec(void)
{
    char *path, *argv[MAXARG];
    int i;
    uint uargv, uarg;

    if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
        return -1;
    }

    memset(argv, 0, sizeof(argv));

    for(i=0;; i++){
        if(i >= NELEM(argv)) {
            return -1;
        }

        if(fetchint(uargv+4*i, (int*)&uarg) < 0) {
            return -1;
        }

        if(uarg == 0){
            argv[i] = 0;
            break;
        }

        if(fetchstr(uarg, &argv[i]) < 0) {
            return -1;
        }
    }

    return exec(path, argv);
}

int sys_pipe(void)
{
    int *fd;
    struct file *rf, *wf;
    int fd0, fd1;

    if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0) {
        return -1;
    }

    if(pipealloc(&rf, &wf) < 0) {
        return -1;
    }

    fd0 = -1;

    if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
        if(fd0 >= 0) {
            proc->ofile[fd0] = 0;
        }

        fileclose(rf);
        fileclose(wf);

        return -1;
    }

    fd[0] = fd0;
    fd[1] = fd1;

    return 0;
}
