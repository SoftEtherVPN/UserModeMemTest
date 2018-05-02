// Types
struct TEST_TYPE
{
	UINT Test;
};

struct UMMT
{
	UINT64 TickStart;				// �J�n����
	UINT NumThreads;				// �X���b�h��
	UINT MemBlockPerSize;			// 1 �������u���b�N������̃T�C�Y
	UINT NumMemBlocks;				// ���v�������u���b�N��
	UINT64 TotalMemorySize;			// ���v�������T�C�Y

	UINT64 AllocatedMemorySize;		// �m�ۍς݃������T�C�Y

	LIST *MemBlockList;
	QUEUE *Queue1;
	QUEUE *Queue2;
	bool Start;

	UINT64 TotalReadWriteSize;
};

// Consts

#define MEM_BLOCK_PER_SIZE		((UINT)(1024 * 1024 * 100))

// Functions
int main(int argc, char *argv[]);

void UmmtMain(UINT num_threads, UINT64 mem_size);
void UmmtMemoryAllocator(THREAD *thread, void *param);
void UmmtThread(THREAD *thread, void *param);
void UmmtMemoryTestMain(UMMT *t, UCHAR *buf);


