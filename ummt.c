#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>
#include <locale.h>
#include <time.h>
#include <errno.h>

#include <seclib.h>
#include "project.h"

// メモリテスト実行
void UmmtMemoryTestMain(UMMT *t, UCHAR *buf)
{
	UINT64 pattern;
	UINT i, count;
	UINT64 *buf2;
	if (t == NULL || buf == NULL)
	{
		return;
	}

	// ランダムパターンの生成
	pattern = Rand64();

	count = (UINT)(t->MemBlockPerSize / 8);

	buf2 = (UINT64 *)buf;

	// 書き込み
	for (i = 0;i < count;i++)
	{
		buf2[i] = pattern;
	}
	
	// 読み取り
	for (i = 0;i < count;i++)
	{
		if (buf2[i] != pattern)
		{
			char tmp[MAX_SIZE];
			UINT64 data = buf2[i];

			Format(tmp, sizeof(tmp), "UmmtMemoryTestMain: Pattern read error: 0x%p: %I64u != %I64u\n",
				&buf2[i], data, pattern);

			AbortExitEx(tmp);
		}
	}

	t->TotalReadWriteSize += (UINT64)t->MemBlockPerSize;
}

// UMMT メイン処理スレッド
void UmmtThread(THREAD *thread, void *param)
{
	UMMT *t = (UMMT *)param;
	if (t == NULL)
	{
		return;
	}

#ifdef OS_WIN32
	MsSetThreadPriorityIdle();
#else // OS_WIN32
	UnixSetThreadPriorityIdle();
#endif // OS_WIN32`


	// 開始まで待機
	while (t->Start == false)
	{
		SleepThread(1);
	}

	while (true)
	{
		// Queue1 からメモリブロックを 1 つ取得する
		UCHAR *buf;

LABEL_RETRY:
		buf = NULL;
		LockQueue(t->Queue1);
		{
			buf = GetNext(t->Queue1);

			if (buf == NULL)
			{
				// Queue2 にあるメモリブロックを Queue1 にすべて移動する
				while (true)
				{
					buf = GetNext(t->Queue2);
					if (buf == NULL)
					{
						break;
					}
					InsertQueue(t->Queue1, buf);
				}

				buf = GetNext(t->Queue1);
			}
		}
		UnlockQueue(t->Queue1);

		if (buf == NULL)
		{
			// 処理すべきメモリブロックがない
			SleepThread(1);
			goto LABEL_RETRY;
		}

		// メモリブロックに対してメモリテストを実行
		UmmtMemoryTestMain(t, buf);

		// メモリテストが完了したら Queue2 に入れる
		LockQueue(t->Queue1);
		{
			InsertQueue(t->Queue2, buf);
		}
		UnlockQueue(t->Queue1);
	}
}

// メモリアロケーション用スレッド
void UmmtMemoryAllocator(THREAD *thread, void *param)
{
	UMMT *t = (UMMT *)param;
	UINT i;
	if (t == NULL)
	{
		return;
	}

	for (i = 0;i < t->NumMemBlocks;i++)
	{
		UCHAR *buf = Malloc(t->MemBlockPerSize);

		memset(buf, 1, t->MemBlockPerSize);

		Add(t->MemBlockList, buf);

		t->AllocatedMemorySize += (UINT64)t->MemBlockPerSize;

		SleepThread(1);
	}
}

