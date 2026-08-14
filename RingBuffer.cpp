#include "RingBuffer.h"

#include <cstring>

int RingBuffer::GetUsedSize()
{
	return (tail_ - head_ + BUFFER_SIZE) % BUFFER_SIZE;
}

int RingBuffer::GetEmptySize()
{
	return BUFFER_SIZE - GetUsedSize() - 1;
}

int RingBuffer::GetLinearUsedSize()
{
	if (tail_ == head_)
		return 0;
	if (head_ < tail_)
		return tail_ - head_;
	return BUFFER_SIZE - head_;
}

int RingBuffer::GetLinearEmptySize()
{
	if (tail_ >= head_)
		return BUFFER_SIZE - tail_ - 1;
	return head_ - tail_ - 1;
}

char* RingBuffer::GetHead()
{
	return &buffer_[head_];
}

char* RingBuffer::GetTail()
{
	return &buffer_[tail_];
}

void RingBuffer::moveHead(int bytes)
{
	head_ = (head_ + bytes) % BUFFER_SIZE;
}

void RingBuffer::moveTail(int bytes)
{
	tail_ = (tail_ + bytes) % BUFFER_SIZE;
}

void RingBuffer::Peek(char* dest, int len)
{
	if (GetUsedSize() < len) return;
	if (tail_ >= head_)
	{
		memcpy(dest, &buffer_[head_], len);
	} else
	{
		int rightSize = BUFFER_SIZE - head_;
		if (len <= rightSize)
		{
			memcpy(dest, &buffer_[head_], len);
		} else
		{
			memcpy(dest, &buffer_[head_], rightSize);
			memcpy(dest + rightSize, &buffer_[0], len - rightSize);
		}
	}
}

bool RingBuffer::Write(const char* data, int len)
{
	if (len > GetEmptySize()) return false;

	int rightSize = BUFFER_SIZE - tail_;
	if (len <= rightSize)
	{
		memcpy(&buffer_[tail_], data, len);
	} else
	{
		memcpy(&buffer_[tail_], data, rightSize);
		memcpy(&buffer_[0], data + rightSize, len - rightSize);
	}

	return true;
}

void RingBuffer::Clear()
{
	head_ = tail_ = 0;
}
