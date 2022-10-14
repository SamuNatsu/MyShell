#include <pwd.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <readline/history.h>
#include <readline/readline.h>

// ======== Reusable Link List ========

// Link node structure
struct LinkNode {
	void *data;			// Data field
	struct LinkNode *next;		// Next pointer
};

// Link node deleter prototype
typedef void (*LinkNodeDeleter)(void*);

// Link list structure
struct LinkList {
	struct LinkNode *head, *curTail, *maxTail;	// Head guard pointer, current tail pointer, maximum tail pointer
	LinkNodeDeleter deleter;			// Custom deleter function
	size_t curLen, maxLen;				// Current length, maximum length
};

// Create link list
struct LinkList* LinkList_Create(LinkNodeDeleter deleter) {
	// Allocate memory
	struct LinkList *rtn = (struct LinkList*)malloc(sizeof(struct LinkList));
	if (rtn == NULL)
		return NULL;

	// Allocate head
	rtn->head = rtn->curTail = rtn->maxTail = (struct LinkNode*)malloc(sizeof(struct LinkNode));
	if (rtn->head == NULL) {
		free(rtn);
		return NULL;
	}

	// Initialize
	rtn->head->data = NULL;
	rtn->head->next = NULL;
	rtn->deleter = deleter;
	rtn->curLen = rtn->maxLen = 0;
	return rtn;
}

// Get size
size_t LinkList_Size(struct LinkList* ptr) {
	return ptr == NULL ? 0 : ptr->curLen;
}

// Get begin iterator
struct LinkNode* LinkList_Begin(struct LinkList* ptr) {
	return ptr == NULL || ptr->curLen == 0 ? NULL : ptr->head->next;
}

// Get next iterator
struct LinkNode* LinkList_Next(struct LinkList* ptr1, struct LinkNode* ptr2) {
	return ptr1 == NULL || ptr2 == NULL || ptr1->curTail == ptr2 ? NULL : ptr2->next;
}

// Is first iterator
int LinkList_IsFirst(struct LinkList* ptr1, struct LinkNode* ptr2) {
	return ptr1 == NULL || ptr2 != ptr1->head->next ? 0 : 1;
}

// Is last iterator
int LinkList_IsLast(struct LinkList* ptr1, struct LinkNode* ptr2) {
	return ptr1 == NULL || ptr2 != ptr1->curTail ? 0 : 1;
}

// Destroy link list
void LinkList_Destroy(struct LinkList* ptr) {
	// Check not null
	if (ptr == NULL)
		return;

	// Call deleter
	if (ptr->deleter != NULL)
		for (struct LinkNode *i = LinkList_Begin(ptr); i != NULL; i = LinkList_Next(ptr, i))
			ptr->deleter(i->data);

	// Release link memory
	while (ptr->head != NULL) {
		struct LinkNode *tmp = ptr->head;
		ptr->head = ptr->head->next;
		free(tmp);
	}

	// Release structure
	free(ptr);
}

// Extend link list
int LinkList_Extend(struct LinkList* ptr, void* dat) {
	// Check not null
	if (ptr == NULL)
		return -1;

	// Allocate new node
	struct LinkNode *tmp = (struct LinkNode*)malloc(sizeof(struct LinkNode));
	if (tmp == NULL)
		return -2;

	// Initialize
	tmp->data = dat;
	tmp->next = NULL;

	// Link to maximum tail
	ptr->maxTail->next = tmp;
	ptr->maxTail = tmp;
	++ptr->maxLen;
	return 0;
}

// Push back data
int LinkList_Push(struct LinkList* ptr, void* dat) {
	// Check not null
	if (ptr == NULL)
		return -1;

	// Check if need to be extened
	if (ptr->curLen >= ptr->maxLen) {
		// Extend link
		int rtn = LinkList_Extend(ptr, dat);
		if (rtn == 0) {
			ptr->curTail = ptr->maxTail;
			++ptr->curLen;
		}
		return rtn;
	}

	// Push to current tail
	ptr->curTail = ptr->curTail->next;
	ptr->curTail->data = dat;
	++ptr->curLen;
	return 0;
}