// UMMT メイン
void UmmtMain(UINT num_threads, UINT64 mem_size)
{
	char tmp[MAX_SIZE];
	char tmp2[MAX_SIZE];
	UMMT *t = ZeroMalloc(sizeof(UMMT));
	THREAD *alloc_thread;
	UINT i;

	if (num_threads == 0)
	{
		num_threads = GetNumberOfCpu();
		if (num_threads == 0)
		{
			num_threads = 1;
		}
	}

	if (mem_size == 0)
	{
		MEMINFO mi;

		GetMemInfo(&mi);

		mem_size = mi.FreePhys;

		if (mem_size >= 3000000000ULL)
		{
			mem_size -= 3000000000ULL;
		}
	}

	// test
	//mem_size = 2000000000ULL;

	t->NumThreads = num_threads;
	t->MemBlockPerSize = MEM_BLOCK_PER_SIZE;
	t->NumMemBlocks = (UINT)(mem_size / t->MemBlockPerSize);
	if (t->NumMemBlocks == 0)
	{
		t->NumMemBlocks = t->NumThreads;
	}
	t->TotalMemorySize = t->MemBlockPerSize * (UINT64)t->NumMemBlocks;

	// 案内の表示
	Print("User Mode Memory Test\n\n");
	Print("Number of Threads: %u\n", t->NumThreads);

	ToStrByte(tmp, sizeof(tmp), t->TotalMemorySize);
	ToStr3(tmp2, sizeof(tmp2), t->TotalMemorySize);
	Print("Target Memory Size: %s (%s bytes)\n\n", tmp, tmp2);

	Print("Allocating memory blocks ...\n");
	t->MemBlockList = NewList(NULL);

	alloc_thread = NewThread(UmmtMemoryAllocator, t);
	while (true)
	{
		char tmp3[MAX_SIZE];
		bool finished;
		double percent;
		
		finished = WaitThread(alloc_thread, 1000);

		percent = (double)t->AllocatedMemorySize / (double)t->TotalMemorySize * 100.0f;
		ToStrByte(tmp3, sizeof(tmp3), t->AllocatedMemorySize);
		Print("%s / %s completed (%.1f %%)\n", tmp3, tmp, percent);

		if (finished)
		{
			break;
		}
	}
	ReleaseThread(alloc_thread);

	t->Queue1 = NewQueue();
	t->Queue2 = NewQueue();

	for (i = 0;i < LIST_NUM(t->MemBlockList);i++)
	{
		UCHAR *buf = LIST_DATA(t->MemBlockList, i);

		InsertQueue(t->Queue1, buf);
	}

	Print("Memory allocation done.\n");

	Print("Creating test threads... ");

	for (i = 0;i < t->NumThreads;i++)
	{
		NewThread(UmmtThread, t);
	}

	Print("done.\n");

	Print("Threads start.\n");
	t->Start = true;
	t->TickStart = Tick64();

	while (true)
	{
		char tmp1[MAX_SIZE];
		char tmp2[MAX_SIZE];
		char tmp3[MAX_SIZE];
		char tmp4[MAX_SIZE];
		UINT64 now = Tick64();
		double secs = (double)(now - t->TickStart) / 1000.0;
		double byte_per_secs;
		if (secs == 0.0)
		{
			secs = 1.0;
		}

		GetSpanStr(tmp4, sizeof(tmp4), now - t->TickStart);

		byte_per_secs = (double)t->TotalReadWriteSize / secs;

		ToStrByte(tmp1, sizeof(tmp1), t->TotalReadWriteSize);
		ToStr3(tmp2, sizeof(tmp2), t->TotalReadWriteSize);
		ToStrByte(tmp3, sizeof(tmp3), (UINT64)byte_per_secs);
		Print("MemTest %s (%s), Avg %s/s (%s)\n", tmp1, tmp2, tmp3, tmp4);

		SleepThread(1000);
	}
}

// Test function definition list
void test(UINT num, char **arg)
{
	UmmtMain(0, 0);
}

typedef void (TEST_PROC)(UINT num, char **arg);

typedef struct TEST_LIST
{
	char *command_str;
	TEST_PROC *proc;
} TEST_LIST;

TEST_LIST test_list[] =
{
	{ "test", test },
};

// Test function
void TestMain(char *cmd)
{
	char tmp[MAX_SIZE];
	bool first = true;
	bool exit_now = false;

	Print("Test Program\n");

#ifdef	OS_WIN32
	MsSetEnableMinidump(false);
#endif	// OS_WIN32
	while (true)
	{
		Print("TEST>");
		if (first && StrLen(cmd) != 0 && g_memcheck == false)
		{
			first = false;
			StrCpy(tmp, sizeof(tmp), cmd);
			exit_now = true;
			Print("%s\n", cmd);
		}
		else
		{
			GetLine(tmp, sizeof(tmp));
		}
		Trim(tmp);
		if (StrLen(tmp) != 0)
		{
			UINT i, num;
			bool b = false;
			TOKEN_LIST *token = ParseCmdLine(tmp);
			char *cmd = token->Token[0];
			if (!StrCmpi(cmd, "exit") || !StrCmpi(cmd, "quit") || !StrCmpi(cmd, "q"))
			{
				FreeToken(token);
				break;
			}
			else
			{
				num = sizeof(test_list) / sizeof(TEST_LIST);
				for (i = 0;i < num;i++)
				{
					if (!StrCmpi(test_list[i].command_str, cmd))
					{
						char **arg = Malloc(sizeof(char *) * (token->NumTokens - 1));
						UINT j;
						for (j = 0;j < token->NumTokens - 1;j++)
						{
							arg[j] = CopyStr(token->Token[j + 1]);
						}
						test_list[i].proc(token->NumTokens - 1, arg);
						for (j = 0;j < token->NumTokens - 1;j++)
						{
							Free(arg[j]);
						}
						Free(arg);
						b = true;
						Print("\n");
						break;
					}
				}
				if (b == false)
				{
					Print("Invalid Command: %s\n\n", cmd);
				}
			}
			FreeToken(token);

			if (exit_now)
			{
				break;
			}
		}
	}
	Print("Exiting...\n\n");
}

// Entry point
int main(int argc, char *argv[])
{
	bool memchk = false;
	bool is_service_mode = false;
	UINT64 size = 0;
	UINT threads = 0;

	SetHamMode();
	MayaquaMinimalMode();

	InitMayaqua(memchk, false, argc, argv);
	EnableProbe(false);
	InitCedar();
	SetHamMode();

#ifdef OS_WIN32
	Win32SetLowPriority();
#endif // OS_WIN32


	//TestMain(cmdline);

	if (argc >= 2)
	{
		size = ToInt64(argv[1]);

		if (argc >= 3)
		{
			threads = ToInt(argv[2]);
		}
	}

	UmmtMain(threads, size);

	FreeCedar();
	FreeMayaqua();

	return 0;
}

