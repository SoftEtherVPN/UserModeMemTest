// Types
struct TEST_TYPE
{
	UINT Test;
};

struct UMMT
{
	UINT64 TickStart;				// 開始時刻
	UINT NumThreads;				// スレッド数
	UINT MemBlockPerSize;			// 1 メモリブロックあたりのサイズ
	UINT NumMemBlocks;				// 合計メモリブロック数
	UINT64 TotalMemorySize;			// 合計メモリサイズ

	UINT64 AllocatedMemorySize;		// 確保済みメモリサイズ

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