// Clear link list
int LinkList_Clear(struct LinkList* ptr) {
	// Check not null
	if (ptr == NULL)
		return -1;

	// Call deleter
	if (ptr->deleter != NULL)
		for (struct LinkNode *i = LinkList_Begin(ptr); i != NULL; i = LinkList_Next(ptr, i))
			ptr->deleter(i->data);

	// Reset current tail pointer & current length
	ptr->curTail = ptr->head;
	ptr->curLen = 0;
	return 0;
}

// ======== End Of Reusable Link List ========


// ======== Easy Pipe ========

// Easy pipe flags
#define EASY_PIPE_STDIN		1
#define EASY_PIPE_STDOUT	2
#define EASY_PIPE_READ		0x1
#define EASY_PIPE_WRITE		0x2

// Easy pipe structure
struct EasyPipe {
	int fd[2];	// Pipe pair
};

// Create easy pipe
struct EasyPipe* EasyPipe_Create() {
	// Allocate memory
	struct EasyPipe *rtn = (struct EasyPipe*)malloc(sizeof(struct EasyPipe));
	if (rtn == NULL)
		return NULL;

	// Create pipe
	if (pipe(rtn->fd) < 0) {
		free(rtn);
		return NULL;
	}
	return rtn;
}

// Close easy pipe
void EasyPipe_Close(struct EasyPipe* ptr, int flag) {
	// Check not null
	if (ptr == NULL)
		return;

	// Close read
	if (flag & EASY_PIPE_READ)
		close(ptr->fd[0]);

	// Close write
	if (flag & EASY_PIPE_WRITE)
		close(ptr->fd[1]);
}

// Destroy easy pipe
void EasyPipe_Destroy(struct EasyPipe* ptr) {
	// Check not null
	if (ptr == NULL)
		return;

	// Close pipes
	EasyPipe_Close(ptr, EASY_PIPE_READ | EASY_PIPE_WRITE);

	// Release structure
	free(ptr);
}

// Connect stdin/stdout
int EasyPipe_Connect(struct EasyPipe* ptr, int flag) {
	// Check not null
	if (ptr == NULL)
		return -1;

	// Connect stdin
	if (flag == EASY_PIPE_STDIN) {
		dup2(ptr->fd[0], STDIN_FILENO);
		EasyPipe_Close(ptr, EASY_PIPE_READ | EASY_PIPE_WRITE);
	}
	// Connect stdout
	else if (flag == EASY_PIPE_STDOUT) {
		dup2(ptr->fd[1], STDOUT_FILENO);
		EasyPipe_Close(ptr, EASY_PIPE_READ | EASY_PIPE_WRITE);
	}
	// Invalid flag
	else
		return -2;
	return 0;
}

// ======== End Of Easy Pipe ========


// Prompt variables
struct passwd *g_PWD;	// Passwd structure
char *g_HostName, *g_UserHost, *g_Dir, *g_Prompt, *g_TmpDir;	// Host name string, User@Host string, Working directory string, Prompt string, Temp directory string


// Pipe variables
struct LinkList *g_List;	// Command list
struct LinkList *g_Pipe;	// Pipe list


// ======== Run Command Utilities ========
// Help list
const char* gc_HelpList[] = {
	"======== Naive bash help list ========\n",
	"cd [path]\tChange directory\n",
	"exit\t\tExit bash\n",
	"help\t\tShow help list\n",
	NULL
};

