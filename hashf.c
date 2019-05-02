/* hashf
 * convert filenames to md5 digests
 *
 * 2015 GROND
 */

// v1.0		2015-12-29: start fresh from previous version
#define HASHF_VERSION	"1.0"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <md5.h>
#include <libgen.h>
#include <sys/stat.h>

#define HASHF_BUFSZ		4096
#define HASHF_LOGFILE	"/home/colvin/store/logs/hashf.log"

typedef struct {
	int process;
	int isdir;
	char orig[PATH_MAX];
	char full[PATH_MAX];
	char path[PATH_MAX];
	char name[PATH_MAX];
	char suffix[PATH_MAX];
	char digest[4096];
	char new[PATH_MAX];
	void *next;
} target_t;


int			fileinfo(target_t *target);
target_t	*processdir(target_t *target);
int			computehash(target_t *target);
void		usage(void);

int qflag = 0;
int dflag = 0;

int main(int argc, char **argv)
{
	int ch;
	int Nflag = 0;
	int pflag = 0;
	int oflag = 0;

	char custdest[PATH_MAX];

	FILE *from;
	FILE *to;

	target_t *first;
	target_t *foo;
	target_t *bar;
	target_t *tmp;

	if (argc < 2) {
		usage();
		exit(1);
	}

	first = malloc(sizeof(target_t));
	foo = first;

	opterr = 0;
	while ((ch = getopt(argc,argv,"vhNqdpo:")) != -1) {
		switch(ch) {
			case 'v':	printf("%s\n",HASHF_VERSION);
						exit(0);
			case 'h':	usage();
						exit(0);
			case 'N':	Nflag = 1;
						break;
			case 'q':	qflag = 1;
						break;
			case 'd':	dflag = 1;
						break;
			case 'p':	pflag = 1;
						break;
			case 'o':	oflag = 1;
						realpath(optarg,custdest);
						break;
			default:	usage();
						exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if ((qflag) && (dflag))
		dflag = 0;	/* quiet flag trumps */

	/* build linked list of targets */
	for (int i = 0; i < argc; i++) {
		snprintf(foo->orig,sizeof(foo->orig),"%s",argv[i]);
		foo->next = (target_t *)malloc(sizeof(target_t));
		foo = foo->next;
	}
	foo = first;

	/* process the targets */
	while (foo->next != NULL) {
		if (access(foo->orig,F_OK) != -1) {
			if (fileinfo(foo)) {
				/* if a directory was specified, open the directory and process the entries */
				if (foo->isdir) {
					target_t *new;
					if ((new = processdir(foo)) != NULL) {
						/* patch the new linked-list into the existing list */
						bar = foo->next;
						foo->next = new;
						tmp = new;
						while (tmp->next != NULL) {
							tmp = tmp->next;
						} 
						tmp->next = bar;
						foo = tmp;
					}
				}
			} else {
				(qflag) || printf("[!]: error from fileinfo() on %s\n",foo->orig);
			}
		} else {
			(qflag) || printf("[!]: file does not exist: %s\n",foo->orig);
		}
		foo = foo->next;
	}
	foo = first;

	if (dflag) {
		while (foo->next != NULL) {
			if (foo->process) {
				printf("target: %s\n",foo->full);
			}
			foo = foo->next;
		}
		foo = first;
	}

	while (foo->next != NULL) {
		if (foo->process) {
			if (computehash(foo)) {
				if (oflag) {
					snprintf(foo->new,sizeof(foo->new),"%s/%s%s",custdest,foo->digest,foo->suffix);
				} else {
					snprintf(foo->new,sizeof(foo->new),"%s/%s%s",foo->path,foo->digest,foo->suffix);
				}
				if (strcmp(foo->full,foo->new) != 0) {
					if (!Nflag) {
						if ((from = fopen(foo->full,"rb")) != NULL) {
							if ((to = fopen(foo->new,"wb")) != NULL) {
								(qflag) || printf("%s --> ",foo->full);
								int cp;
								while ((cp = fgetc(from)) != EOF) {
									fputc(cp,to);
								}
								fclose(to);
								(qflag) || printf("%s\n",foo->new);
							} else {
								(qflag) || printf("[!]: failed to open file for writing: %s\n",foo->new);
							}
							fclose(from);
							if (!pflag) {
								unlink(foo->full);
							}
						} else {
							(qflag) || printf("[!]: failed to open file for reading: %s\n",foo->full);
						}
					} else {
						printf("noop: %s --> %s\n",foo->full,foo->new);
					}
				} else {
					(qflag) || printf("[!]: source and dest are the same file: %s\n",foo->new);
				}
			} else {
				(qflag) || printf("[!]: error from computehash() on %s\n",foo->full);
			}
		}
		foo = foo->next;
	}
	foo = first;

	return 0;
}



/* accepts a target_t struct pointer
 * populates the members of the struct with path info
 * returns 1 on success, 0 on failure */
int fileinfo(target_t *target)
{
	int r = 0;
	char *tmp;
	char *sufx;
	struct stat st;

	if (realpath(target->orig,target->full)) {
		stat(target->orig,&st);
		if (S_ISDIR(st.st_mode)) {
			target->isdir = 1;
			target->process = 0;
		} else {
			target->isdir = 0;
			target->process = 1;
		}
		if ((tmp = basename(target->full)) != NULL) {
			snprintf(target->name,sizeof(target->name),"%s",tmp);
			if ((tmp = dirname(target->full)) != NULL) {
				snprintf(target->path,sizeof(target->path),"%s",tmp);
				tmp = strrchr(target->name,'.');
				sufx = "";
				if (tmp) {
					if (tmp != target->name) {	/* target begins with dot */
						sufx = tmp;
					}
				}
				snprintf(target->suffix,sizeof(target->suffix),"%s",sufx);
				r = 1;
			}
		}
	}
	return r;
}



/* accepts a target_t struct pointer for a directory
 *  that has been specified by the user
 * it returns a target_t struct pointer to the beginning
 *  of a linked-list of target_t stucts for valid entries within
 *  that directory to be included in the target list */
target_t *processdir(target_t *specified)
{
	DIR *dh;
	struct dirent *entity;
	struct stat st;

	target_t *r = NULL;
	target_t *store = NULL;

	int entries = 0;
	int count = 0;

	//printf("===> processdir(): begin: %s\n",specified->full);
	if ((dh = opendir(specified->full))) {
		while ((entity = readdir(dh)) != NULL) {
			if ((strcmp(entity->d_name,".") != 0) && (strcmp(entity->d_name,"..") != 0)) {
				char *full = malloc(sizeof(char) * PATH_MAX);
				snprintf(full,(sizeof(char) * PATH_MAX),"%s/%s",specified->full,entity->d_name);
				stat(full,&st);
				if (!S_ISDIR(st.st_mode)) {
					target_t *tmp = (target_t *)malloc(sizeof(target_t));
					if (store) {
						store->next = tmp;
						store = tmp;
					} else {
						r = tmp;
						store = tmp;
						store->next = NULL;
					}
					snprintf(store->orig,sizeof(store->orig),"%s",full);
					fileinfo(store);
					entries++;
				}
				free(full);
			}
		}
	} else {
		(qflag) || printf("[!]: failed to open directory: %s\n",specified->orig);
		r = NULL;
	}

	if (entries == 0) {
		r = NULL;
		(qflag) || printf("[!]: no eligible files in directory: %s\n",specified->full);
	}
	
	return r;
}



/* accepts a target_t struct pointer
 * populates the digest member of the struct with the
 *  md5 digest of the file
 * returns 1 on success, 0 on failure */
int computehash(target_t *target)
{
	int r = 0;
	int sz;
	int completed = 0;
	FILE *fp;
	unsigned char buf[HASHF_BUFSZ];
	unsigned char intd[MD5_DIGEST_LENGTH];
	char digest[33];

	struct stat st;
	struct MD5Context md5;

	MD5Init(&md5);
	stat(target->full,&st);

	if (st.st_size < HASHF_BUFSZ) {
		sz = st.st_size;
	} else {
		sz = HASHF_BUFSZ;
	}

	if ((fp = fopen(target->full,"r"))) {
		while (fread(buf,1,sz,fp) != 0) {
			MD5Update(&md5,buf,sz);
			completed += sz;
			if ((completed + sz) > st.st_size) {
				sz = (st.st_size - completed);
			}
		}
		fclose(fp);
		MD5Final(intd,&md5);
		for (int i = 0; i < 16; ++i) {
			sprintf(&digest[i*2], "%02x", (unsigned int)intd[i]);
		}
		snprintf(target->digest,sizeof(target->digest),"%s",digest);
		r = 1;
	}

	return r;
}


/* print usage message */
void usage(void)
{
	printf("usage: hashf [-qdp] [-o <directory>] target [target [...]]\n");
	printf("   -h: this help\n");
	printf("   -q: suppress output\n");
	printf("   -d: debugging output\n");
	printf("   -p: preserve original file\n");
	printf("   -o: put resultant file in the given directory\n");
}
