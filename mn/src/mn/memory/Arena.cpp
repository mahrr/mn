#include "mn/memory/Arena.h"
#include "mn/IO.h"
#include "mn/Assert.h"

namespace mn::memory
{
	Arena::Arena(size_t block_size, Interface* meta)
	{
		mn_assert(block_size != 0);
		this->meta = meta;
		this->head = nullptr;
		this->block_size = block_size;
		this->total_mem = 0;
		this->used_mem = 0;
		this->highwater_mem = 0;
		this->clear_all_readjust_threshold = 4ULL * 1024ULL * 1024ULL;
		this->clear_all_current_highwater = 0;
		this->clear_all_previous_highwater = 0;
	}

	Arena::~Arena()
	{
		free_all();
	}

	Block
	Arena::alloc(size_t size, uint8_t)
	{
		grow(size);

		uint8_t* ptr = this->head->alloc_head;
		this->head->alloc_head += size;
		this->used_mem += size;
		this->highwater_mem = this->highwater_mem > this->used_mem ? this->highwater_mem : this->used_mem;
		this->clear_all_current_highwater = this->clear_all_current_highwater > this->used_mem ? this->clear_all_current_highwater : this->used_mem;

		return Block{ ptr, size };
	}

	void
	Arena::free(Block)
	{
	}

	void
	Arena::grow(size_t size)
	{
		if (this->head != nullptr)
		{
			size_t node_used_mem = this->head->alloc_head - (uint8_t*)this->head->mem.ptr;
			size_t node_free_mem = this->head->mem.size - node_used_mem;
			if (node_free_mem >= size)
				return;
		}

		size_t request_size = size > this->block_size ? size : this->block_size;
		request_size += sizeof(Node);

		Node* new_node = (Node*)meta->alloc(request_size, alignof(int)).ptr;
		this->total_mem += request_size - sizeof(Node);

		new_node->mem.ptr = &new_node[1];
		new_node->mem.size = request_size - sizeof(Node);
		new_node->alloc_head = (uint8_t*)new_node->mem.ptr;
		new_node->next = this->head;
		this->head = new_node;
	}

	void
	Arena::free_all()
	{
		while (this->head)
		{
			Node* next = this->head->next;
			meta->free(Block{ this->head, this->head->mem.size + sizeof(Node) });
			this->head = next;
		}
		this->head = nullptr;
		this->total_mem = 0;
		this->used_mem = 0;
	}

	void
	Arena::clear_all()
	{
		size_t delta = 0;
		if (this->clear_all_current_highwater > this->clear_all_previous_highwater)
			delta = this->clear_all_current_highwater - this->clear_all_previous_highwater;
		else
			delta = this->clear_all_previous_highwater - this->clear_all_current_highwater;

		bool readjust = delta >= this->clear_all_readjust_threshold;

		if ((this->head && this->head->next != nullptr) || readjust)
		{
			this->free_all();
			this->grow(this->clear_all_current_highwater);
			this->clear_all_previous_highwater = this->clear_all_current_highwater;
			this->clear_all_current_highwater = 0;
		}
		else if (this->head && this->head->next == nullptr)
		{
			this->head->alloc_head = (uint8_t*)this->head->mem.ptr;
			this->used_mem = 0;
			this->clear_all_current_highwater = 0;
		}
	}

	bool
	Arena::owns(void* ptr) const
	{
		for (auto it = this->head; it != nullptr; it = it->next)
		{
			auto begin_ptr = (char*)it->mem.ptr;
			auto end_ptr = begin_ptr + it->mem.size;
			if (ptr >= begin_ptr && ptr < end_ptr)
				return true;
		}
		return false;
	}

	Arena::State
	Arena::checkpoint() const
	{
		State s{};
		s.head = this->head;
		if (this->head)
			s.alloc_head = this->head->alloc_head;
		s.total_mem = this->total_mem;
		s.used_mem = this->used_mem;
		s.highwater_mem = this->highwater_mem;
		return s;
	}

	void
	Arena::restore(State s)
	{
		while (this->head != s.head)
		{
			Node* next = this->head->next;
			meta->free(Block{ this->head, this->head->mem.size + sizeof(Node) });
			this->head = next;
		}
		mn_assert(this->head == s.head);
		this->head = s.head;
		if (this->head)
			this->head->alloc_head = s.alloc_head;
		this->total_mem = s.total_mem;
		this->used_mem = s.used_mem;
	}
}