// Run external command
int runExternal(const char* cmd, struct EasyPipe* in, struct EasyPipe* out) {
	// Fork new process
	pid_t pid;
	if ((pid = fork()) < 0) {
		puts("Error: Cannot fork process");
		return -1;
	}

	// Parent process
	if (pid) {
		// Close unused pipe
		EasyPipe_Close(in, EASY_PIPE_READ);
		EasyPipe_Close(out,  EASY_PIPE_WRITE);

		// Wait for child process
		int status;
		waitpid(pid, &status, 0);

		// If exception occurred OR return value not 0
		if (!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
			return -2;
	}
	// Child process
	else {
		// Connect pipe
		if (in != NULL && EasyPipe_Connect(in, EASY_PIPE_STDIN) < 0) {
			puts("Error: Cannot connect to input pipe");
			exit(1);
		}
		if (out != NULL && EasyPipe_Connect(out, EASY_PIPE_STDOUT) < 0) {
			puts("Error: Cannot connect to output pipe");
			exit(1);
		}

		// Execute command
		execl("/bin/bash", "/bin/bash", "-c", cmd, NULL);
	}
	return 0;
}

// Run command
int runCommand(char* cmd, struct EasyPipe* in, struct EasyPipe* out) {
	// Get command length
	size_t len = strlen(cmd);

	// Get handle
	char *handle = strtok(cmd, " ");

	// Check cd
	if (strcmp(handle, "cd") == 0) {
		// Get path
		char *tmp = strtok(NULL, " ");
		if (tmp[0] == '/')
			sprintf(g_TmpDir, "%s", tmp);
		else
			sprintf(g_TmpDir, "%s/%s", g_Dir, tmp);

		// Change working directory
		if (chdir(g_TmpDir) == 0) {
			char *rtn = getcwd(g_Dir, 1024);
			if (rtn == NULL) {
				puts("Error: Cannot get current working diretory");
				return -1;
			}
			sprintf(g_Prompt, "%s:%s > ", g_UserHost, g_Dir);
		}
		else
			puts("Error: Fail to change directory, please check path");
	}
	// Check exit
	else if (strcmp(handle, "exit") == 0) {
		puts("Bye");
		return -2;
	}
	// Check help
	else if (strcmp(handle, "help") == 0) {
		// Connect to write pipe
		int fd = STDOUT_FILENO;
		if (out != NULL)
			fd = out->fd[1];

		// Print help
		for (const char **i = gc_HelpList; *i != NULL; ++i)
			if (write(fd, *i, strlen(*i)) < 0) {
				puts("Error: Fail to print help list");
				return -3;
			}

		// Close write pipe
		if (out != NULL)
			EasyPipe_Close(out, EASY_PIPE_WRITE);
	}
	// External commands/executables
	else {
		// Recover command
		char* lst = cmd;
		while (*lst != '\0')
			++lst;
		if (lst != cmd + len)
			*lst = ' ';

		// Run external
		int ret = runExternal(cmd, in, out);
		if (ret < 0) {
			printf("Warning: Some error occurred while executing, code: %d\n", ret);
			return 1;
		}
	}
	return 0;
}

// ======== End Of Run Command Utilities ========


// Split command according to pipe
int splitPipe(char* cmd) {
	// Clear list
	if (LinkList_Clear(g_List) < 0)
		return -1;

	// Go through command string
	int flag = 0;
	char *start = cmd, *i = cmd;
	for ( ; *i != '\0'; ++i)
		// Detect quoting
		if (*i == '"' && (i == cmd || *(i - 1) != '\\'))
			flag = flag ^ 1;

		// Detect pipe
		else if (*i == '|' && !flag) {
			*i = '\0';
			if (LinkList_Push(g_List, start) < 0)
				return -2;
			start = i + 1;
		}

	// Last one
	if (start != i)
		if (LinkList_Push(g_List, start) < 0)
			return -2;
	return 0;
}


// ======== Memory Safety Utilities ========

// Allocate memory
void* safeAllocate(size_t size, int errno, const char* errstr) {
	void *rtn = malloc(size);
	if (rtn == NULL) {
		puts(errstr);
		exit(errno);
	}
	return rtn;
}

// Release memory
void safeFree(void* ptr) {
	if (ptr != NULL)
		free(ptr);
}

// Release all global memory
void release() {
	safeFree(g_HostName);
	safeFree(g_UserHost);
	safeFree(g_Dir);
	safeFree(g_Prompt);
	safeFree(g_TmpDir);
	LinkList_Destroy(g_List);
	LinkList_Destroy(g_Pipe);
}

// ======== End Of Memory Safety Utilities ========


// Entrance
int main() {
	// Register release function
	atexit(release);

	// Initialize resources
	if ((g_PWD = getpwuid(getuid())) == NULL) {
		puts("Error: Cannot get current user info");
		return 1;
	}

	g_HostName = (char*)safeAllocate(HOST_NAME_MAX, 2, "Error: Cannot allocate host name buffer");
	if (gethostname(g_HostName, HOST_NAME_MAX) < 0) {
		puts("Error: Cannot get host name");
		return 3;
	}

	g_UserHost = (char*)safeAllocate(512, 4, "Error: Cannot allocate user host buffer");
	sprintf(g_UserHost, "%s@%s", g_PWD->pw_name, g_HostName);

	g_Dir = (char*)safeAllocate(1024, 5, "Error: Cannot allocate directory buffer");
	strcpy(g_Dir, g_PWD->pw_dir);

	g_Prompt = (char*)safeAllocate(2048, 6, "Error: Cannot allocate promt buffer");
	sprintf(g_Prompt, "%s:%s > ", g_UserHost, g_Dir);

	g_TmpDir = (char*)safeAllocate(1024, 7, "Error: Cannot allocate temporary buffer");

	if ((g_List = LinkList_Create(NULL)) == NULL) {
		puts("Error: Cannot create command list buffer");
		return 8;
	}

	if ((g_Pipe = LinkList_Create((LinkNodeDeleter)EasyPipe_Destroy)) == NULL) {
		puts("Error: Cannot create pipe list buffer");
		return 9;
	}

	// Initialize working directory
	if (chdir(g_PWD->pw_dir) < 0) {
		puts("Error: Fail to change current working directory");
		return 10;
	}

	// Main loop
	while (1) {
		// Read command
		char *cmd = readline(g_Prompt);
		if (cmd == NULL || cmd[0] == '\0') {
			safeFree(cmd);
			continue;
		}

		// Add history
		add_history(cmd);

		// Split command
		if (splitPipe(cmd) < 0) {
			puts("Error: Fail to parse pipe info");
			safeFree(cmd);
			continue;
		}

		// Initialize pipe
		if (LinkList_Clear(g_Pipe) < 0) {
			puts("Error: Fail to clear pipe buffer");
			safeFree(cmd);
			continue;
		}
		int flag = 0;
		for (size_t i = 1, lmt = LinkList_Size(g_List); i < lmt; ++i) {
			struct EasyPipe *tmp = EasyPipe_Create();
			if (tmp == NULL) {
				puts("Error: Fail to create pipe");
				flag = 1;
				break;
			}
			if (LinkList_Push(g_Pipe, tmp) < 0) {
				puts("Error: Fail to store pipe");
				flag = 1;
				break;
			}
		}
		if (flag) {
			safeFree(cmd);
			continue;
		}

		// Enumerate command
		flag = 0;
		struct LinkNode *rdPipe = g_Pipe->head;
		struct LinkNode *wrPipe = LinkList_Begin(g_Pipe);
		for (struct LinkNode *i = LinkList_Begin(g_List); i != NULL; i = LinkList_Next(g_List, i)) {
			struct EasyPipe *in = (LinkList_Size(g_Pipe) == 0 || LinkList_IsFirst(g_Pipe, wrPipe) ? NULL : rdPipe->data);
			struct EasyPipe *out = (LinkList_Size(g_Pipe) == 0 || LinkList_IsLast(g_Pipe, rdPipe) ? NULL : wrPipe->data);
			int ret;
			if ((ret = runCommand(i->data, in, out)) < 0) {
				flag = 1;
				break;
			}
			if (ret > 0)
				break;
			rdPipe = wrPipe;
			wrPipe = LinkList_Next(g_Pipe, wrPipe);
		}

		// Check exit flag
		if (flag) {
			safeFree(cmd);
			break;
		}

		// Release command buffer
		safeFree(cmd);
	}
	return 0;
}

